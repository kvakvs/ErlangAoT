#pragma once
#include "terms.hpp"
#include <map>

namespace erlang_aot::runtime {
// Immutable host pin; spellings survive runtime teardown without retaining a process or code image.
struct AtomValue final {
    // Copy directly into stable storage so allocation failures remain catchable on Debug STL hosts.
    AtomValue(Word word, std::string_view spelling) : word(word), spelling(spelling) {}

    // Preserve the globally non-recycled word used to reject foreign raw atom words.
    Word word;
    // Own validated UTF-8 bytes, without Unicode normalization.
    std::string spelling;
};

// Runtime-owned, host-serialized table; entries are retained until runtime teardown.
class AtomStorage final {
  public:
    static constexpr std::uint32_t hard_limit = std::uint32_t{1} << 26;
    static constexpr std::uint32_t default_limit = std::uint32_t{1} << 20;

    // Validate and deduplicate a spelling; failures never publish a partial entry.
    TermResult<Term> intern(std::string_view spelling) noexcept;
    // Admit only words issued by this runtime, without allocating or interning.
    TermResult<Term> lookup(Word word) const noexcept;

    // Observe retained entries for capacity accounting and registration diagnostics.
    std::size_t size() const noexcept { return names_.size(); }

  private:
    friend class Runtime;

    // Only runtime startup may construct storage after validating the immutable entry ceiling.
    explicit AtomStorage(std::uint32_t limit) : limit_(limit) {}

    // Bound entries independently of the process-wide non-recycled word namespace.
    std::uint32_t limit_;
    // Both indexes share immutable records; failed insertion rolls back the other index.
    std::map<std::string, std::shared_ptr<const AtomValue>, std::less<>> names_;
    std::map<Word, std::shared_ptr<const AtomValue>> words_;
};

// Erlang atoms contain at most 255 Unicode scalar values, including the empty spelling and NUL.
bool valid_atom_spelling(std::string_view spelling) noexcept;
} // namespace erlang_aot::runtime
