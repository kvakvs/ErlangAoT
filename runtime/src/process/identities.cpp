#include "identities.hpp"
#include <algorithm>
#include <atomic>
#include <iterator>
#include <mutex>
#include <new>

namespace erlang_aot::runtime::detail {
namespace {
// Low four bits of an immediate pid (TermKind2::pid under the see_termkind2 primary tag).
constexpr Word PID_TAG = 0x3;
constexpr unsigned PID_TAG_BITS = 4;
// The largest number a pid payload holds on this target.
constexpr Word MAX_PID_NUMBER = ~Word{0} >> PID_TAG_BITS;

// Never reuse a pid number, even across runtimes destroyed earlier in this program.
TermResult<Word> reserve_number() noexcept {
    static std::atomic<Word> next{1};
    auto candidate = next.load(std::memory_order_relaxed);
    do {
        if (candidate > MAX_PID_NUMBER) {
            return std::unexpected(TermError::resource_limit);
        }
    } while (!next.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed));
    return candidate;
}
} // namespace

TermResult<Word> ProcessNumbers::issue() {
    // Reserve under the lock so this runtime's runs stay ascending when several threads create processes.
    const std::unique_lock lock(mutex_);
    const auto number = reserve_number();
    if (!number) {
        return number;
    }
    if (!runs_.empty() && runs_.back().second == *number) {
        ++runs_.back().second;
        return number;
    }
    try {
        runs_.emplace_back(*number, *number + 1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    }
    return number;
}

bool ProcessNumbers::issued(Word number) const noexcept {
    const std::shared_lock lock(mutex_);
    const auto run = std::ranges::upper_bound(runs_, number, {}, &std::pair<Word, Word>::first);
    return run != runs_.begin() && number < std::prev(run)->second;
}

Word pid_word(Word number) noexcept { return (number << PID_TAG_BITS) | PID_TAG; }

Word pid_number(Word word) noexcept { return word >> PID_TAG_BITS; }
} // namespace erlang_aot::runtime::detail
