#include "timer_wheel.hpp"
#include <algorithm>
#include <bit>
#include <utility>

namespace clause::runtime::detail {
namespace {
// The occupied slots of `mask` after slot `current`.
std::uint64_t after(const std::uint64_t mask, const std::size_t current) noexcept {
    return current + 1 >= TimerWheel::SLOTS ? 0 : mask & (~std::uint64_t{0} << (current + 1));
}

// The number of ticks one slot of `level` spans, as a shift.
unsigned shift(const std::size_t level) noexcept { return static_cast<unsigned>(level) * TimerWheel::LEVEL_BITS; }
} // namespace

std::pair<std::size_t, std::size_t> TimerWheel::position(const std::uint64_t tick) const noexcept {
    const auto difference = tick ^ now_;
    const auto level = difference == 0
                           ? std::size_t{0}
                           : std::min<std::size_t>((std::bit_width(difference) - 1) / LEVEL_BITS, LEVELS - 1);
    return {level, static_cast<std::size_t>(tick >> shift(level)) & (SLOTS - 1)};
}

void TimerWheel::place(Timer &timer) noexcept {
    const auto [level, slot] = position(timer.tick);
    auto &head = slots_.at(level).at(slot);
    timer.prev = nullptr;
    timer.next = head;
    if (head) {
        head->prev = &timer;
    }
    head = &timer;
    timer.armed = true;
    timer.level = static_cast<std::uint8_t>(level);
    timer.slot = static_cast<std::uint8_t>(slot);
    occupied_.at(level) |= std::uint64_t{1} << slot;
}

void TimerWheel::arm(Timer &timer, const std::uint64_t tick) noexcept {
    cancel(timer);
    timer.tick = std::max(tick, now_ + 1);
    place(timer);
    ++count_;
}

void TimerWheel::cancel(Timer &timer) noexcept {
    if (!timer.armed) {
        return;
    }
    auto &head = slots_.at(timer.level).at(timer.slot);
    if (timer.prev) {
        timer.prev->next = timer.next;
    } else {
        head = timer.next;
    }
    if (timer.next) {
        timer.next->prev = timer.prev;
    }
    if (!head) {
        occupied_.at(timer.level) &= ~(std::uint64_t{1} << timer.slot);
    }
    timer.prev = timer.next = nullptr;
    timer.armed = false;
    --count_;
}

std::optional<std::uint64_t> TimerWheel::next() const noexcept {
    std::optional<std::uint64_t> result;
    for (std::size_t level = 0; level < LEVELS; ++level) {
        const auto current = static_cast<std::size_t>(now_ >> shift(level)) & (SLOTS - 1);
        const auto later = after(occupied_.at(level), current);
        if (later == 0) {
            continue;
        }
        // The start of the first occupied slot after the current one, within the current parent slot.
        const auto parent = now_ >> shift(level + 1) << shift(level + 1);
        const auto start = parent | (static_cast<std::uint64_t>(std::countr_zero(later)) << shift(level));
        result = result ? std::min(*result, start) : start;
    }
    return result;
}

TimerWheel::Timer *TimerWheel::take(const std::size_t level, const std::size_t slot) noexcept {
    auto *list = std::exchange(slots_.at(level).at(slot), nullptr);
    occupied_.at(level) &= ~(std::uint64_t{1} << slot);
    return list;
}

void TimerWheel::cascade(const std::size_t level) noexcept {
    auto *timer = take(level, static_cast<std::size_t>(now_ >> shift(level)) & (SLOTS - 1));
    while (timer) {
        auto *following = timer->next;
        place(*timer);
        timer = following;
    }
}

void TimerWheel::fire(std::vector<Timer *> &due) {
    auto *timer = take(0, static_cast<std::size_t>(now_) & (SLOTS - 1));
    while (timer) {
        auto *following = timer->next;
        timer->prev = timer->next = nullptr;
        timer->armed = false;
        --count_;
        due.push_back(timer);
        timer = following;
    }
}

std::vector<TimerWheel::Timer *> TimerWheel::advance(const std::uint64_t tick) {
    std::vector<Timer *> due;
    for (auto event = next(); event && *event <= tick; event = next()) {
        now_ = *event;
        // A slot reached at its start moves its timers down, highest level first, before level 0 fires.
        for (auto level = LEVELS - 1; level > 0; --level) {
            if ((now_ & ((std::uint64_t{1} << shift(level)) - 1)) == 0) {
                cascade(level);
            }
        }
        fire(due);
    }
    now_ = std::max(now_, tick);
    return due;
}

void TimerWheel::clear() noexcept {
    for (std::size_t level = 0; level < LEVELS; ++level) {
        for (std::size_t slot = 0; slot < SLOTS; ++slot) {
            for (auto *timer = take(level, slot); timer;) {
                auto *following = timer->next;
                timer->prev = timer->next = nullptr;
                timer->armed = false;
                timer = following;
            }
        }
    }
    count_ = 0;
}
} // namespace clause::runtime::detail
