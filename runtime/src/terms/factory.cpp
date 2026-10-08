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

TermResult<Term> TermFactory::atom(std::string_view spelling) {
    const auto lifetime = lifetime_.lock();
    if (!lifetime || !lifetime->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return atoms_->intern(spelling);
}

TermResult<Term> TermFactory::boolean(bool value) { return atom(value ? "true" : "false"); }

TermResult<Term> TermFactory::port(const PortIdentity &) { return unavailable("TermFactory::port"); }

TermResult<Term> TermFactory::reference(const ReferenceIdentity &) { return unavailable("TermFactory::reference"); }

TermResult<Term> TermFactory::function(const FunctionIdentity &) { return unavailable("TermFactory::function"); }
} // namespace erlang_aot::runtime
