#include <algorithm>
#include <atomic>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Runtime services shared by scheduler workers (docs/runtime.md#threads), stressed from several host threads: the
// atom table interns overlapping spellings concurrently and keeps one stable word per spelling.
namespace {
using namespace erlang_aot::runtime;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Threads racing on every spelling; enough to interleave inserts and lookups, few enough for a Debug run.
constexpr std::size_t THREADS = 8;
constexpr std::size_t SPELLINGS = 2'000;

// Run `work(index)` on THREADS threads at once and join them; false when any thread threw.
template <typename Work> bool parallel(Work work) {
    std::atomic<bool> failed{false};
    std::atomic<std::size_t> ready{0};
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < THREADS; ++index) {
        threads.emplace_back([&, index] {
            // Start together so the threads contend from the first operation.
            ++ready;
            while (ready < THREADS) {
                std::this_thread::yield();
            }
            try {
                work(index);
            } catch (...) {
                failed = true;
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    return !failed;
}

// Every thread interns all spellings, each starting at a different offset, and reads them back by word: each
// spelling gets one word, whichever thread created it, and the table holds each spelling once.
void atoms(Runtime &runtime) {
    auto &storage = *runtime.atom_storage();
    const auto before = storage.size();
    std::vector<std::vector<Word>> words(THREADS, std::vector<Word>(SPELLINGS));
    const bool joined = parallel([&](std::size_t thread) {
        for (std::size_t step = 0; step < SPELLINGS; ++step) {
            const auto index = (step + thread * SPELLINGS / THREADS) % SPELLINGS;
            const auto atom = storage.intern("atom_" + std::to_string(index)).value();
            require(storage.lookup(atom.word()).value().atom_spelling().value() == atom.atom_spelling().value(),
                    "lookup by word found another spelling");
            words[thread][index] = atom.word();
        }
    });
    require(joined, "a thread failed to intern or look up an atom");
    require(std::ranges::all_of(words, [&](const auto &seen) { return seen == words[0]; }),
            "threads got different words for one spelling");
    require(storage.size() == before + SPELLINGS, "a spelling was stored twice");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        atoms(*runtime);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
