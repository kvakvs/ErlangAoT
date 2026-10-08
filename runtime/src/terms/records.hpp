#pragma once
#include <clause/runtime/code_server.hpp>
#include <clause/runtime/terms.hpp>
#include <optional>
#include <span>
#include <utility>

// Native record cells (docs/native-records.md): a captured definition followed by field values.
namespace clause::runtime::detail {
// A decoded native record, borrowing the cell's words while its Term is current.
struct RecordView {
    // The definition captured when the record was built; it lives as long as the runtime.
    const RecordDefinition *definition;
    // Field values in definition order.
    std::span<const Word> values;
};

// Decode a native record; wrong_type for any other term.
TermResult<RecordView> record_view(const Term &value) noexcept;
// The module and record name atoms a record captured.
TermResult<std::pair<Term, Term>> record_identity(const Term &value) noexcept;
// Zero-based position of a field-name atom word in a definition.
std::optional<std::size_t> record_position(const RecordDefinition &definition, Word field) noexcept;
} // namespace clause::runtime::detail
