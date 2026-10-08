#pragma once
#include "v1.hpp"
#include <cstdint>

namespace clause::abi::v1 {
// What CLAUSE_receive_v1 does to the process's mailbox (docs/processes.md#receive).
enum class ReceiveOperation : std::uint8_t {
    // Write the next message the receive has not examined; the result says whether there was one.
    peek = 0,
    // Leave the examined message, which no clause matched, and move on to the next.
    skip = 1,
    // Remove the examined message, which a clause matched; the next receive starts at the oldest message.
    take = 2,
    // The timeout expired: the next receive starts at the oldest message.
    restart = 3,
};
} // namespace clause::abi::v1

// One step of a receive over the calling process's mailbox. For peek, write the next unexamined message to `output`
// and return 1, or return 0 when every message has been examined; other operations return 0.
std::uint8_t CLAUSE_receive_v1(void *context, std::uint8_t operation, clause::abi::v1::TermWord *output) noexcept;
// The FrameDescriptor of the builtin a receive enters when it has examined every message, with its timeout in the
// first register: true once a message arrives, false once the timeout expired (at once for 0); the process waits in
// between. A timeout other than infinity or an integer in 0..4294967295 raises timeout_value.
const void *CLAUSE_wait_frame_v1(void *context) noexcept;
