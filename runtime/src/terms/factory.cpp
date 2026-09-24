#include "terms.hpp"
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermFactory::TermFactory(ProcessContext &context, DiagnosticSink sink) noexcept
    : lifetime_(context.lifetime()), sink_(sink) {}

TermResult<Term> TermFactory::unavailable(std::string_view operation) const noexcept {
    const auto lifetime = lifetime_.lock();
    if (!lifetime || !lifetime->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return deferred_service<TermError>(abi::v1::FeatureId::term_services, operation, sink_);
}

TermResult<Term> TermFactory::integer(std::int64_t) { return unavailable("TermFactory::integer"); }

TermResult<Term> TermFactory::integer_decimal(std::string_view) { return unavailable("TermFactory::integer_decimal"); }

TermResult<Term> TermFactory::floating(double) { return unavailable("TermFactory::floating"); }

TermResult<Term> TermFactory::atom(std::string_view) { return unavailable("TermFactory::atom"); }

TermResult<Term> TermFactory::boolean(bool) { return unavailable("TermFactory::boolean"); }

TermResult<Term> TermFactory::nil() { return unavailable("TermFactory::nil"); }

TermResult<Term> TermFactory::cons(const Term &, const Term &) { return unavailable("TermFactory::cons"); }

TermResult<Term> TermFactory::list(std::span<const Term>) { return unavailable("TermFactory::list"); }

TermResult<Term> TermFactory::tuple(std::span<const Term>) { return unavailable("TermFactory::tuple"); }

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
