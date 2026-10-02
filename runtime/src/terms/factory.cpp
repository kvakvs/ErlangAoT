#include "terms.hpp"
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermFactory::TermFactory(ProcessContext &context, DiagnosticSink sink) noexcept
    : lifetime_(context.lifetime()), sink_(sink), atoms_(&context.atom_storage()), heap_(&context.heap()) {}

TermResult<Term> TermFactory::unavailable(std::string_view operation) const noexcept {
    const auto lifetime = lifetime_.lock();
    if (!lifetime || !lifetime->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return deferred_service<TermError>(abi::v1::FeatureId::term_services, operation, sink_);
}

TermResult<Term> TermFactory::floating(double) { return unavailable("TermFactory::floating"); }

TermResult<Term> TermFactory::atom(std::string_view spelling) {
    const auto lifetime = lifetime_.lock();
    if (!lifetime || !lifetime->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return atoms_->intern(spelling);
}

TermResult<Term> TermFactory::boolean(bool value) { return atom(value ? "true" : "false"); }

TermResult<Term> TermFactory::map(std::span<const std::pair<Term, Term>>) { return unavailable("TermFactory::map"); }

TermResult<Term> TermFactory::binary(std::span<const std::byte>) { return unavailable("TermFactory::binary"); }

TermResult<Term> TermFactory::bitstring(std::span<const std::byte>, std::size_t) {
    return unavailable("TermFactory::bitstring");
}

TermResult<Term> TermFactory::pid(const ProcessIdentity &) { return unavailable("TermFactory::pid"); }

TermResult<Term> TermFactory::port(const PortIdentity &) { return unavailable("TermFactory::port"); }

TermResult<Term> TermFactory::reference(const ReferenceIdentity &) { return unavailable("TermFactory::reference"); }

TermResult<Term> TermFactory::make_reference() { return unavailable("TermFactory::make_reference"); }

TermResult<Term> TermFactory::external_function(const Term &, const Term &, std::size_t) {
    return unavailable("TermFactory::external_function");
}

TermResult<Term> TermFactory::closure(const ClosureDescriptor &, std::span<const Term>) {
    return unavailable("TermFactory::closure");
}

TermResult<Term> TermFactory::function(const FunctionIdentity &) { return unavailable("TermFactory::function"); }

TermResult<Term> TermFactory::native_record(const NativeRecordDescriptor &, std::span<const Term>) {
    return unavailable("TermFactory::native_record");
}
} // namespace erlang_aot::runtime
