#include "terms.hpp"
#include <array>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>

// Header-based admission: only words that point at a published object start of the right shape in
// this process are admitted. Interior, retagged, misaligned, stale and foreign words are rejected.
namespace {
using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// A word at offset bytes past value's own word must not be admitted by context.
void rejects(ProcessContext &context, Word word, const char *message) {
    require(Term::from_word(word, context) == std::unexpected(TermError::wrong_owner), message);
}

// Interior words of every compound layout, including untraced payload, are never object starts.
void interior(ProcessContext &context) {
    TermFactory factory(context);
    const auto one = factory.integer(1).value();
    const auto tuple = factory.tuple(std::array{one, one, one}).value();
    const auto map = factory.map(std::array{std::pair{one, one}, std::pair{factory.atom("a").value(), one}}).value();
    const auto bignum = factory.integer_decimal(std::string(60, '7')).value();
    const auto inline_binary = factory.binary(std::vector(40, std::byte{9})).value();
    const auto shared_binary = factory.binary(std::vector(200, std::byte{9})).value();
    for (const auto &value : {tuple, map, bignum, inline_binary, shared_binary}) {
        require(Term::from_word(value.word(), context).has_value(), "object start rejected");
        for (Word offset = 1; offset <= 3; ++offset) {
            rejects(context, value.word() + offset * sizeof(Word), "interior word admitted");
        }
    }
    rejects(context, tuple.word() + sizeof(Word) / 2, "misaligned word admitted");
}

// A list tag on a boxed object or a boxed tag on a cons cell is rejected; later cons cells are starts.
void retagged(ProcessContext &context) {
    TermFactory factory(context);
    const auto one = factory.integer(1).value();
    const auto list = factory.list(std::array{one, one, one}).value();
    const auto tuple = factory.tuple(std::array{one}).value();
    constexpr Word tags = 3;
    rejects(context, (tuple.word() & ~tags) | 1U, "list-tagged tuple admitted");
    rejects(context, (list.word() & ~tags) | 2U, "boxed-tagged cons admitted");
    rejects(context, list.word() + sizeof(Word), "cons tail word admitted");
    const auto second = Term::from_word(list.word() + 2 * sizeof(Word), context);
    require(second && second->list_length() == 2, "later cons cell rejected");
}

// Words past the used area and words of a rolled-back reservation are never admitted.
void stale(ProcessContext &context) {
    Word abandoned = 0;
    {
        auto reservation = context.heap().reserve(2).value();
        abandoned = reinterpret_cast<Word>(reservation.bytes().data()) | 2U;
    }
    rejects(context, abandoned, "rolled-back word admitted");
    const auto tuple = TermFactory(context).tuple(std::array{Term::from_word(0x3b).value()}).value();
    rejects(context, tuple.word() + 64 * sizeof(Word), "word past used area admitted");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        auto &context = *runtime->create_context().value();
        auto &other = *runtime->create_context().value();
        interior(context);
        retagged(context);
        stale(context);
        const auto foreign = TermFactory(other).tuple(std::array{Term::from_word(0x3b).value()}).value();
        rejects(context, foreign.word(), "foreign word admitted");
        require(context.heap().verify().has_value() && other.heap().verify().has_value(), "heaps unparseable");
        require(runtime->destroy_context(&other) == Status::ok, "teardown failed");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
