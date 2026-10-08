#include "../builtins/portions.hpp"
#include <erlang_aot/abi/messages.hpp>
#include <erlang_aot/runtime/builtin_registry.hpp>
#include <erlang_aot/runtime/process_context.hpp>

// Selective receive (docs/processes.md#receive): generated code scans the mailbox with erlang_aot_receive_v1 and,
// having examined every message, enters the wait builtin until another message arrives.
namespace erlang_aot::runtime {
namespace {
Word wait(ProcessContext &context, builtins::Arguments state);

// The wait builtin; its register holds the receive's timeout (infinity until timeouts exist).
constexpr BuiltinFrame WAIT_FRAME = continuation_frame(&builtins::guarded<wait>, 1);

// true once a message the receive has not examined arrived; until then the process waits, resuming here.
Word wait(ProcessContext &context, builtins::Arguments state) {
    if (context.mailbox().unexamined()) {
        return builtins::need(TermFactory(context).boolean(true)).word();
    }
    context.stack().wait(WAIT_FRAME.frame, state);
    return 0;
}
} // namespace
} // namespace erlang_aot::runtime

std::uint8_t erlang_aot_receive_v1(void *context, std::uint8_t operation,
                                   erlang_aot::abi::v1::TermWord *output) noexcept {
    using erlang_aot::abi::v1::ReceiveOperation;
    auto &mailbox = static_cast<erlang_aot::runtime::ProcessContext *>(context)->mailbox();
    switch (static_cast<ReceiveOperation>(operation)) {
    case ReceiveOperation::peek: {
        const auto message = mailbox.peek();
        if (message) {
            *output = *message;
        }
        return message ? 1 : 0;
    }
    case ReceiveOperation::skip:
        mailbox.skip();
        return 0;
    case ReceiveOperation::take:
        mailbox.take();
        return 0;
    }
    return 0;
}

const void *erlang_aot_wait_frame_v1(void *) noexcept { return &erlang_aot::runtime::WAIT_FRAME.frame; }
