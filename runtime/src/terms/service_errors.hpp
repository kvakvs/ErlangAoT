#pragma once
#include <erlang_aot/abi/status.hpp>
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
// Keep ownership and resource failures separate from Erlang bad arguments in checked runtime services.
inline abi::v1::Status term_status(TermError error) noexcept {
    using abi::v1::Status;
    static constexpr std::array entries{std::pair{TermError::out_of_memory, Status::out_of_memory},
                                        std::pair{TermError::resource_limit, Status::resource_limit},
                                        std::pair{TermError::wrong_owner, Status::wrong_owner},
                                        std::pair{TermError::expired_context, Status::stopped},
                                        std::pair{TermError::not_implemented, Status::not_implemented},
                                        std::pair{TermError::diagnostic_failure, Status::diagnostic_failure},
                                        std::pair{TermError::stale_term, Status::internal_error}};
    for (const auto &[term, status] : entries) {
        if (term == error) {
            return status;
        }
    }
    return Status::invalid_argument;
}
} // namespace erlang_aot::runtime::detail
