// Step-17 decision prototype, rejected baseline: today's model. Erlang calls are native calls, live values sit
// in explicit root frames off the native stack, and every call is followed by a failure-channel check.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
using Word = std::uintptr_t;
constexpr Word BOOM = 0xB00;

struct Context {
    // Root slots of every live frame; reserved up front so slot addresses stay stable (the 8F property).
    std::vector<Word> roots;
    bool failed = false;
    Word reason = 0;
    std::uintptr_t deepest = UINTPTR_MAX;
};

// Push and pop one root frame of `count` slots, as clause_roots_enter/leave do.
Word *enter(Context &context, std::size_t count) {
    context.roots.resize(context.roots.size() + count);
    return context.roots.data() + context.roots.size() - count;
}

void leave(Context &context, std::size_t count) { context.roots.resize(context.roots.size() - count); }

void probe(Context &context) {
    volatile char marker = 0;
    context.deepest = std::min(context.deepest, reinterpret_cast<std::uintptr_t>(&marker));
}

// sum(0) -> 0; sum(N) -> N + sum(N - 1).
Word sum(Context &context, Word n) {
    Word *roots = enter(context, 1);
    roots[0] = n;
    if (n == 0) {
        probe(context);
        leave(context, 1);
        return 0;
    }
    Word result = sum(context, n - 1);
    if (context.failed) {
        leave(context, 1);
        return 0;
    }
    result += roots[0];
    leave(context, 1);
    return result;
}

// loop(0, Acc) -> Acc; loop(N, Acc) -> loop(N - 1, Acc + 1).   Only an optimizer sibling call removes the frame.
Word loop(Context &context, Word n, Word acc) {
    if (n == 0) {
        probe(context);
        return acc;
    }
    return loop(context, n - 1, acc + 1);
}

// fail(0) -> error(boom); fail(N) -> 1 + fail(N - 1).
Word fail(Context &context, Word n) {
    if (n == 0) {
        probe(context);
        context.failed = true;
        context.reason = BOOM;
        return 0;
    }
    Word result = fail(context, n - 1);
    return context.failed ? 0 : result + 1;
}

// catcher(N) -> try fail(N) catch error:R -> R end.
Word catcher(Context &context, Word n) {
    Word result = fail(context, n);
    if (context.failed) {
        context.failed = false;
        return context.reason;
    }
    return result;
}

// Run one scenario and report the native stack bytes used per recursion level.
template <typename Body> bool scenario(const char *name, Word depth, Word expected, Body body) {
    Context context;
    context.roots.reserve(1'000'000);
    volatile char marker = 0;
    auto base = reinterpret_cast<std::uintptr_t>(&marker);
    auto started = std::chrono::steady_clock::now();
    Word value = body(context);
    auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    long long native = context.deepest == UINTPTR_MAX ? 0 : static_cast<long long>(base - context.deepest);
    bool ok = value == expected;
    std::printf("%-10s %s value=%llu depth=%llu native_bytes=%lld per_level=%.1f ms=%.2f\n", name, ok ? "ok  " : "FAIL",
                static_cast<unsigned long long>(value), static_cast<unsigned long long>(depth), native,
                static_cast<double>(native) / static_cast<double>(depth), elapsed);
    return ok;
}
} // namespace

int main() {
    // Depths stay small: the native stack (1 MiB on Windows) bounds them, and nothing can suspend a process.
    constexpr Word DEPTH = 10'000;
    std::printf("model: native calls with explicit root frames, word %zu bits, yield unsupported\n", sizeof(Word) * 8);
    bool ok = scenario("recursion", DEPTH, DEPTH * (DEPTH + 1) / 2, [&](Context &c) { return sum(c, DEPTH); });
    ok = scenario("tail", DEPTH, DEPTH, [&](Context &c) { return loop(c, DEPTH, 0); }) && ok;
    ok = scenario("caught", DEPTH, BOOM, [&](Context &c) { return catcher(c, DEPTH); }) && ok;
    return ok ? 0 : 1;
}
