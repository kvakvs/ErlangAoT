#include <cstddef>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/builtin_registry.hpp>
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

// Builtins that run in portions (docs/builtins.md#portions) as a process's first call: each portion ends in a yield,
// a collection runs between every two portions and moves every heap term, and the results stay exact.
namespace {
using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::empty_list;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Elements per list: more than two portions of a full time slice each.
constexpr std::size_t COUNT = 2 * SLICE_REDUCTIONS * WORK_PER_REDUCTION;

// The outcome of a builtin run: its result and the portions it took.
struct Run {
    Word result = 0;
    std::size_t portions = 0;
};

// Run erlang:Name on `arguments` in `context` slice by slice, collecting the heap between slices.
Run portions(ProcessContext &context, std::string_view name, std::span<const Word> arguments) {
    const auto *builtin = context.code_server().builtins().find("erlang", name, arguments.size());
    require(builtin != nullptr, "builtin not registered");
    GeneratedInvocation scope(context.generated_calls());
    std::ranges::copy(arguments, context.stack().registers());
    require(context.stack().start(builtin->frame), "start failed");
    Run run;
    for (run.portions = 1; !context.stack().run(SLICE_REDUCTIONS); ++run.portions) {
        const SafePoint safe(context.generated_calls());
        require(context.heap().collect().has_value(), "collection between portions failed");
    }
    require(!context.generated_calls().failure().has_value(), "builtin failed");
    run.result = context.stack().registers()[0];
    return run;
}

// The list [{First}, {First + Step}, ...] of `count` one-element tuples, which collections move.
Word tuples(ProcessContext &context, std::int64_t first, std::int64_t step, std::size_t count) {
    TermFactory factory(context);
    std::vector<Term> elements;
    elements.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto number = factory.integer(first + step * static_cast<std::int64_t>(i)).value();
        elements.push_back(factory.tuple(std::array{number}).value());
    }
    return factory.list(elements).value().word();
}

// The integers inside the one-element tuples of a result list, in order.
std::vector<std::int64_t> numbers(ProcessContext &context, Word list) {
    std::vector<std::int64_t> result;
    for (auto cell = Term::from_word(list, context).value(); cell.is_cons(); cell = cell.tail().value()) {
        result.push_back(cell.head().value().tuple_element(0).value().integer_value().value());
    }
    return result;
}

// Whether `values` are First, First + Step, ... up to `count` of them.
bool sequence(const std::vector<std::int64_t> &values, std::int64_t first, std::int64_t step, std::size_t count) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] != first + step * static_cast<std::int64_t>(i)) {
            return false;
        }
    }
    return values.size() == count;
}

// Left ++ Right copies Left onto Right; length/1 counts it.
void append_and_length(ProcessContext &context) {
    const std::array arguments{tuples(context, 1, 1, COUNT), tuples(context, COUNT + 1, 1, 1)};
    const auto appended = portions(context, "++", arguments);
    require(appended.portions > 2, "++ ran in too few portions");
    require(sequence(numbers(context, appended.result), 1, 1, COUNT + 1), "++ result differs");
    const auto counted = portions(context, "length", std::array{appended.result});
    require(counted.portions > 2 && counted.result == encode_integer(COUNT + 1).value(), "length/1 differs");
}

// Left -- Right removes the even numbers, given in descending order: sorting them takes more than one portion of
// comparisons, and so does scanning Left with a binary search per element.
void subtract(ProcessContext &context) {
    constexpr std::size_t size = COUNT / 8;
    const std::array arguments{tuples(context, 1, 1, size), tuples(context, size, -2, size / 2)};
    const auto subtracted = portions(context, "--", arguments);
    require(subtracted.portions > 4, "-- ran in too few portions");
    require(sequence(numbers(context, subtracted.result), 1, 2, size / 2), "-- result differs");
}

// binary_to_list/1 and list_to_binary/1 round-trip a binary through nested iolists.
void binaries(ProcessContext &context) {
    TermFactory factory(context);
    std::vector<std::byte> bytes(COUNT);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::byte>(i % 251);
    }
    const auto binary = factory.binary(bytes).value();
    const auto listed = portions(context, "binary_to_list", std::array{binary.word()});
    require(listed.portions > 2, "binary_to_list/1 ran in too few portions");
    const auto list = Term::from_word(listed.result, context).value();
    const auto nested = factory.list(std::array{list, factory.list(std::array{list}).value()}).value();
    const auto joined = portions(context, "list_to_binary", std::array{nested.word()});
    require(joined.portions > 2, "list_to_binary/1 ran in too few portions");
    auto twice = bytes;
    twice.insert(twice.end(), bytes.begin(), bytes.end());
    require(Term::from_word(joined.result, context).value().binary_bytes().value() == twice,
            "list_to_binary/1 result differs");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        auto &context = *runtime->create_context().value();
        append_and_length(context);
        subtract(context);
        binaries(context);
        require(context.heap().verify().has_value() && context.stack().trap_state<TrapState>() == nullptr,
                "state left behind");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
