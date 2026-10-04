#include "memory/heap_collect.hpp"
#include "terms.hpp"
#include "terms/term_layout.hpp"
#include <array>
#include <erlang_aot/abi/bits.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Copying collection at an explicit host safe point: explicit roots are rewritten, every layout and
// internal sharing survives, garbage and dead off-heap binaries are reclaimed, the heap follows the
// ERTS size policy, and host Terms taken before a collection become stale.
namespace {
using namespace erlang_aot::runtime;
using detail::layout::BinaryBuffer;
using detail::layout::RefcBinaryCell;
using Status = erlang_aot::abi::v1::Status;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Admit a root word again after a collection rewrote it.
Term current(ProcessContext &context, Word word) { return Term::from_word(word, context).value(); }

// Render a value; text compares values across collections, since older Terms are stale.
std::string text(const Term &value) { return format_term(value, TermStyle::write).value(); }

// Observe the buffer of a large binary's cell without holding a reference to it.
std::weak_ptr<const BinaryBuffer> buffer(const Term &value) {
    return reinterpret_cast<const RefcBinaryCell *>(value.word() & ~Word{3})->buffer_;
}

// Extract a sub-binary through the generated bit service, as source matching does.
Term slice(ProcessContext &context, const Term &value, std::size_t offset, std::size_t count) {
    TermFactory factory(context);
    const std::array input{value.word(), factory.integer(static_cast<std::int64_t>(offset))->word(),
                           factory.integer(256 + 2)->word(), factory.integer(static_cast<std::int64_t>(count))->word(),
                           factory.integer(0)->word()};
    std::array<Word, 2> output{};
    GeneratedInvocation call(context.generated_calls());
    require(erlang_aot_bits_v1(&context, static_cast<std::uint8_t>(erlang_aot::abi::v1::BitOperation::extract),
                               input.data(), input.size(), output.data()) == 0,
            "slice failed");
    return Term::from_word(output[0], context).value();
}

// Words charged for an off-heap buffer of bytes.
std::size_t charge(std::size_t bytes) { return (bytes + sizeof(Word) - 1) / sizeof(Word); }

// Allocate unrooted tuples so the live graph is interleaved with garbage across many fragments.
void garbage(ProcessContext &context, std::size_t count) {
    TermFactory factory(context);
    for (std::size_t i = 0; i < count; ++i) {
        require(factory.tuple(std::array{factory.integer(static_cast<std::int64_t>(i)).value()}).has_value(),
                "garbage allocation failed");
    }
}

// Build one root per heap layout plus nested values that share a tuple and a list tail.
std::vector<Word> layouts(ProcessContext &context) {
    TermFactory f(context);
    garbage(context, 20);
    const auto shared = f.tuple(std::array{f.integer(7).value(), f.atom("shared").value()}).value();
    const auto big = f.integer_decimal("-123456789012345678901234567890").value();
    const auto real = f.floating(2.5).value();
    garbage(context, 20);
    const auto small = f.binary(std::vector(10, std::byte{1})).value();
    const auto large = f.binary(std::vector(100, std::byte{2})).value();
    const auto bits = f.bitstring(std::array{std::byte{0xff}, std::byte{0x80}}, 9).value();
    const auto tail = f.list(std::array{shared, big}).value();
    const auto first = f.cons(f.integer(1).value(), tail).value();
    const auto second = f.cons(real, tail).value();
    garbage(context, 20);
    const auto improper = f.list(std::array{small, bits}, large).value();
    const auto map = f.map(std::array{std::pair{big, first}, std::pair{real, improper}}).value();
    const auto pair = f.tuple(std::array{shared, shared, map, second}).value();
    garbage(context, 20);
    return {pair.word(), first.word(), second.word(), map.word(), big.word(), real.word(), large.word()};
}

// Every rewritten root still prints as before, and the nested aliases name one copy each.
void require_graph(ProcessContext &context, const std::vector<Word> &roots, const std::vector<std::string> &texts) {
    for (std::size_t i = 0; i < roots.size(); ++i) {
        require(text(current(context, roots[i])) == texts[i], "rooted value changed");
    }
    const auto pair = current(context, roots[0]);
    const auto first = current(context, roots[1]);
    const auto second = current(context, roots[2]);
    require(pair.tuple_element(0)->word() == pair.tuple_element(1)->word(), "shared tuple was duplicated");
    require(first.tail()->word() == second.tail()->word(), "shared list tail was duplicated");
    require(pair.tuple_element(3)->word() == roots[2], "root and nested alias diverged");
    require(first.tail()->head()->word() == pair.tuple_element(0)->word(), "shared tuple alias diverged");
    const auto found = current(context, roots[3]).map_find(current(context, roots[4])).value();
    require(found && found->word() == roots[1], "map value lost its identity");
    const auto census = context.heap().verify().value();
    require(census.filler_words == 0 && census.words == context.heap().used_words(), "new heap holds garbage");
}

// Repeated collections keep the rooted graph and drop garbage; fragments merge into the heap block.
void repeated(Runtime &runtime) {
    auto &context = *runtime.create_context({16, std::size_t{1} << 20}).value();
    auto roots = layouts(context);
    std::vector<std::string> texts;
    for (const auto word : roots) {
        texts.push_back(text(current(context, word)));
    }
    std::size_t live = 0;
    for (int round = 0; round < 3; ++round) {
        const auto before = context.heap().used_words();
        const auto host = current(context, roots[0]);
        const auto stats = context.heap().collect(roots).value();
        require(stats.words_before == before && stats.live_words == context.heap().used_words(), "wrong stats");
        require(stats.heap_words == context.heap().capacity_words(), "fragments survived the collection");
        require(round != 0 || (stats.fragment_words > 0 && stats.live_words < before), "garbage survived");
        require(round == 0 || stats.live_words == live, "live size changed without allocation");
        require(host.tuple_size() == std::unexpected(TermError::stale_term), "pre-collection term stayed valid");
        live = stats.live_words;
        require_graph(context, roots, texts);
        garbage(context, 50);
    }
    require(runtime.destroy_context(&context) == Status::ok, "teardown failed");
}

// Dead binary cells release their buffer reference once; a buffer shared by a live tail stays.
void binaries(Runtime &runtime) {
    auto &context = *runtime.create_context().value();
    TermFactory factory(context);
    const auto kept = factory.binary(std::vector(100, std::byte{3})).value();
    const auto dropped = factory.binary(std::vector(200, std::byte{4})).value();
    const auto tail = slice(context, kept, 8, 700);
    const std::weak_ptr<const BinaryBuffer> kept_buffer = buffer(kept);
    const std::weak_ptr<const BinaryBuffer> dropped_buffer = buffer(dropped);
    require(kept_buffer.use_count() == 2 && context.heap().off_heap_words() == charge(100) + charge(200),
            "binary fixture wrong");
    std::array roots{tail.word()};
    const auto stats = context.heap().collect(roots).value();
    require(dropped_buffer.expired() && kept_buffer.use_count() == 1, "dead binary cells kept references");
    require(stats.off_heap_words == charge(100) && context.heap().off_heap_words() == charge(100),
            "released buffer kept its charge");
    require(context.heap().verify()->off_heap_cells == 1, "off-heap list not rebuilt");
    require(current(context, roots[0]).bit_size() == 700, "rooted tail lost its view");
    require(context.heap().collect().has_value() && kept_buffer.expired(), "last reference not released");
    require(context.heap().off_heap_words() == 0 && context.heap().verify()->off_heap_cells == 0, "list kept cells");
    require(runtime.destroy_context(&context) == Status::ok, "teardown failed");
}

// Build a proper list of count small integers, 2 words per element.
Term numbers(ProcessContext &context, std::size_t count) {
    TermFactory factory(context);
    std::vector<Term> elements;
    for (std::size_t i = 0; i < count; ++i) {
        elements.push_back(factory.integer(static_cast<std::int64_t>(i)).value());
    }
    return factory.list(elements).value();
}

// The new block keeps live data below 75% along the ERTS sizes; a block under 25% live shrinks.
void policy(Runtime &runtime) {
    require(detail::heap_size_at_least(1) == 12 && detail::heap_size_at_least(233) == 233 &&
                detail::heap_size_at_least(234) == 376 && detail::heap_size_at_least(833'027) == 999'631,
            "heap size sequence differs from ERTS");
    auto &context = *runtime.create_context().value();
    std::array roots{numbers(context, 2000).word()};
    const auto grown = context.heap().collect(roots).value();
    require(grown.live_words == 4000 && grown.heap_words == 6772, "heap did not grow per policy");
    roots[0] = current(context, roots[0]).tail().value().word();
    for (int i = 0; i < 1990; ++i) {
        roots[0] = current(context, roots[0]).tail().value().word();
    }
    const auto shrunk = context.heap().collect(roots).value();
    require(shrunk.live_words == 18 && shrunk.heap_words == HeapOptions{}.min_heap_words, "heap did not shrink");
    require(current(context, roots[0]).list_length() == 9, "shrinking lost the live tail");
    require(context.heap().verify().has_value(), "shrunk heap does not verify");
    require(runtime.destroy_context(&context) == Status::ok, "teardown failed");
}

// Generated code running or an open reservation is not a safe point; nothing changes.
void unsafe(Runtime &runtime) {
    auto &context = *runtime.create_context().value();
    const auto value = numbers(context, 3);
    std::array roots{value.word()};
    const auto used = context.heap().used_words();
    {
        GeneratedInvocation invocation(context.generated_calls());
        require(context.heap().collect(roots) == std::unexpected(HeapError::unsafe_point), "collected in a call");
        require(!context.generated_calls().failure(), "refused collection failed the generated call");
    }
    {
        auto open = context.heap().reserve(1).value();
        require(context.heap().collect(roots) == std::unexpected(HeapError::unsafe_point), "collected a reservation");
    }
    require(roots[0] == value.word() && value.list_length() == 3 && context.heap().used_words() == used,
            "refused collection changed the heap");
    require(runtime.destroy_context(&context) == Status::ok, "teardown failed");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        repeated(*runtime);
        binaries(*runtime);
        policy(*runtime);
        unsafe(*runtime);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
