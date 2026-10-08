#include "binary_options.hpp"
#include "capabilities.hpp"
#include "match_plan_internal.hpp"

namespace clause::semantic {
namespace {
// Resolve the normalized literal without converting through host-width integers.
std::optional<MatchLiteral> float_literal(const MatchPlanner &state, const ast::BinarySegment &segment);

// A skip pattern keeps only the variables of segment values, which later sizes may read.
void skipped_value(const MatchPlanner &state, const ast::BinarySegment &segment, const std::size_t output,
                   const std::optional<std::int64_t> character, std::vector<MatchTask> &forward) {
    const auto &pattern = *state.patterns.at(&state.module.syntax->expression(segment.value));
    if (!character && pattern.kind == PatternKind::variable) {
        forward.emplace_back(PatternVisit{segment.value, output});
    }
}

// A generator pattern matches a prefix; a final tail segment holds the rest of the input (OTP append_tail_segment).
void rest(MatchPlanner &state, const NormalizedPattern &pattern, const std::size_t input, std::size_t &cursor,
          std::vector<MatchTask> &forward) {
    MatchNode tail{pattern.origin, MatchOperation::binary_extract, input};
    tail.index = cursor;
    tail.output = state.plan.values++;
    tail.cursor_output = state.plan.values++;
    tail.bits = BinaryOptions{.type = abi::v1::BitType::binary, .unit = 1, .size = 0, .all = true};
    state.plan.rest = tail.output;
    cursor = tail.cursor_output;
    forward.emplace_back(tail);
}

// One checked extraction produces both an owned candidate and the following encoded bit cursor.
void extract(MatchPlanner &state, const ast::BinarySegment &segment, const std::size_t input, std::size_t &cursor,
             std::vector<MatchTask> &forward, const std::optional<std::int64_t> character = {}) {
    MatchNode field{segment.value, MatchOperation::binary_extract, input};
    field.index = cursor;
    field.output = state.plan.values++;
    field.cursor_output = state.plan.values++;
    field.key = segment.size;
    field.bits = binary_options(segment);
    if (state.skip && field.bits->type == abi::v1::BitType::floating) {
        field.bits->type = abi::v1::BitType::integer;
    }
    if (field.bits->type == abi::v1::BitType::floating) {
        field.literal = character ? MatchLiteral{ast::IntegerLiteral{Integer{std::to_string(*character)}}}
                                  : float_literal(state, segment);
    }
    cursor = field.cursor_output;
    forward.emplace_back(field);
    if (state.skip) {
        skipped_value(state, segment, field.output, character, forward);
    } else if (character && !field.literal) {
        forward.emplace_back(MatchNode{segment.value, MatchOperation::exact_literal, field.output, {}, *character});
    } else if (!field.literal) {
        forward.emplace_back(PatternVisit{segment.value, field.output});
    }
}

// Literal float segments compare decoded values; integer literals are coerced before matching.
std::optional<MatchLiteral> float_literal(const MatchPlanner &state, const ast::BinarySegment &segment) {
    const auto &pattern = *state.patterns.at(&state.module.syntax->expression(segment.value));
    if (!pattern.literal) {
        return {};
    }
    if (const auto *real = std::get_if<ast::FloatLiteral>(&*pattern.literal)) {
        return *real;
    }
    if (const auto *integer = std::get_if<ast::IntegerLiteral>(&*pattern.literal)) {
        return *integer;
    }
    return {};
}

// Expand string characters within the same work ceiling, scheduling each value before the next size read.
bool fields(MatchPlanner &state, const ast::BinarySegment &segment, const std::size_t input, std::size_t &cursor,
            std::vector<MatchTask> &forward) {
    const auto &value = state.module.syntax->expression(ungroup(*state.module.syntax, segment.value)).value;
    const auto *string = std::get_if<ast::StringLiteral>(&value);
    const auto count = string ? string->value.size() : 1;
    for (std::size_t i = 0; i < count; ++i) {
        if (!state.spend(segment.value) || !state.spend(segment.value)) {
            return false;
        }
        extract(state, segment, input, cursor, forward,
                string ? std::optional<std::int64_t>{string->value[i]} : std::nullopt);
    }
    return true;
}
} // namespace

bool expand_bits(MatchPlanner &state, const PatternVisit &visit, const NormalizedPattern &pattern,
                 std::vector<MatchTask> &pending) {
    const auto &binary = std::get<ast::Bitstring>(state.module.syntax->expression(pattern.expression).value);
    std::vector<MatchTask> forward;
    auto cursor = state.plan.values++;
    MatchNode start{pattern.origin, MatchOperation::binary_start, visit.input};
    start.cursor_output = cursor;
    forward.emplace_back(start);
    for (const auto &segment : binary.segments) {
        if (!fields(state, segment, visit.input, cursor, forward)) {
            return false;
        }
    }
    if (state.generator == &state.module.syntax->expression(visit.id)) {
        rest(state, pattern, visit.input, cursor, forward);
    }
    MatchNode finish{pattern.origin, MatchOperation::binary_finish, visit.input};
    finish.index = cursor;
    forward.emplace_back(finish);
    pending.insert(pending.end(), forward.rbegin(), forward.rend());
    return true;
}
} // namespace clause::semantic
