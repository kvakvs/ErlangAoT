#pragma once

// Empty owner lifecycle is implemented; selective receive and scheduler-managed suspension remain sketches.
// C++ co_await illustrates the continuation protocol; see processes.md for lowering rules.
#include "terms.hpp"

#include <coroutine>
#include <cstdint>
#include <expected>
#include <memory>

namespace erlang_aot::runtime {
class Mailbox;
class Process;
class ReceiveCursor;

// Keep cursor misuse distinct from an exhausted mailbox, which suspends rather than errors.
enum class MailboxError : std::uint8_t { receive_active, invalid_cursor, no_current_message, expired_context };
template <typename Value> using MailboxResult = std::expected<Value, MailboxError>;

// Await the next candidate; an exhausted scan always returns control to the scheduler.
class MailboxRead final {
  public:
    // Transfer the pending operation without duplicating its waiter registration.
    MailboxRead(MailboxRead &&other) noexcept;
    MailboxRead &operator=(MailboxRead &&other) noexcept;
    // Cancel any outstanding waiter token before its continuation frame is destroyed.
    ~MailboxRead();
    // Keep exactly one consumer for each asynchronous cursor advancement.
    MailboxRead(const MailboxRead &) = delete;
    MailboxRead &operator=(const MailboxRead &) = delete;

    // Obtain an unread candidate immediately, or report misuse without parking the process.
    bool await_ready();
    // Register receive-wait; the scheduler handles pending signals and rechecks before parking.
    bool await_suspend(std::coroutine_handle<> continuation);
    // Return the next rooted candidate after scheduler resumption; end-of-mailbox is never a value.
    MailboxResult<Term> await_resume();

  private:
    friend class ReceiveCursor;
    // Retain cursor state, candidate root and a generation-checked scheduler waiter token.
    class Impl;
    std::unique_ptr<Impl> impl_;
    // Bind one validated scan advancement to the current receive session.
    explicit MailboxRead(std::unique_ptr<Impl> impl);
};

// Scan in arrival order, preserving unmatched messages and position across waits and tick yields.
class ReceiveCursor final {
  public:
    // Move the receive session without changing its stable mailbox position.
    ReceiveCursor(ReceiveCursor &&other) noexcept;
    ReceiveCursor &operator=(ReceiveCursor &&other) noexcept;
    // Abandon the receive, cancel its waiter and leave all unconsumed messages in place.
    ~ReceiveCursor();
    // Forbid two cursors from consuming through one active receive session.
    ReceiveCursor(const ReceiveCursor &) = delete;
    ReceiveCursor &operator=(const ReceiveCursor &) = delete;

    // Advance past the previous candidate; co_await suspends automatically at the mailbox tail.
    [[nodiscard]] MailboxRead next();
    // Remove only the current matched message, return its rooted value and close this receive.
    MailboxResult<Term> receive();

  private:
    friend class Mailbox;
    friend class MailboxRead;
    // Share stable scan state with an outstanding read; never store raw pointers into term cells.
    class Impl;
    std::shared_ptr<Impl> impl_;
    // Open one mailbox-owned receive session at the oldest still-enqueued message.
    explicit ReceiveCursor(std::shared_ptr<Impl> impl);
};

// Keep handled message signals as rooted terms until received; unhandled signals stay in Process.
class Mailbox final {
  public:
    // Release messages and invalidate all cursor generations before the heap is destroyed.
    ~Mailbox();
    // Keep mailbox identity stable across process continuation suspension.
    Mailbox(const Mailbox &) = delete;
    Mailbox &operator=(const Mailbox &) = delete;

    // Create a fresh oldest-first cursor; reject a second active receive on this process.
    MailboxResult<ReceiveCursor> begin_receive();

  private:
    friend class ProcessContext;
    friend class Process;
    friend class ReceiveCursor;
    friend class MailboxRead;
    // Bind mailbox roots and waiter registration to one process and its scheduler.
    explicit Mailbox(ProcessContext &owner);
    // Store ordered stable message IDs, rooted terms, receive generation and arrival version.
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Called only by Process signal handling with a receiver-owned term; preserve explicit suspension.
    // Append atomically and wake a pending tail read only once a candidate is available.
    TermResult<void> append_handled_message(Term value);
};
} // namespace erlang_aot::runtime
