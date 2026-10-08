#pragma once

// The links and exit trapping of one process (docs/processes.md#links): the pids of the processes it is linked to,
// in the order the links were made, and whether exit signals reach it as messages.
#include "terms.hpp"

#include <algorithm>
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

  private:
    friend class ProcessContext;
    // Start without links, not trapping exits; the owning context holds it.
    Signals() = default;

    // Pid words of the linked processes, oldest link first, so exit signals go out in a stable order.
    std::vector<Word> links_;
    // Turns exit signals other than kill into messages.
    bool trap_exit_ = false;
};
} // namespace erlang_aot::runtime
