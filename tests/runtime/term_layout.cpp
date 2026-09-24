#include "term_layout.hpp"
#include <erlang_aot/abi/term.hpp>
#include <iostream>

using namespace erlang_aot::runtime;
using Encoding = erlang_aot::abi::v1::NativeIntegerEncoding;

// Compile every private prefix assertion and prove the runtime decoder shares the ABI's low-bit tags.
int main() {
    constexpr auto negative = *Encoding::encode(-42);
    constexpr auto positive = *Encoding::encode(42);
    static_assert(TermTag{negative}.get_kind() == TermKind::smallint);
    static_assert(TermTag{positive}.get_kind() == TermKind::smallint);
    static_assert(TermTag{1}.get_kind() == TermKind::list);
    static_assert(TermTag{2}.get_kind() == TermKind::boxed);
    static_assert(TermTag{0x3b}.get_kind() == TermKind::empty_list);
    static_assert(TermTag{0x2b}.get_kind() == TermKind::empty_tuple);
    std::cout << "term/header word bytes: " << sizeof(Term) << '/' << sizeof(detail::layout::BoxHeader) << '\n';
}
