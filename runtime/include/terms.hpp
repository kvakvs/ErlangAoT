#pragma once

// TermFactory exposes reporting placeholders; immediate-only Term lives in the namespaced header.
// See runtime/design/terms.md for proposed ownership, immutable updates and the private ABI boundary.
#include <array>
#include <cstddef>
#include <cstdint>
#include <erlang_aot/runtime/features.hpp>
#include <erlang_aot/runtime/terms.hpp>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace erlang_aot::runtime {
class ContextLifetime;

// Reserve process-owned constructors; every operation currently reports term_services and returns failure.
// Raw small integers and empty containers remain available through Term::from_word.
class TermFactory final {
  public:
    // Bind a live-context token and borrowed sink without allocating or registering roots.
    explicit TermFactory(ProcessContext &context, DiagnosticSink sink = {}) noexcept;
    // Release the weak context binding; no process storage or roots are retained.
    ~TermFactory() = default;
    // Keep one factory binding; moving transfers it without moving the process.
    TermFactory(TermFactory &&other) noexcept = default;
    TermFactory &operator=(TermFactory &&other) noexcept = default;
    // Forbid accidental copying of the process-bound factory.
    TermFactory(const TermFactory &) = delete;
    TermFactory &operator=(const TermFactory &) = delete;

    // Construct machine-sized or arbitrary-precision integers (optional sign, decimal digits).
    TermResult<Term> integer(std::int64_t value);
    TermResult<Term> integer_decimal(std::string_view value);
    // Construct a finite Erlang float; reject NaN and infinity.
    TermResult<Term> floating(double value);
    // Delegate interning to the context's runtime AtomStorage; boolean uses its true/false entries.
    TermResult<Term> atom(std::string_view utf8);
    TermResult<Term> boolean(bool value);

    // Construct an empty list, a cons with any tail, or a proper list in input order.
    TermResult<Term> nil();
    TermResult<Term> cons(const Term &head, const Term &tail);
    TermResult<Term> list(std::span<const Term> elements);
    // Construct a tuple, including the zero-element tuple.
    TermResult<Term> tuple(std::span<const Term> elements);
    // Construct a map; the last input entry wins for an exactly equal key.
    TermResult<Term> map(std::span<const std::pair<Term, Term>> entries);
    // Copy bytes or an explicitly sized MSB-first sequence of bits.
    TermResult<Term> binary(std::span<const std::byte> bytes);
    TermResult<Term> bitstring(std::span<const std::byte> bytes, std::size_t bit_count);

    // Wrap runtime-issued identities; constructing a term does not spawn/open a resource.
    TermResult<Term> pid(const ProcessIdentity &identity);
    TermResult<Term> port(const PortIdentity &identity);
    TermResult<Term> reference(const ReferenceIdentity &identity);
    // Obtain a fresh unique reference from the owning runtime.
    TermResult<Term> make_reference();
    // Construct an external fun from module/name atoms and a validated arity.
    TermResult<Term> external_function(const Term &module, const Term &name, std::size_t arity);
    // Bind captures to a runtime-registered closure descriptor, never a raw C++ callback.
    TermResult<Term> closure(const ClosureDescriptor &descriptor, std::span<const Term> captures);
    // Rewrap an extracted callable identity without losing its captured values.
    TermResult<Term> function(const FunctionIdentity &identity);
    // Construct a registered native record with all fields in descriptor order.
    TermResult<Term> native_record(const NativeRecordDescriptor &descriptor, std::span<const Term> fields);

  private:
    // Reject expired or moved-from bindings before reporting an unavailable constructor.
    TermResult<Term> unavailable(std::string_view operation) const noexcept;
    // Inspect context liveness without retaining or dereferencing process storage.
    std::weak_ptr<const ContextLifetime> lifetime_;
    // Borrow diagnostic delivery state for this factory's lifetime; null selects stderr.
    DiagnosticSink sink_;
};
} // namespace erlang_aot::runtime
