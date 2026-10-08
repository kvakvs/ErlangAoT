#include "capabilities.hpp"
#include "pattern_state.hpp"
#include "records.hpp"

namespace clause::semantic {
namespace {
enum class Action : std::uint8_t { pattern, read, segment, publish_segment };

struct Visit {
    // Reads borrow an immutable incoming scope, except for a binary's own preceding segment definitions.
    ast::ExprId id;
    std::optional<std::size_t> argument;
    std::shared_ptr<BindingEnvironment> visible;
    Action action = Action::pattern;
};

struct LiteralCost {
    // Charge retained scalar text as work so many large constants cannot multiply the module memory budget.
    template <typename T> std::size_t operator()(const T &) const { return 1; }

    std::size_t operator()(const ast::IntegerLiteral &value) const { return value.value.decimal.size(); }

    std::size_t operator()(const ast::StringLiteral &value) const { return value.value.size(); }

    std::size_t operator()(const ast::Atom &value) const { return value.name.size(); }
};

struct Walk {
    // Share one transaction/budget while scheduling flat, explicitly scoped semantic tasks.
    BindingAnalysis &state;
    BindingCandidate &scope;
    BindingContext context;
    std::vector<Visit> &pending;
    Visit visit;
    ast::ExprId expression;

    // Store normalized structure and its original source anchor separately from executable capability.
    void node(const PatternKind kind, std::vector<ast::ExprId> children = {},
              std::optional<PatternLiteral> literal = {}) {
        const auto cost = literal ? std::visit(LiteralCost{}, *literal) : 1;
        if (!state.spend(expression, cost + children.size())) {
            return;
        }
        state.function.patterns.push_back({visit.id, expression, kind, std::move(children), std::move(literal)});
    }

    // All compound siblings use the same readable environment; only equality constraints accumulate.
    void children(const PatternKind kind, const std::vector<ast::ExprId> &ids, const bool whole = false) {
        node(kind, ids);
        if (state.work > state.limit) {
            return;
        }
        for (auto child = ids.rbegin(); child != ids.rend(); ++child) {
            pending.push_back({*child, whole ? visit.argument : std::nullopt, visit.visible});
        }
    }

    // Unknown expression syntax in a permissive pattern is always a semantic error.
    template <typename T> void operator()(const T &) { pattern_error(state, expression); }

    void operator()(const ast::Variable &value) {
        node(value.name == U"_" ? PatternKind::wildcard : PatternKind::variable);
        state.define(expression, scope, context, visit.argument);
    }

    void operator()(const ast::Atom &value) { node(PatternKind::literal, {}, value); }

    void operator()(const ast::IntegerLiteral &value) { node(PatternKind::literal, {}, value); }

    void operator()(const ast::FloatLiteral &value) { node(PatternKind::literal, {}, value); }

    void operator()(const ast::StringLiteral &value) { node(PatternKind::literal, {}, value); }

    void operator()(const ast::CharacterLiteral &value) {
        node(PatternKind::literal, {}, ast::IntegerLiteral{Integer{std::to_string(value.value)}});
    }

    void operator()(const ast::UnaryExpression &) {
        node(PatternKind::literal, {}, pattern_constant(state, expression));
    }

    void operator()(const ast::Tuple &value) { children(PatternKind::tuple, value.elements); }

    void operator()(const ast::List &value) {
        std::vector<ast::ExprId> ids;
        ids.reserve(value.elements.size() + 1);
        ids.assign(value.elements.begin(), value.elements.end());
        if (value.tail) {
            ids.push_back(*value.tail);
        }
        children(PatternKind::list, ids);
    }

    void operator()(const ast::MatchExpression &value) {
        children(PatternKind::alias, {value.left, value.right}, true);
    }

    void operator()(const ast::RecordIndex &value) {
        const auto *layout =
            record_layout(state.module, value.record, state.module.syntax->expression(expression).source);
        const auto field = layout ? record_field(*layout, value.field) : std::nullopt;
        node(PatternKind::record_index, {},
             field ? std::optional<PatternLiteral>{ast::IntegerLiteral{Integer{std::to_string(*field + 2)}}}
                   : std::nullopt);
    }

    void operator()(const ast::RecordExpression &value) {
        if (!record_budget(state, expression)) {
            return;
        }
        if (value.base) {
            pattern_error(state, expression, "record update is illegal in a pattern");
        }
        children(PatternKind::record, pattern_fields(state.module, value));
        if (const auto *layout = record_layout(state.module, value.identity); layout && state.work <= state.limit) {
            state.function.patterns.back().literal = layout->name;
        }
    }

    void operator()(const ast::MapExpression &value) {
        node(PatternKind::map, binding_children(value));
        if (value.base) {
            pattern_error(state, expression, "map update is illegal in a pattern");
        }
        for (auto field = value.fields.rbegin(); field != value.fields.rend(); ++field) {
            if (field->kind != ast::MapFieldKind::exact) {
                pattern_error(state, field->key, "map pattern requires ':='");
            }
            pending.push_back({field->value, {}, visit.visible});
            pending.push_back({field->key, {}, visit.visible, Action::read});
        }
    }

    void operator()(const ast::Bitstring &value) {
        node(PatternKind::bitstring, binding_children(value));
        pattern_binary(state, expression, value);
        if (!state.spend(expression, visit.visible->names.size() + visit.visible->unsafe.size() + 1)) {
            return;
        }
        auto visible = std::make_shared<BindingEnvironment>(*visit.visible);
        for (auto segment = value.segments.rbegin(); segment != value.segments.rend(); ++segment) {
            pending.push_back({segment->value, {}, visible, Action::publish_segment});
            pending.push_back({segment->value, {}, visible, Action::segment});
            if (segment->size) {
                pending.push_back({*segment->size, {}, visible, Action::read});
            }
        }
    }

    void operator()(const ast::BinaryExpression &value);
};

// Parentheses are transparent to normalized identity while their original source remains attached.
std::optional<ast::ExprId> ungroup_pattern(BindingAnalysis &state, ast::ExprId id) {
    while (const auto *group = std::get_if<ast::Group>(&state.module.syntax->expression(id).value)) {
        if (!state.spend(id)) {
            return {};
        }
        id = group->expression;
    }
    return id;
}

// Parentheses disappear in OTP's abstract pattern prefix elements too.
bool prefix_elements(BindingAnalysis &state, const ast::List &list) {
    for (const auto &element_id : list.elements) {
        const auto child = ungroup_pattern(state, element_id);
        if (!child || !state.spend(*child)) {
            return false;
        }
        const auto &element = state.module.syntax->expression(*child).value;
        if (!std::holds_alternative<ast::IntegerLiteral>(element) &&
            !std::holds_alternative<ast::CharacterLiteral>(element)) {
            return false;
        }
    }
    return true;
}

// Walk explicit literal tails iteratively: [1|[2|[]]] is as legal as [1,2], but [a] is not.
bool prefix(BindingAnalysis &state, ast::ExprId id) {
    while (state.spend(id)) {
        const auto &value = state.module.syntax->expression(id).value;
        if (const auto *group = std::get_if<ast::Group>(&value)) {
            id = group->expression;
            continue;
        }
        if (std::holds_alternative<ast::StringLiteral>(value)) {
            return true;
        }
        const auto *list = std::get_if<ast::List>(&value);
        if (!list || !prefix_elements(state, *list)) {
            return false;
        }
        if (!list->tail) {
            return true;
        }
        id = *list->tail;
    }
    return false;
}

void Walk::operator()(const ast::BinaryExpression &value) {
    if (value.operation != ast::BinaryOperator::append) {
        node(PatternKind::literal, {}, pattern_constant(state, expression));
    } else if (prefix(state, value.left)) {
        children(PatternKind::prefix, {value.left, value.right});
    } else {
        pattern_error(state, expression);
    }
}

// Segment values have a stricter grammar than enclosing patterns: no alias or nested container.
void segment(Walk &walk, const ast::ExprValue &value) {
    if (std::holds_alternative<ast::Variable>(value) || std::holds_alternative<ast::StringLiteral>(value)) {
        std::visit(walk, value);
    } else {
        walk.node(PatternKind::literal, {}, pattern_constant(walk.state, walk.expression));
    }
}

// Only this binary's already analyzed variable segments become visible to its subsequent sizes.
void publish(const Walk &walk, const ast::ExprValue &value) {
    const auto *variable = std::get_if<ast::Variable>(&value);
    if (variable) {
        if (const auto identity = walk.scope.find(variable->name)) {
            walk.visit.visible->names.emplace(variable->name, *identity);
        }
    }
}

// Read-only syntax never contributes definitions, including inside a key's nested containers or calls.
void read(const Walk &walk) {
    BindingCandidate readable{*walk.visit.visible, {}};
    walk.state.read(walk.expression, readable, walk.context);
    walk.scope.valid = walk.scope.valid && readable.valid;
    const auto children = pattern_expression(walk.state, walk.expression);
    for (auto child = children.rbegin(); child != children.rend(); ++child) {
        walk.pending.push_back({*child, {}, walk.visit.visible, Action::read});
    }
}

// Dispatch bounded tasks without recursively visiting either patterns or their embedded expressions.
void execute(Walk &walk) {
    const auto &expression = walk.state.module.syntax->expression(walk.expression);
    validate_record(walk.state.module, expression, walk.state.out, true);
    const auto &value = expression.value;
    switch (walk.visit.action) {
    case Action::read:
        read(walk);
        break;
    case Action::segment:
        segment(walk, value);
        break;
    case Action::publish_segment:
        publish(walk, value);
        break;
    case Action::pattern:
        std::visit(walk, value);
        break;
    }
}
} // namespace

void bind_pattern(BindingAnalysis &state, const ast::PatternSyntaxId &id, BindingCandidate &scope,
                  const BindingContext context, const std::optional<std::size_t> argument) {
    bind_pattern(state, pattern_root(*state.module.syntax, id), scope, context, argument);
}

void bind_pattern(BindingAnalysis &state, const ast::ExprId &id, BindingCandidate &scope, const BindingContext context,
                  const std::optional<std::size_t> argument) {
    if (!state.spend(id, scope.incoming.names.size() + scope.incoming.unsafe.size() + 1)) {
        scope.valid = false;
        return;
    }
    auto visible = std::make_shared<BindingEnvironment>(scope.incoming);
    std::vector<Visit> pending{{id, argument, std::move(visible)}};
    while (!pending.empty() && state.work <= state.limit) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id)) {
            break;
        }
        if (const auto expression = ungroup_pattern(state, visit.id)) {
            Walk walk{state, scope, context, pending, visit, *expression};
            execute(walk);
        }
    }
    scope.valid = scope.valid && !state.invalid_pattern && state.work <= state.limit;
}
} // namespace clause::semantic
