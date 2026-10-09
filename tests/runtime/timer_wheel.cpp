#include "scheduler/timer_wheel.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

// The receive-timeout timer wheel (plan step 62B, docs/processes.md#receive-timeouts): many timers armed, most
// cancelled, the wheel advanced in uneven steps; every remaining timer fires once, never before its tick and in the
// first advance that reaches it, in tick order, and no cancelled timer fires. The wheel never asks to be advanced
// later than its earliest timer.
namespace {
using clause::runtime::detail::TimerWheel;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

constexpr std::size_t TIMERS = 20'000;

// The earliest tick among armed timers, or none.
std::optional<std::uint64_t> earliest(const std::vector<TimerWheel::Timer> &timers) {
    std::optional<std::uint64_t> result;
    for (const auto &timer : timers) {
        if (timer.armed) {
            result = result ? std::min(*result, timer.tick) : timer.tick;
        }
    }
    return result;
}

void many_timers() {
    TimerWheel wheel;
    std::mt19937_64 random(62);
    std::vector<TimerWheel::Timer> timers(TIMERS);
    std::vector<bool> cancelled(TIMERS);
    for (std::size_t index = 0; index < TIMERS; ++index) {
        // Mostly short timeouts, some over every level up to OTP's longest.
        const auto far = index % 50 == 0;
        const auto tick = far ? random() % 4'294'967'296 : 1 + random() % 20'000;
        timers[index].owner = &timers[index];
        wheel.arm(timers[index], tick);
        require(timers[index].tick == std::max<std::uint64_t>(tick, 1), "armed at another tick");
    }
    for (std::size_t index = 0; index < TIMERS; ++index) {
        if (random() % 10 != 0) {
            wheel.cancel(timers[index]);
            cancelled[index] = true;
        }
    }
    std::vector<bool> fired(TIMERS);
    std::uint64_t previous = 0;
    std::uint64_t last_tick = 0;
    while (!wheel.empty()) {
        const auto next = wheel.next();
        require(next && *next > wheel.now() && *next <= *earliest(timers), "next slot after the earliest timer");
        // Uneven steps: sometimes short of the next slot, sometimes far past it.
        const auto target = random() % 3 == 0 ? *next - 1 : *next + random() % (random() % 4 == 0 ? 1'000'000 : 50);
        for (auto *timer : wheel.advance(target)) {
            const auto index = static_cast<std::size_t>(static_cast<TimerWheel::Timer *>(timer->owner) - timers.data());
            require(!cancelled[index] && !fired[index], "a cancelled or fired timer fired");
            require(timer->tick <= target && timer->tick > previous, "a timer fired early or late");
            require(timer->tick >= last_tick, "timers fired out of order");
            last_tick = timer->tick;
            fired[index] = true;
        }
        previous = target;
    }
    for (std::size_t index = 0; index < TIMERS; ++index) {
        require(fired[index] != cancelled[index], "a timer neither fired nor was cancelled");
    }
}

// A timer armed for a tick already reached fires at the next tick; the longest timeout fires exactly at its tick.
void edges() {
    TimerWheel wheel;
    TimerWheel::Timer past;
    static_cast<void>(wheel.advance(100));
    wheel.arm(past, 50);
    require(past.tick == 101 && wheel.next() == 101, "a past tick is not the next one");
    require(wheel.advance(100).empty() && wheel.advance(101).size() == 1, "a past timer did not fire next");
    TimerWheel::Timer longest;
    const std::uint64_t tick = 101 + 4'294'967'295;
    wheel.arm(longest, tick);
    require(wheel.advance(tick - 1).empty(), "the longest timeout fired early");
    require(wheel.advance(tick).size() == 1 && wheel.empty(), "the longest timeout did not fire");
    TimerWheel::Timer again;
    wheel.arm(again, tick + 10);
    wheel.arm(again, tick + 5);
    require(wheel.next() == tick + 5 && wheel.advance(tick + 9).size() == 1, "re-arming kept the old tick");
    wheel.arm(again, tick + 20);
    wheel.clear();
    require(wheel.empty() && !again.armed && !wheel.next(), "clear left a timer");
}
} // namespace

int main() {
    try {
        many_timers();
        edges();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "timer wheel: " << TIMERS << " timers, 90% cancelled, fired once each in tick order\n";
}
