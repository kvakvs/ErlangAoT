#include "immediate_order.hpp"
#include <array>
#include <erlang_aot/abi/immediate_services.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
namespace {
using Op = abi::v1::ImmediateOperation;
using Outcome = abi::v1::ValueOutcome;

struct Fault {
    // Semantic argument rejection never enters the channel; exact infrastructure status always does.
    Outcome outcome;
    abi::v1::Status status = abi::v1::Status::ok;
};

using Result = std::expected<Word, Fault>;

// Missing canonical slots indicate failed initialization, never an Erlang false result.
Result boolean(ProcessContext &context, bool value) {
    const auto term = context.atom_storage().boolean(value);
    if (!term) {
        return std::unexpected(Fault{Outcome::failure, abi::v1::Status::internal_error});
    }
    return term->word();
}

// Only four real representations are admitted; absent identity/container/numeric families classify false.
bool predicate(Op operation, const Term &value) {
    const auto kind = value.kind();
    const std::array predicates{value.is_atom(),
                                kind == TermKind::smallint,
                                kind == TermKind::smallint,
                                value.is_boolean(),
                                kind == TermKind::empty_tuple,
                                kind == TermKind::empty_list,
                                false,
                                false,
                                false,
                                false,
                                false,
                                false,
                                false,
                                false};
    return predicates.at(static_cast<unsigned>(operation) - static_cast<unsigned>(Op::is_atom));
}

// Exact comparisons share Term::exactly_equal with pattern matching; numeric extensions stay explicit.
Result comparison(ProcessContext &context, Op operation, const Term &left, const Term &right) {
    if (operation <= Op::not_equal) {
        const auto result = left.exactly_equal(right);
        if (!result) {
            return std::unexpected(Fault{Outcome::failure, abi::v1::Status::internal_error});
        }
        const bool inverse = operation == Op::exact_not_equal || operation == Op::not_equal;
        return boolean(context, *result != inverse);
    }
    const auto order = immediate_order(left, right);
    if (!order) {
        return std::unexpected(Fault{Outcome::failure, abi::v1::Status::not_implemented});
    }
    const std::array results{*order < 0, *order <= 0, *order > 0, *order >= 0};
    return boolean(context, results.at(static_cast<unsigned>(operation) - static_cast<unsigned>(Op::less)));
}

// Empty-container queries return exact zero; wrong types and unavailable empty extractions are semantic badarg.
Result query(const Term &left, Op operation, const Term &right) {
    if (operation == Op::is_function_arity) {
        const auto arity = right.integer_value();
        if (!arity || *arity < 0) {
            return std::unexpected(Fault{Outcome::bad_argument});
        }
        return Word{0};
    }
    const bool list = operation == Op::length && left.kind() == TermKind::empty_list;
    const bool tuple = (operation == Op::tuple_size || operation == Op::size) && left.kind() == TermKind::empty_tuple;
    if (list || tuple) {
        return encode_integer(0).value();
    }
    return std::unexpected(Fault{Outcome::bad_argument});
}

// Min/max return an original admitted word, with spelling-based order and left selection on equal values.
Result select(Op operation, const Term &left, const Term &right) {
    const auto order = immediate_order(left, right);
    if (!order) {
        return std::unexpected(Fault{Outcome::failure, abi::v1::Status::not_implemented});
    }
    const bool lhs = operation == Op::minimum ? *order <= 0 : *order >= 0;
    return lhs ? left.word() : right.word();
}

// Boolean operators validate admitted terms; lazy lowering checks only the reached left operand here.
Result logical(ProcessContext &context, Op operation, const Term &left, const Term &right) {
    if (!left.is_boolean()) {
        return std::unexpected(Fault{Outcome::bad_argument});
    }
    if (operation == Op::boolean_check) {
        return left.word();
    }
    const bool lhs = left.atom_spelling() == "true";
    if (operation == Op::logical_not) {
        return boolean(context, !lhs);
    }
    if (!right.is_boolean()) {
        return std::unexpected(Fault{Outcome::bad_argument});
    }
    const bool rhs = right.atom_spelling() == "true";
    const std::array results{lhs && rhs, lhs || rhs, lhs != rhs};
    return boolean(context, results.at(static_cast<unsigned>(operation) - static_cast<unsigned>(Op::logical_and)));
}

// Dispatch only semantically authorized opcodes; later representation services extend this boundary.
Result evaluate(ProcessContext &context, Op operation, const Term &left, const Term &right) {
    if (operation >= Op::logical_not) {
        return logical(context, operation, left, right);
    }
    if (operation <= Op::greater_equal) {
        return comparison(context, operation, left, right);
    }
    if (operation <= Op::is_function) {
        return boolean(context, predicate(operation, left));
    }
    if (operation == Op::minimum || operation == Op::maximum) {
        return select(operation, left, right);
    }
    const auto result = query(left, operation, right);
    if (result && operation == Op::is_function_arity) {
        return boolean(context, false);
    }
    return result;
}

// A unary service never tries to admit its unused placeholder operand.
bool binary(Op operation) {
    return operation <= Op::greater_equal || operation == Op::is_function_arity || operation == Op::element ||
           operation == Op::minimum || operation == Op::maximum ||
           (operation >= Op::logical_and && operation <= Op::logical_xor);
}

// Ownership/encoding failures precede semantic argument classification and cannot reject a guard silently.
Result invoke(ProcessContext &context, Op operation, Word left, Word right) {
    const auto lhs = Term::from_word(left, context);
    const auto rhs = binary(operation) ? Term::from_word(right, context) : TermResult<Term>{Term{}};
    if (!lhs || !rhs) {
        return std::unexpected(Fault{Outcome::failure, abi::v1::Status::invalid_argument});
    }
    return evaluate(context, operation, *lhs, *rhs);
}
} // namespace

// Publish only checked results and preserve first infrastructure failure under native fault/reentry seams.
std::uint8_t immediate_service(ProcessContext &context, std::uint8_t operation, Word left, Word right,
                               Word *output) noexcept {
    auto &state = context.generated_calls();
    if (!state.active() || state.failure()) {
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    if (!output || operation > static_cast<std::uint8_t>(Op::boolean_check)) {
        state.fail_service(abi::v1::Status::invalid_argument);
        return static_cast<std::uint8_t>(Outcome::failure);
    }
    try {
        const auto result = invoke(context, static_cast<Op>(operation), left, right);
        if (!result) {
            state.fail_service(result.error().status);
            return static_cast<std::uint8_t>(result.error().outcome);
        }
        *output = *result;
        return static_cast<std::uint8_t>(Outcome::success);
    } catch (const std::bad_alloc &) {
        state.fail_service(abi::v1::Status::out_of_memory);
        return static_cast<std::uint8_t>(Outcome::failure);
    } catch (const std::length_error &) {
        state.fail_service(abi::v1::Status::resource_limit);
        return static_cast<std::uint8_t>(Outcome::failure);
    } catch (...) {
        state.fail_service(abi::v1::Status::internal_error);
        return static_cast<std::uint8_t>(Outcome::failure);
    }
}
} // namespace erlang_aot::runtime::detail

std::uint8_t erlang_aot_immediate_v1(void *context, std::uint8_t operation, erlang_aot::abi::v1::TermWord left,
                                     erlang_aot::abi::v1::TermWord right,
                                     erlang_aot::abi::v1::TermWord *output) noexcept {
    if (!context) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::ValueOutcome::failure);
    }
    return erlang_aot::runtime::detail::immediate_service(*static_cast<erlang_aot::runtime::ProcessContext *>(context),
                                                          operation, left, right, output);
}
