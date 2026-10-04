#include <algorithm>
#include <array>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

namespace {
// These lifecycle checks remain active in optimized native consumers.
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

// Nested results transfer before pop, and a host scope preserves an enclosing generated caller's roots.
void transfers(ProcessContext &context) {
    GeneratedInvocation invocation(context.generated_calls());
    auto &roots = context.roots();
    auto *outer = roots.enter(2);
    require(outer && outer[0] == 0 && outer[1] == 0, "roots were not zero initialized");
    outer[0] = encode_integer(11).value();
    const auto result = encode_integer(22).value();
    {
        RootInvocation nested(roots);
        auto *inner = roots.enter(3);
        require(inner && roots.words() == 5 && roots.depth() == 2, "nested accounting failed");
        require(roots.leave(inner, result) == Status::ok && roots.contains(result), "result lost during pop");
        require(roots.contains(outer[0]), "callee changed caller root");
        require(roots.enter(1) != nullptr, "exception fixture entry failed");
    }
    require(roots.depth() == 1 && roots.words() == 2, "nested exception cleanup removed caller roots");
    require(roots.leave(outer, result) == Status::ok && roots.contains(result), "host handoff lost result");
    roots.restore(0);
    require(roots.words() == 0 && roots.depth() == 0 && !roots.contains(result), "host cleanup retained roots");
}

// Capacity and order faults remain infrastructure failures and never discard older roots or first failures.
void limits(ProcessContext &context) {
    for (const auto options : {RootOptions{2, 2}, RootOptions{20, 1}}) {
        GeneratedInvocation invocation(context.generated_calls());
        GeneratedRoots roots(context, options);
        auto *first = roots.enter(2);
        require(first && !roots.enter(1), "root capacity ignored");
        require(context.generated_calls().failure()->status == Status::resource_limit, "root failure lost status");
        require(roots.words() == 2 && roots.depth() == 1, "failed entry published partial roots");
        require(roots.leave(first, 0) == Status::ok, "error cleanup failed");
    }
    GeneratedInvocation invocation(context.generated_calls());
    auto &roots = context.roots();
    auto *first = roots.enter(1);
    Word forged = 0;
    require(roots.leave(&forged, 0) == Status::internal_error && roots.depth() == 1, "out-of-order release accepted");
    require(roots.leave(first, 0) == Status::ok, "first-failure root cleanup failed");
    roots.restore(0);
}

// Frames crossing small stack segments keep their addresses and slots, all slots are enumerated, the word
// bound counts every segment, and segments are freed once their frames return.
void segments(ProcessContext &context) {
    GeneratedInvocation invocation(context.generated_calls());
    GeneratedRoots roots(context, RootOptions{.words = 24, .frames = 16, .segment_bytes = 64});
    std::vector<std::pair<Word *, std::size_t>> frames;
    for (const std::size_t count : {3, 3, 5, 2, 7}) {
        auto *slots = roots.enter(count);
        require(slots && std::ranges::all_of(std::span(slots, count), [](Word word) { return word == 0; }),
                "segment window not zeroed");
        std::ranges::fill(std::span(slots, count), encode_integer(static_cast<std::int64_t>(frames.size())).value());
        frames.emplace_back(slots, count);
    }
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const auto expected = encode_integer(static_cast<std::int64_t>(index)).value();
        require(std::ranges::all_of(std::span(frames[index].first, frames[index].second),
                                    [&](Word word) { return word == expected; }),
                "frame moved or was overwritten by a later frame");
    }
    std::size_t visited = 0;
    roots.visit([&](Word &) { ++visited; });
    require(visited == 20 && roots.words() == 20 && roots.capacity() >= 20, "stack enumeration missed slots");
    require(roots.enter(4) && !roots.enter(1), "word bound ignored across segments");
    require(context.generated_calls().failure()->status == Status::resource_limit, "stack limit status lost");
    roots.restore(0);
    require(roots.depth() == 0 && roots.words() == 0 && roots.capacity() == 0, "empty segments not freed");
}

// A default segment fills one 4 KiB page together with its three-word header and a two-pointer allocator
// header; a larger frame takes whole pages.
void page_segments(ProcessContext &context) {
    GeneratedInvocation invocation(context.generated_calls());
    RootInvocation scope(context.roots());
    auto &roots = context.roots();
    const auto headers = 5 * sizeof(void *);
    const auto bytes = [&] { return roots.capacity() * sizeof(Word); };
    require(roots.enter(1) && bytes() + headers <= 4096 && bytes() + headers + sizeof(Word) > 4096,
            "segment does not fill one page");
    const auto page_words = roots.capacity();
    require(roots.enter(page_words + 1) && bytes() + (2 * headers) <= 3 * 4096 &&
                bytes() + (2 * headers) + (2 * sizeof(Word)) > 3 * 4096,
            "large frame segment not rounded to whole pages");
}

// The default bounds stay 1,000,000 live words and 4,096 frames.
void default_limits(ProcessContext &context) {
    {
        GeneratedInvocation invocation(context.generated_calls());
        RootInvocation scope(context.roots());
        require(context.roots().enter(1'000'000) && !context.roots().enter(1), "default word bound changed");
    }
    GeneratedInvocation invocation(context.generated_calls());
    RootInvocation scope(context.roots());
    for (std::size_t frame = 0; frame < 4096; ++frame) {
        require(context.roots().enter(1) != nullptr, "default frame bound too small");
    }
    require(!context.roots().enter(1) && context.roots().depth() == 4096, "default frame bound changed");
}

// A failed call leaves frames behind; the host scope restores them and the next call starts a fresh stack.
void failed_call(ProcessContext &context) {
    auto &roots = context.roots();
    {
        GeneratedInvocation invocation(context.generated_calls());
        RootInvocation scope(roots);
        auto *first = roots.enter(2);
        require(first && roots.enter(300), "frames before failure not entered");
        first[0] = encode_integer(1).value();
        context.generated_calls().fail_service(Status::internal_error);
        require(!roots.enter(1), "entry admitted after failure");
    }
    require(roots.depth() == 0 && roots.words() == 0 && roots.capacity() == 0, "failed call not restored");
    GeneratedInvocation invocation(context.generated_calls());
    RootInvocation scope(roots);
    auto *again = roots.enter(2);
    require(again && again[0] == 0 && roots.depth() == 1, "stack unusable after restore");
}

// Null/unscoped calls reject before allocating or reading a result representation.
void boundaries(ProcessContext &context) {
    require(!context.roots().enter(1), "unscoped roots admitted");
    require(!erlang_aot_roots_enter_v4(nullptr, 1), "null context entered roots");
    require(erlang_aot_roots_leave_v4(nullptr, nullptr, 0) == static_cast<std::uint8_t>(Status::invalid_argument),
            "null leave admitted");
    GeneratedInvocation invocation(context.generated_calls());
    require(!context.roots().enter(std::numeric_limits<std::size_t>::max()), "overflow root count admitted");
    require(context.generated_calls().failure()->status == Status::resource_limit, "overflow status lost");
}

// Stack slots, result handoffs, the error payload and explicit roots form the whole root set. Rewriting
// the payload word keeps its Term current; a Term outliving its context reports expired_context.
void root_set(Runtime &runtime) {
    auto &context = *runtime.create_context().value();
    TermFactory factory(context);
    const auto box = [&](std::int64_t value) {
        return factory.tuple(std::array{factory.integer(value).value()}).value();
    };
    const auto slot = box(1);
    const auto handoff = box(2);
    const auto payload = box(3);
    const auto replacement = box(4);
    std::array explicit_roots{box(5).word()};
    {
        GeneratedInvocation invocation(context.generated_calls());
        auto &roots = context.roots();
        roots.enter(1)[0] = slot.word();
        require(roots.leave(roots.enter(1), handoff.word()) == Status::ok, "handoff failed");
        context.generated_calls().fail({.code = CallError::erlang_exception,
                                        .reason = erlang_aot::abi::v1::ErrorReason::badmatch,
                                        .value = payload});
        std::vector<Word> seen;
        context.visit_roots(explicit_roots, [&](Word &word) { seen.push_back(word); });
        for (const auto &expected : {slot.word(), handoff.word(), payload.word(), explicit_roots[0]}) {
            require(std::ranges::contains(seen, expected), "root missing from enumeration");
        }
        context.visit_roots({}, [&](Word &word) { word = word == payload.word() ? replacement.word() : word; });
        const auto &moved = context.generated_calls().failure()->value;
        require(moved->word() == replacement.word() && moved->tuple_element(0)->integer_value() == 4,
                "rewritten payload not rebound");
        roots.restore(0);
    }
    require(runtime.destroy_context(&context) == Status::ok, "teardown failed");
    require(slot.tuple_size() == std::unexpected(TermError::expired_context), "term outlived its context");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        auto &context = *runtime->create_context().value();
        boundaries(context);
        transfers(context);
        limits(context);
        transfers(context);
        segments(context);
        page_segments(context);
        default_limits(context);
        failed_call(context);
        root_set(*runtime);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
