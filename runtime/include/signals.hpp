#pragma once

// The links, monitors and exit trapping of one process (docs/processes.md#links, #monitors): the pids of the
// processes it is linked to, in the order the links were made, the monitors it holds and those held on it, and
// whether exit signals reach it as messages.
#include "terms.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace erlang_aot::runtime {
class Signals final {
  public:
    Signals(const Signals &) = delete;
    Signals &operator=(const Signals &) = delete;
    Signals(Signals &&) = delete;
    Signals &operator=(Signals &&) = delete;
    ~Signals() = default;

    // Whether exit signals become {'EXIT', From, Reason} messages (process_flag(trap_exit, Bool)).
    bool trap_exit() const noexcept { return trap_exit_; }

    // Set exit trapping; returns the previous setting.
    bool set_trap_exit(bool trap) noexcept { return std::exchange(trap_exit_, trap); }

    // Link to the process of `pid`; nothing when already linked. Allocation failure throws.
    void link(Word pid) {
        if (!std::ranges::contains(links_, pid)) {
            links_.push_back(pid);
        }
    }

    // Remove the link to the process of `pid`, if there is one.
    void unlink(Word pid) noexcept { std::erase(links_, pid); }

    // Remove every link and return them, as the process ends.
    std::vector<Word> take_links() noexcept { return std::exchange(links_, {}); }

    // The monitors of one side, by reference: the other process's pid. References order by creation.
    using Monitors = std::map<ReferenceIdentity, Word>;

    // Monitor the process of `pid` with `reference` (monitor/2). Allocation failure throws.
    void monitor(const ReferenceIdentity &reference, Word pid) { monitors_.emplace(reference, pid); }

    // Stop the monitor `reference` this process holds; the monitored pid, or none when it is not active.
    std::optional<Word> demonitor(const ReferenceIdentity &reference) noexcept {
        const auto node = monitors_.extract(reference);
        return node ? std::optional{node.mapped()} : std::nullopt;
    }

    // Record that the process of `pid` monitors this one with `reference`. Allocation failure throws.
    void watch(const ReferenceIdentity &reference, Word pid) { watchers_.emplace(reference, pid); }

    // Forget the monitor `reference` held on this process.
    void unwatch(const ReferenceIdentity &reference) noexcept { watchers_.erase(reference); }

    // Remove and return the monitors this process holds, as it ends; allocation failure throws.
    Monitors take_monitors() { return std::exchange(monitors_, {}); }

    // Remove and return the monitors held on this process, as it ends: each gets a 'DOWN' message.
    Monitors take_watchers() { return std::exchange(watchers_, {}); }

  private:
    friend class ProcessContext;
    // Start without links, not trapping exits; the owning context holds it.
    Signals() = default;

    // Pid words of the linked processes, oldest link first, so exit signals go out in a stable order.
    std::vector<Word> links_;
    // Monitors this process holds: reference to the monitored pid.
    Monitors monitors_;
    // Monitors held on this process: reference to the monitoring pid.
    Monitors watchers_;
    // Turns exit signals other than kill into messages.
    bool trap_exit_ = false;
};
} // namespace erlang_aot::runtime
