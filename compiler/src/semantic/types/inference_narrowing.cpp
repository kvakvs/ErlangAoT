#include "inference_narrowing.hpp"
#include "../../preprocessor/value.hpp"
#include "../capabilities.hpp"
#include "../records.hpp"
#include "decimal.hpp"
#include "inference_containers.hpp"
#include "inference_values.hpp"
#include <charconv>

namespace clause::semantic::types {
namespace {
using Values = std::map<BindingId, Fact>;

// Patterns nested deeper than this match any term as far as their shape goes.
constexpr std::size_t SHAPE_DEPTH = 8;

// A literal size or arity; none for another expression.
std::optional<std::size_t> literal_count(const ast::Module &syntax, const ast::ExprId &id) {
    const auto *literal = std::get_if<ast::IntegerLiteral>(&syntax.expression(ungroup(syntax, id)).value);
    std::size_t value = 0;
    if (!literal) {
        return std::nullopt;
    }
    const auto &digits = literal->value.decimal;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return parsed.ec == std::errc{} && value <= 255 ? std::optional{value} : std::nullopt;
}

// A tuple of `size` elements whose first element is the atom `name`.
Id record_tuple(Lattice &lattice, const std::u32string &name, const std::size_t size) {
    std::vector<Id> elements(size, lattice.graph().top());
    if (size == 0) {
        return lattice.graph().bottom();
    }
    elements.front() = lattice.atom(utf8(name));
    return lattice.tuple(std::move(elements));
}

// Integer bounds as canonical decimals; a missing bound is unbounded.
struct Interval {
    std::optional<std::string> low;
    std::optional<std::string> high;
};

// The larger of two lower bounds and the smaller of two upper bounds.
std::optional<std::string> higher(const std::optional<std::string> &left, const std::optional<std::string> &right) {
    if (!left || !right) {
        return left ? left : right;
    }
    return decimal_less(*left, *right) ? right : left;
}

std::optional<std::string> lower(const std::optional<std::string> &left, const std::optional<std::string> &right) {
    if (!left || !right) {
        return left ? left : right;
    }
    return decimal_less(*left, *right) ? left : right;
}

// Join into `into` the facts `other` gives the same variables: either of two narrowings may hold.
void merge(Lattice &lattice, Values &into, const Values &other) {
    const auto top = Fact{lattice.graph().top()};
    for (auto &[identity, fact] : into) {
        const auto found = other.find(identity);
        const auto second = found == other.end() ? top : found->second;
        fact = {lattice.join(fact.type, second.type), fact.argument == second.argument ? fact.argument : std::nullopt};
    }
}

// A comparison of a variable with an integer constant, the variable on the left.
struct Comparison {
    BindingId identity;
    BigInt constant;
    ast::BinaryOperator operation;
};

// Narrows variables by tests assumed true.
class Assume final {
  public:
    explicit Assume(BindingFacts &bindings)
        : bindings_(bindings), syntax_(*bindings.function.module->syntax), lattice_(bindings.inference.graph) {}

    // Narrow `values` by a true test; false when it can never be true.
    bool test(const ast::ExprId &id, Values &values);
    // Narrow `values` by a true conjunction of tests.
    bool conjunction(const std::vector<ast::ExprId> &tests, Values &values);
    // Narrow `values` by one of two alternatives: the join of what each proves.
    template <typename Left, typename Right> bool either(Values &values, Left left, Right right);
    // The argument a type test checks and the fact of the values it accepts; none for another expression.
    std::optional<std::pair<ast::ExprId, Id>> type_test(const ast::Expression &expression);

  private:
    bool binary(const ast::BinaryExpression &value, Values &values);
    // A comparison of a proven integer variable with an integer constant.
    bool compare(const ast::BinaryExpression &value, Values &values);
    // Narrow a variable proven to be an integer to the integers of `range`.
    bool narrow_range(Values &values, BindingId identity, const Interval &range);
    // The variable, constant and operator (variable first) of a comparison of a variable with an integer constant.
    std::optional<Comparison> comparison(const ast::BinaryExpression &value) const;
    // is_record/2,3: a tuple of the record's size whose first element is its name.
    std::optional<Id> record_test(const ast::Expression &expression, const ast::CallExpression &call);
    // The bounds `operation` against `constant` proves for its left operand; none for others.
    std::optional<Interval> bound(ast::BinaryOperator operation, const BigInt &constant);
    // Narrow one variable by `fact`; false when nothing is left.
    bool narrow(Values &values, BindingId identity, Id fact);
    // The integer constant an expression's recorded fact is.
    std::optional<BigInt> constant(const ast::ExprId &id) const;
    // A type test's category for the test's name.
    std::optional<Id> named_category(const ast::Expression &expression, const ServiceResolution &service);

    BindingFacts &bindings_;
    const ast::Module &syntax_;
    Lattice lattice_;
};

bool Assume::test(const ast::ExprId &id, Values &values) {
    const auto &expression = syntax_.expression(ungroup(syntax_, id));
    if (const auto *value = std::get_if<ast::BinaryExpression>(&expression.value)) {
        return binary(*value, values);
    }
    if (const auto *atom = std::get_if<ast::Atom>(&expression.value)) {
        return atom->name != U"false";
    }
    const auto tested = type_test(expression);
    const auto identity = tested ? variable(bindings_, tested->first) : std::nullopt;
    return !identity || narrow(values, *identity, tested->second);
}

bool Assume::conjunction(const std::vector<ast::ExprId> &tests, Values &values) {
    for (const auto &item : tests) {
        if (!test(item, values)) {
            return false;
        }
    }
    return true;
}

template <typename Left, typename Right> bool Assume::either(Values &values, Left left, Right right) {
    auto first = values;
    auto second = values;
    const bool a = left(first);
    const bool b = right(second);
    if (!a || !b) {
        values = a ? first : second;
        return a || b;
    }
    merge(lattice_, first, second);
    values = std::move(first);
    return true;
}

bool Assume::binary(const ast::BinaryExpression &value, Values &values) {
    using Op = ast::BinaryOperator;
    if (value.operation == Op::and_also || value.operation == Op::logical_and) {
        return test(value.left, values) && test(value.right, values);
    }
    if (value.operation == Op::or_else || value.operation == Op::logical_or) {
        return either(
            values, [&](Values &left) { return test(value.left, left); },
            [&](Values &right) { return test(value.right, right); });
    }
    return compare(value, values);
}

std::optional<BigInt> Assume::constant(const ast::ExprId &id) const {
    const auto found = bindings_.inference.expressions.find(&syntax_.expression(id));
    if (found == bindings_.inference.expressions.end()) {
        return std::nullopt;
    }
    const auto &node = bindings_.inference.graph.get(found->second.type);
    return node.kind == Kind::integer ? std::optional{decimal_number(node.name)} : std::nullopt;
}

std::optional<Interval> Assume::bound(const ast::BinaryOperator operation, const BigInt &constant) {
    using Op = ast::BinaryOperator;
    const auto text = [](const BigInt &value) { return std::optional{decimal_integer(value)}; };
    switch (operation) {
    case Op::less:
        return Interval{std::nullopt, text(constant - 1)};
    case Op::less_equal:
        return Interval{std::nullopt, text(constant)};
    case Op::greater:
        return Interval{text(constant + 1), std::nullopt};
    case Op::greater_equal:
        return Interval{text(constant), std::nullopt};
    case Op::equal:
    case Op::exact_equal:
        return Interval{text(constant), text(constant)};
    default:
        return std::nullopt;
    }
}

std::optional<Comparison> Assume::comparison(const ast::BinaryExpression &value) const {
    static const std::map<ast::BinaryOperator, ast::BinaryOperator> FLIPPED{
        {ast::BinaryOperator::less, ast::BinaryOperator::greater},
        {ast::BinaryOperator::less_equal, ast::BinaryOperator::greater_equal},
        {ast::BinaryOperator::greater, ast::BinaryOperator::less},
        {ast::BinaryOperator::greater_equal, ast::BinaryOperator::less_equal},
        {ast::BinaryOperator::equal, ast::BinaryOperator::equal},
        {ast::BinaryOperator::exact_equal, ast::BinaryOperator::exact_equal}};
    const auto flipped = FLIPPED.find(value.operation);
    if (flipped == FLIPPED.end()) {
        return std::nullopt;
    }
    const auto left = variable(bindings_, value.left);
    const auto number = constant(left ? value.right : value.left);
    const auto identity = left ? left : variable(bindings_, value.right);
    if (!identity || !number) {
        return std::nullopt;
    }
    return Comparison{*identity, *number, left ? value.operation : flipped->second};
}

bool Assume::compare(const ast::BinaryExpression &value, Values &values) {
    const auto compared = comparison(value);
    const auto range = compared ? bound(compared->operation, compared->constant) : std::nullopt;
    return !range || narrow_range(values, compared->identity, *range);
}

bool Assume::narrow_range(Values &values, const BindingId identity, const Interval &range) {
    const auto current = values.contains(identity) ? values.at(identity).type : lattice_.graph().top();
    const auto numbers = lattice_.numbers(current);
    // Terms of every type compare: only a value proven to be an integer narrows to a range.
    if (!numbers.integers || numbers.floats || numbers.others) {
        return true;
    }
    // Intersect the bounds directly: a range bounded on one side only is no fact of its own.
    const auto low = higher(numbers.low, range.low);
    const auto high = lower(numbers.high, range.high);
    const bool empty = low && high && decimal_less(*high, *low);
    return narrow(values, identity, empty ? lattice_.graph().bottom() : lattice_.interval(low, high));
}

bool Assume::narrow(Values &values, const BindingId identity, const Id fact) {
    const auto current = values.contains(identity) ? values.at(identity) : Fact{lattice_.graph().top()};
    const auto next = lattice_.meet(current.type, fact);
    values.insert_or_assign(identity, Fact{next, current.argument});
    return next != lattice_.graph().bottom() || current.type == lattice_.graph().bottom();
}

std::optional<Id> Assume::named_category(const ast::Expression &expression, const ServiceResolution &service) {
    static const std::map<std::u32string, std::string_view> CATEGORIES{
        {U"is_atom", "atom"},           {U"is_boolean", "boolean"},
        {U"is_integer", "integer"},     {U"is_float", "float"},
        {U"is_number", "number"},       {U"is_binary", "binary"},
        {U"is_bitstring", "bitstring"}, {U"is_list", "maybe_improper_list"},
        {U"is_tuple", "tuple"},         {U"is_map", "map"},
        {U"is_function", "fun"},        {U"is_pid", "pid"},
        {U"is_port", "port"},           {U"is_reference", "reference"}};
    const auto &call = std::get<ast::CallExpression>(expression.value);
    const auto &name = service.identity.name;
    if (const auto found = CATEGORIES.find(name); found != CATEGORIES.end() && service.identity.arity == 1) {
        return lattice_.category(found->second);
    }
    if (name == U"is_function" && service.identity.arity == 2) {
        const auto arity = literal_count(syntax_, call.arguments[1]);
        return arity ? std::optional{lattice_.fun(*arity, lattice_.graph().top())} : std::nullopt;
    }
    return name == U"is_record" && service.identity.arity >= 2 ? record_test(expression, call) : std::nullopt;
}

std::optional<Id> Assume::record_test(const ast::Expression &expression, const ast::CallExpression &call) {
    const auto *record = std::get_if<ast::Atom>(&syntax_.expression(ungroup(syntax_, call.arguments[1])).value);
    const auto *layout = record ? record_layout(*bindings_.function.module, *record, expression.source) : nullptr;
    const auto size = call.arguments.size() == 3 ? literal_count(syntax_, call.arguments[2])
                                                 : (layout ? std::optional{layout->fields.size() + 1} : std::nullopt);
    return record && size ? std::optional{record_tuple(lattice_, record->name, *size)} : std::nullopt;
}

std::optional<std::pair<ast::ExprId, Id>> Assume::type_test(const ast::Expression &expression) {
    const auto *call = std::get_if<ast::CallExpression>(&expression.value);
    const auto &services = bindings_.function.function->services;
    const auto service = call ? services.find(&expression) : services.end();
    if (service == services.end() || call->arguments.empty()) {
        return std::nullopt;
    }
    if (service->second.identity.name == U"is_map_key" && call->arguments.size() == 2) {
        return std::pair{call->arguments[1], lattice_.category("map")};
    }
    const auto category = named_category(expression, service->second);
    return category ? std::optional{std::pair{call->arguments[0], *category}} : std::nullopt;
}

// Builds the shape of the values a pattern matches.
class Shape final {
  public:
    explicit Shape(BindingFacts &bindings)
        : bindings_(bindings), syntax_(*bindings.function.module->syntax), lattice_(bindings.inference.graph) {}

    // The shape of a pattern `depth` levels deep.
    Id of(const ast::ExprId &id, std::size_t depth);

  private:
    // Groups, aliases, tuples, lists and records; none for other patterns.
    std::optional<Id> composite(const ast::ExprValue &value, std::size_t depth);
    // Maps, bitstrings, variables and literals.
    Id leaf(const ast::Expression &expression);
    Id tuple(const ast::Tuple &value, std::size_t depth);
    Id list(const ast::List &value, std::size_t depth);
    Id record(const ast::RecordExpression &value, std::size_t depth);
    // A variable: a bound one's fact, any term for a new one.
    Id variable(const ast::Expression &expression);

    BindingFacts &bindings_;
    const ast::Module &syntax_;
    Lattice lattice_;
};

Id Shape::of(const ast::ExprId &id, const std::size_t depth) {
    const auto &expression = syntax_.expression(id);
    if (depth > SHAPE_DEPTH) {
        return lattice_.graph().top();
    }
    if (const auto shape = composite(expression.value, depth)) {
        return *shape;
    }
    return leaf(expression);
}

std::optional<Id> Shape::composite(const ast::ExprValue &value, const std::size_t depth) {
    if (const auto *group = std::get_if<ast::Group>(&value)) {
        return of(group->expression, depth);
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&value)) {
        return lattice_.meet(of(match->left, depth), of(match->right, depth));
    }
    if (const auto *tuple_value = std::get_if<ast::Tuple>(&value)) {
        return tuple(*tuple_value, depth);
    }
    if (const auto *list_value = std::get_if<ast::List>(&value)) {
        return list(*list_value, depth);
    }
    if (const auto *record_value = std::get_if<ast::RecordExpression>(&value)) {
        return record(*record_value, depth);
    }
    return std::nullopt;
}

Id Shape::leaf(const ast::Expression &expression) {
    const auto &value = expression.value;
    if (std::holds_alternative<ast::MapExpression>(value) || std::holds_alternative<ast::Bitstring>(value)) {
        return lattice_.category(std::holds_alternative<ast::MapExpression>(value) ? "map" : "bitstring");
    }
    if (std::holds_alternative<ast::Variable>(value)) {
        return variable(expression);
    }
    const bool literal =
        std::holds_alternative<ast::Atom>(value) || std::holds_alternative<ast::IntegerLiteral>(value) ||
        std::holds_alternative<ast::CharacterLiteral>(value) || std::holds_alternative<ast::FloatLiteral>(value) ||
        std::holds_alternative<ast::StringLiteral>(value);
    const auto fact = literal ? constructed_fact(bindings_.inference, bindings_.function, value) : std::nullopt;
    return fact.value_or(lattice_.graph().top());
}

Id Shape::tuple(const ast::Tuple &value, const std::size_t depth) {
    std::vector<Id> elements;
    elements.reserve(value.elements.size());
    for (const auto &element : value.elements) {
        elements.push_back(of(element, depth + 1));
    }
    return lattice_.tuple(std::move(elements));
}

Id Shape::list(const ast::List &value, const std::size_t depth) {
    if (value.elements.empty()) {
        return value.tail ? of(*value.tail, depth) : lattice_.nil();
    }
    std::vector<Id> elements;
    elements.reserve(value.elements.size());
    for (const auto &element : value.elements) {
        elements.push_back(of(element, depth + 1));
    }
    return cons(lattice_, {lattice_.join(elements, 0), value.tail ? of(*value.tail, depth + 1) : lattice_.nil()});
}

Id Shape::record(const ast::RecordExpression &value, const std::size_t depth) {
    const auto *layout = record_layout(*bindings_.function.module, value.identity);
    if (!layout || layout->native) {
        return lattice_.graph().top();
    }
    std::vector<Id> elements{lattice_.atom(utf8(layout->name.name))};
    for (const auto &field : record_values(*bindings_.function.module, value, true)) {
        elements.push_back(field ? of(*field, depth + 1) : lattice_.graph().top());
    }
    return lattice_.tuple(std::move(elements));
}

Id Shape::variable(const ast::Expression &expression) {
    const auto event = bindings_.events.find(&expression);
    if (event == bindings_.events.end() || event->second->use == BindingUse::definition) {
        return lattice_.graph().top();
    }
    // A bound variable or a repeated one matches exactly the value it holds.
    const auto found = bindings_.values.find(event->second->identity);
    return found == bindings_.values.end() ? lattice_.graph().top() : found->second.type;
}
} // namespace

std::optional<BindingId> variable(const BindingFacts &bindings, const ast::ExprId &expression) {
    const auto &syntax = *bindings.function.module->syntax;
    const auto event = bindings.events.find(&syntax.expression(ungroup(syntax, expression)));
    return event == bindings.events.end() ? std::nullopt : std::optional{event->second->identity};
}

bool assume(BindingFacts &bindings, const ast::ExprId &test) { return Assume(bindings).test(test, bindings.values); }

bool assume_guard(BindingFacts &bindings, const ast::GuardSyntax &guard) {
    Assume assumption(bindings);
    std::optional<Values> result;
    bool possible = false;
    for (const auto &alternative : guard.alternatives) {
        auto values = bindings.values;
        if (!assumption.conjunction(alternative.tests, values)) {
            continue;
        }
        possible = true;
        if (!result) {
            result = std::move(values);
            continue;
        }
        Lattice lattice(bindings.inference.graph);
        merge(lattice, *result, values);
    }
    if (result) {
        bindings.values = std::move(*result);
    }
    return possible;
}

Id pattern_shape(BindingFacts &bindings, const ast::ExprId &pattern) { return Shape(bindings).of(pattern, 0); }

std::optional<std::pair<BindingId, Id>> single_test(BindingFacts &bindings, const ast::GuardSyntax &guard) {
    if (guard.alternatives.size() != 1 || guard.alternatives.front().tests.size() != 1) {
        return std::nullopt;
    }
    const auto &syntax = *bindings.function.module->syntax;
    const auto &test = syntax.expression(ungroup(syntax, guard.alternatives.front().tests.front()));
    const auto tested = Assume(bindings).type_test(test);
    const auto identity = tested ? variable(bindings, tested->first) : std::nullopt;
    return identity ? std::optional{std::pair{*identity, tested->second}} : std::nullopt;
}
} // namespace clause::semantic::types
