#include "inference_uses.hpp"
#include "../../parsing/operator_info.hpp"
#include "../binary_options.hpp"
#include "../capabilities.hpp"
#include "../funs.hpp"
#include "../records.hpp"
#include "lattice.hpp"

namespace clause::semantic::types {
namespace {
// An operand and the category an operation requires of it.
using Requirement = std::pair<ast::ExprId, Id>;

// The category each builtin requires of its arguments, by name/arity; an empty name requires nothing.
const std::map<std::pair<std::u32string, std::size_t>, std::vector<std::string_view>> &builtin_requirements() {
    static const std::map<std::pair<std::u32string, std::size_t>, std::vector<std::string_view>> TABLE{
        {{U"abs", 1}, {"number"}},
        {{U"float", 1}, {"number"}},
        {{U"trunc", 1}, {"number"}},
        {{U"round", 1}, {"number"}},
        {{U"floor", 1}, {"number"}},
        {{U"ceil", 1}, {"number"}},
        {{U"length", 1}, {"list"}},
        {{U"hd", 1}, {"nonempty_maybe_improper_list"}},
        {{U"tl", 1}, {"nonempty_maybe_improper_list"}},
        {{U"tuple_size", 1}, {"tuple"}},
        {{U"element", 2}, {"pos_integer", "tuple"}},
        {{U"setelement", 3}, {"pos_integer", "tuple", ""}},
        {{U"map_size", 1}, {"map"}},
        {{U"map_get", 2}, {"", "map"}},
        {{U"is_map_key", 2}, {"", "map"}},
        {{U"byte_size", 1}, {"bitstring"}},
        {{U"bit_size", 1}, {"bitstring"}},
        {{U"atom_to_list", 1}, {"atom"}},
        {{U"list_to_atom", 1}, {"string"}},
        {{U"integer_to_list", 1}, {"integer"}},
        {{U"integer_to_list", 2}, {"integer", "pos_integer"}},
        {{U"list_to_integer", 1}, {"string"}},
        {{U"tuple_to_list", 1}, {"tuple"}},
        {{U"list_to_tuple", 1}, {"list"}},
        {{U"binary_to_list", 1}, {"binary"}},
        {{U"make_tuple", 2}, {"non_neg_integer", ""}},
        {{U"spawn", 3}, {"atom", "atom", "list"}},
        {{U"register", 2}, {"atom", ""}},
        {{U"unregister", 1}, {"atom"}},
        {{U"whereis", 1}, {"atom"}}};
    return TABLE;
}

// The category an operator requires of its operands, by spelling: numbers for arithmetic, integers for integer
// and bit operators, booleans for logical ones; empty for others.
std::string_view operator_requirement(const std::u32string_view name) {
    static const std::map<std::u32string_view, std::string_view> TABLE{
        {U"+", "number"},     {U"-", "number"},    {U"*", "number"},     {U"/", "number"},
        {U"div", "integer"},  {U"rem", "integer"}, {U"band", "integer"}, {U"bor", "integer"},
        {U"bxor", "integer"}, {U"bsl", "integer"}, {U"bsr", "integer"},  {U"bnot", "integer"},
        {U"and", "boolean"},  {U"or", "boolean"},  {U"xor", "boolean"},  {U"not", "boolean"}};
    const auto found = TABLE.find(name);
    return found == TABLE.end() ? std::string_view() : found->second;
}

// Collects the requirements an expression puts on its operands.
class Uses final {
  public:
    explicit Uses(BindingFacts &bindings)
        : bindings_(bindings), syntax_(*bindings.function.module->syntax), lattice_(bindings.inference.graph) {}

    std::vector<Requirement> of(const ast::Expression &expression);

  private:
    void binary(const ast::BinaryExpression &value);
    void call(const ast::Expression &expression, const ast::CallExpression &value);
    void builtin(const ServiceResolution &service, const std::vector<ast::ExprId> &arguments);
    void record(const ast::RecordIdentity &identity, const ast::ExprId &base);
    void segments(const ast::Bitstring &value);

    // Require `category` (a built-in name; none when empty) of an operand.
    void require(const ast::ExprId &operand, std::string_view category) {
        if (!category.empty()) {
            found_.emplace_back(operand, lattice_.category(category));
        }
    }

    BindingFacts &bindings_;
    const ast::Module &syntax_;
    Lattice lattice_;
    std::vector<Requirement> found_;
};

std::vector<Requirement> Uses::of(const ast::Expression &expression) {
    const auto &value = expression.value;
    if (const auto *binary_value = std::get_if<ast::BinaryExpression>(&value)) {
        binary(*binary_value);
    } else if (const auto *unary = std::get_if<ast::UnaryExpression>(&value)) {
        require(unary->operand, operator_requirement(operator_spelling(unary->operation)));
    } else if (const auto *call_value = std::get_if<ast::CallExpression>(&value)) {
        call(expression, *call_value);
    } else if (const auto *map = std::get_if<ast::MapExpression>(&value); map && map->base) {
        require(*map->base, "map");
    } else if (const auto *access = std::get_if<ast::RecordAccess>(&value)) {
        record(access->identity, access->base);
    } else if (const auto *update = std::get_if<ast::RecordExpression>(&value); update && update->base) {
        record(update->identity, *update->base);
    } else if (const auto *bits = std::get_if<ast::Bitstring>(&value)) {
        segments(*bits);
    }
    return std::move(found_);
}

void Uses::binary(const ast::BinaryExpression &value) {
    using Op = ast::BinaryOperator;
    if (value.operation == Op::and_also || value.operation == Op::or_else) {
        // Only the left operand always runs; it must be a boolean.
        require(value.left, "boolean");
        return;
    }
    if (value.operation == Op::append || value.operation == Op::subtract_list) {
        require(value.left, "list");
        require(value.right, value.operation == Op::subtract_list ? "list" : "");
        return;
    }
    const auto category = operator_requirement(operator_spelling(value.operation));
    require(value.left, category);
    require(value.right, category);
}

void Uses::call(const ast::Expression &expression, const ast::CallExpression &value) {
    if (fun_call(syntax_, value)) {
        found_.emplace_back(value.target, lattice_.fun(value.arguments.size(), lattice_.graph().top()));
        return;
    }
    if (dynamic_call(syntax_, value)) {
        const auto &remote = std::get<ast::RemoteExpression>(syntax_.expression(ungroup(syntax_, value.target)).value);
        require(remote.module, "atom");
        require(remote.function, "atom");
        return;
    }
    const auto &services = bindings_.function.function->services;
    if (const auto service = services.find(&expression); service != services.end()) {
        builtin(service->second, value.arguments);
    }
}

void Uses::builtin(const ServiceResolution &service, const std::vector<ast::ExprId> &arguments) {
    if (const auto category = operator_requirement(service.identity.name); !category.empty()) {
        for (const auto &argument : arguments) {
            require(argument, category);
        }
        return;
    }
    if (service.identity.name == U"spawn" && arguments.size() == 1) {
        found_.emplace_back(arguments[0], lattice_.fun(0, lattice_.graph().top()));
        return;
    }
    const auto found = builtin_requirements().find({service.identity.name, arguments.size()});
    for (std::size_t index = 0; found != builtin_requirements().end() && index < arguments.size(); ++index) {
        require(arguments[index], found->second.at(index));
    }
}

void Uses::record(const ast::RecordIdentity &identity, const ast::ExprId &base) {
    const auto *layout = record_layout(*bindings_.function.module, identity);
    if (!layout || layout->native) {
        return;
    }
    std::vector<Id> elements(layout->fields.size() + 1, lattice_.graph().top());
    elements.front() = lattice_.atom(utf8(layout->name.name));
    found_.emplace_back(base, lattice_.tuple(std::move(elements)));
}

void Uses::segments(const ast::Bitstring &value) {
    using Type = abi::v1::BitType;
    static const std::map<Type, std::string_view> VALUES{{Type::integer, "integer"},
                                                         {Type::floating, "number"},
                                                         {Type::utf8, "non_neg_integer"},
                                                         {Type::utf16, "non_neg_integer"},
                                                         {Type::utf32, "non_neg_integer"}};
    for (const auto &segment : value.segments) {
        const auto options = binary_options(segment);
        const auto found = VALUES.find(options.type);
        const auto binary = options.unit % 8 == 0 ? "binary" : "bitstring";
        require(segment.value, found == VALUES.end() ? binary : found->second);
        if (segment.size) {
            require(*segment.size, "non_neg_integer");
        }
    }
}
} // namespace

void narrow_uses(BindingFacts &bindings, const ast::Expression &expression) {
    for (const auto &[operand, category] : Uses(bindings).of(expression)) {
        bindings.narrow(operand, category);
    }
}
} // namespace clause::semantic::types
