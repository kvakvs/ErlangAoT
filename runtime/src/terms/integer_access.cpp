#include "integers.hpp"
#include <limits>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
bool Term::is_integer() const { return kind() == TermKind::smallint || kind() == TermKind::bignum; }

bool Term::is_float() const { return false; }

bool Term::is_number() const { return is_integer() || is_float(); }

TermResult<std::int64_t> Term::integer_value() const {
    if (kind() == TermKind::smallint) {
        return decode_integer(word());
    }
    try {
        const auto value = detail::integer_read(*this);
        if (!value) {
            return std::unexpected(value.error());
        }
        if (*value < std::numeric_limits<std::int64_t>::min() || *value > std::numeric_limits<std::int64_t>::max()) {
            return std::unexpected(TermError::out_of_range);
        }
        return value->convert_to<std::int64_t>();
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    }
}

TermResult<std::string> Term::integer_decimal() const {
    try {
        return detail::integer_read(*this).transform(detail::integer_text);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    }
}
} // namespace erlang_aot::runtime
