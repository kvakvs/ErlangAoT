#include <erlang_aot/runtime/terms.hpp>

#include <array>
#include <cstddef>
#include <iostream>

namespace {
using namespace erlang_aot::runtime;

// Rows enumerate (primary, secondary); columns enumerate tertiary, all in numeric tag order.
constexpr std::array<std::array<TermKind, 4>, 16> expected{{
    {TermKind::header, TermKind::header, TermKind::header, TermKind::header},
    {TermKind::header, TermKind::header, TermKind::header, TermKind::header},
    {TermKind::header, TermKind::header, TermKind::header, TermKind::header},
    {TermKind::header, TermKind::header, TermKind::header, TermKind::header},
    {TermKind::list, TermKind::list, TermKind::list, TermKind::list},
    {TermKind::list, TermKind::list, TermKind::list, TermKind::list},
    {TermKind::list, TermKind::list, TermKind::list, TermKind::list},
    {TermKind::list, TermKind::list, TermKind::list, TermKind::list},
    {TermKind::boxed, TermKind::boxed, TermKind::boxed, TermKind::boxed},
    {TermKind::boxed, TermKind::boxed, TermKind::boxed, TermKind::boxed},
    {TermKind::boxed, TermKind::boxed, TermKind::boxed, TermKind::boxed},
    {TermKind::boxed, TermKind::boxed, TermKind::boxed, TermKind::boxed},
    {TermKind::local_pid, TermKind::local_pid, TermKind::local_pid, TermKind::local_pid},
    {TermKind::local_port, TermKind::local_port, TermKind::local_port, TermKind::local_port},
    {TermKind::atom, TermKind::catch_object, TermKind::empty_tuple, TermKind::empty_list},
    {TermKind::smallint, TermKind::smallint, TermKind::smallint, TermKind::smallint},
}};

// Check one combination against the explicit truth table and report its tag coordinates on failure.
bool check_combination(std::size_t combination) {
    const auto primary = combination / 16;
    const auto secondary = (combination / 4) % 4;
    const auto tertiary = combination % 4;
    const TermTag tag{static_cast<Word>(primary | (secondary << 2) | (tertiary << 4))};
    const auto actual = tag.get_kind();
    const auto wanted = expected[combination / 4][tertiary];
    if (actual == wanted) {
        return true;
    }
    std::cerr << "primary=" << primary << " secondary=" << secondary << " tertiary=" << tertiary
              << ": expected TermKind=" << static_cast<unsigned>(wanted) << ", got=" << static_cast<unsigned>(actual)
              << '\n';
    return false;
}
} // namespace

// Exercise every combination, including fields that must be ignored when earlier tags do not delegate.
int main() {
    unsigned failures = 0;
    for (std::size_t combination = 0; combination < 64; ++combination) {
        failures += static_cast<unsigned>(!check_combination(combination));
    }
    return failures == 0 ? 0 : 1;
}
