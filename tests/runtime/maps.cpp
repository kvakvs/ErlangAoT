#include "terms.hpp"
#include <array>
#include <erlang_aot/abi/maps.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>

namespace {
using namespace erlang_aot::runtime;
using Op = erlang_aot::abi::v1::MapOperation;
using Outcome = erlang_aot::abi::v1::MapOutcome;
using Status = erlang_aot::abi::v1::Status;

// Keep ownership/publication assertions active in optimized native builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Inspect the public service's semantic outcome without exposing private map storage.
std::pair<Outcome, Term> invoke(ProcessContext &context, Op operation, std::span<const Term> input) {
    std::vector<Word> words;
    for (const auto &value : input) {
        words.push_back(value.word());
    }
    GeneratedInvocation scope(context.generated_calls());
    Word output = 0;
    const auto result = static_cast<Outcome>(
        erlang_aot_map_v1(&context, static_cast<std::uint8_t>(operation), words.data(), words.size(), &output));
    require(!context.generated_calls().failure(), "unexpected map infrastructure failure");
    return {result, Term::from_word(output, context).value()};
}

// Immutable tables normalize duplicate exact keys, preserve numeric key distinctions and roll back failed updates.
Term construction(ProcessContext &context) {
    TermFactory factory(context);
    const auto one = factory.integer(1).value();
    const auto two = factory.integer(2).value();
    const auto real = factory.floating(1.0).value();
    const auto key = factory.atom("key").value();
    const auto map =
        factory.map(std::array{std::pair{key, one}, std::pair{real, two}, std::pair{one, one}, std::pair{key, two}})
            .value();
    require(map.map_size() == 3, "map duplicate/type identity incorrect");
    require(map.map_find(key)->value().integer_value() == 2, "last association did not win");
    require(map.map_find(real)->value().integer_value() == 2 && map.map_find(one)->value().integer_value() == 1,
            "integer/float keys coalesced");
    const auto reversed =
        factory.map(std::array{std::pair{one, one}, std::pair{real, two}, std::pair{key, two}}).value();
    require(map.word() != reversed.word() && map.exactly_equal(reversed).value(),
            "map equality depended on allocation");
    const auto missing = factory.atom("missing").value();
    const auto used = context.heap().used_words();
    const auto failed = invoke(context, Op::update, std::array{map, missing, one, one});
    require(failed.first == Outcome::bad_key && failed.second.exactly_equal(missing).value(), "exact update lost key");
    require(context.heap().used_words() == used && map.map_size() == 3, "failed update published partial storage");
    const auto updated = invoke(context, Op::update, std::array{map, key, one, one});
    require(updated.first == Outcome::success && updated.second.map_find(key)->value().integer_value() == 1,
            "exact update recovery failed");
    require(map.map_find(key)->value().integer_value() == 2, "update mutated old map");
    return map;
}

// Parent pins keep extracted values and compound keys stable through growth, then reject expired access.
void ownership() {
    auto runtime = Runtime::start().value();
    auto &context = *runtime->create_context().value();
    TermFactory factory(context);
    const auto map = construction(context);
    const auto nested = factory.map(std::array{std::pair{map, map}}).value();
    const auto child = nested.map_find(map)->value();
    auto &foreign = *runtime->create_context().value();
    require(Term::from_word(map.word(), foreign) == std::unexpected(TermError::wrong_owner), "foreign map admitted");
    const auto foreign_key = TermFactory(foreign).map({}).value();
    require(nested.map_find(foreign_key) == std::unexpected(TermError::wrong_owner), "foreign key admitted");
    for (unsigned i = 0; i < 100; ++i) {
        require(factory.map(std::array{std::pair{map, nested}}).has_value(), "growth failed");
    }
    require(child.exactly_equal(map).value(), "later allocation damaged extracted map");
    require(runtime->destroy_context(&context) == Status::ok, "map owner teardown failed");
    require(map.map_size() == std::unexpected(TermError::expired_context), "expired map accessed");
    require(child.map_entries() == std::unexpected(TermError::expired_context), "expired child accessed");
    require(factory.map({}) == std::unexpected(TermError::expired_context), "expired factory allocated map");
}

// Invalid service arrays leave output untouched; a fresh invocation can construct an empty map.
void malformed() {
    auto runtime = Runtime::start().value();
    auto &context = *runtime->create_context().value();
    const auto one = TermFactory(context).integer(1)->word();
    Word output = 123;
    {
        GeneratedInvocation scope(context.generated_calls());
        require(erlang_aot_map_v1(&context, static_cast<std::uint8_t>(Op::make), &one, 1, &output) ==
                    static_cast<std::uint8_t>(Outcome::failure),
                "odd map array accepted");
        require(output == 123 && context.generated_calls().failure().has_value(), "invalid map array published output");
    }
    const auto value = invoke(context, Op::make, {});
    require(value.first == Outcome::success && value.second.map_size() == 0, "map service retry failed");
}

// Descending keys exhaust charged insertion work before any backing is published, then ordinary work recovers.
void work_limit() {
    auto runtime = Runtime::start().value();
    auto &context = *runtime->create_context().value();
    TermFactory factory(context);
    std::vector<std::pair<Term, Term>> entries;
    for (int i = 1500; i > 0; --i) {
        const auto value = factory.integer(i).value();
        entries.emplace_back(value, value);
    }
    require(factory.map(entries) == std::unexpected(TermError::resource_limit), "unbounded map insertion work");
    require(context.heap().used_words() == 0, "work rejection published a partial map");
    require(factory.map({})->map_size() == 0, "work rejection poisoned retry");
}
} // namespace

// Source differential tests own semantic coverage; this native consumer owns inaccessible host invariants.
int main() {
    try {
        ownership();
        malformed();
        work_limit();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
