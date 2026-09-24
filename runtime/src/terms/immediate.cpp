#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime {
TermResult<TermKind> classify_immediate(Word value) noexcept {
    const auto kind = TermTag{value}.get_kind();
    switch (kind) {
    case TermKind::header:
    case TermKind::catch_object:
        return std::unexpected(TermError::invalid_encoding);
    case TermKind::list:
    case TermKind::boxed:
        return std::unexpected(TermError::wrong_type);
    case TermKind::empty_tuple:
    case TermKind::empty_list:
        if ((value >> 6) != 0) {
            return std::unexpected(TermError::invalid_encoding);
        }
        return kind;
    default:
        return kind;
    }
}

TermResult<Word> encode_integer(std::int64_t value) noexcept {
    return abi::v1::NativeIntegerEncoding::encode(value).transform_error(
        [](abi::v1::IntegerError) { return TermError::out_of_range; });
}

TermResult<std::int64_t> decode_integer(Word value) noexcept {
    const auto kind = classify_immediate(value);
    if (!kind) {
        return std::unexpected(kind.error());
    }
    return abi::v1::NativeIntegerEncoding::decode(value).transform_error(
        [](abi::v1::IntegerError) { return TermError::wrong_type; });
}
} // namespace erlang_aot::runtime
