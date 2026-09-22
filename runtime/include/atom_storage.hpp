#pragma once

// REVIEW SKETCH ONLY: API declarations, no interning, table lookup or collector implementation.
// One runtime owns one AtomStorage; see atom_storage.md for identity and compaction rules.
#include "terms.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace erlang_aot::runtime {
// Distinguish capacity, identity exhaustion and ownership failures without inventing an atom.
enum class AtomStorageError : std::uint8_t {
    invalid_options,
    invalid_name,
    limit_reached,
    id_exhausted,
    unknown_id,
    wrong_owner,
    expired_context,
    out_of_memory,
    unsafe_point,
    not_implemented
};
template <typename Value> using AtomStorageResult = std::expected<Value, AtomStorageError>;

// Intern immutable spellings under stable numeric identities shared by all runtime processes.
class AtomStorage final {
  public:
    // Supply the atom-table cap once during runtime startup; entries/storage are allocated lazily.
    struct AtomStorageOptions final {
        // Never permit more than 2^26 retained atom entries, regardless of startup configuration.
        static constexpr std::uint32_t hard_limit = std::uint32_t{1} << 26;
        // Use 2^20 entries unless runtime startup explicitly selects another valid cap.
        static constexpr std::uint32_t default_limit = std::uint32_t{1} << 20;
        // Bound retained entries, including builtins; valid range is [1, hard_limit].
        std::uint32_t max_atoms = default_limit;
    };

    // Copy counters consistently under the table lock without exposing backing storage.
    struct AtomStorageStats final {
        // Count entries currently retained; before GC this also equals the next sequential ID.
        std::uint32_t retained_atoms;
        // Report the immutable cap selected at runtime startup.
        std::uint32_t max_atoms;
        // Report the largest successfully issued ID; empty storage has no issued ID.
        std::optional<AtomId> highest_issued;
    };

    // Reserve collection reporting; the first non-collecting implementation reports not_implemented.
    struct AtomCollectionStats final {
        // Count retained entries immediately before tracing/reclamation.
        std::uint32_t atoms_before;
        // Count surviving entries; every survivor retains its original ID and exact spelling.
        std::uint32_t atoms_after;
        // Report backing bytes released by table/string-storage compaction.
        std::size_t bytes_reclaimed;
    };

  public:
    // Validate the startup cap and create an empty table; runtime bootstrap interns builtins afterward.
    static AtomStorageResult<std::unique_ptr<AtomStorage>> start(AtomStorageOptions options = {});
    // Release tables only after workers, code metadata and atom roots have been shut down.
    ~AtomStorage();
    // Keep runtime identity and table ownership unique; access the same instance through process contexts.
    AtomStorage(const AtomStorage &) = delete;
    AtomStorage &operator=(const AtomStorage &) = delete;
    AtomStorage(AtomStorage &&) = delete;
    AtomStorage &operator=(AtomStorage &&) = delete;

    // Intern UTF-8 spelling and return a caller-rooted atom Term whose payload is the stable AtomId.
    // Module initialization uses this boundary to bind compiled read-only atom constants at runtime.
    AtomStorageResult<Term> create(ProcessContext &context, std::string_view utf8);
    // Find an existing spelling and root its atom without creating a name or consuming an ID.
    AtomStorageResult<std::optional<Term>> find(ProcessContext &context, std::string_view utf8) const;
    // Wrap an existing ID in a caller-rooted atom; before GC the checked lookup is direct indexing.
    AtomStorageResult<Term> lookup(ProcessContext &context, AtomId id) const;
    // Copy spelling by ID so callers never borrow bytes that a future compactor could relocate.
    AtomStorageResult<std::string> name(AtomId id) const;
    // Obtain one consistent retained-count/cap/high-water snapshot.
    AtomStorageStats stats() const;
    // TODO(atom-gc): require a runtime-wide safe point; reserve reclamation/compaction without changing IDs.
    AtomStorageResult<AtomCollectionStats> collect();

  private:
    // Construct only with a validated cap through the runtime startup factory.
    explicit AtomStorage(AtomStorageOptions options);
    // Own the growable dense ID table, name-to-ID hash index, high-water counter and synchronization.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
