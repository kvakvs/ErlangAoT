#include "inference_operators.hpp"
#include "../../parsing/operator_info.hpp"
#include "../../preprocessor/expression.hpp"
#include "inference_containers.hpp"
#include "lattice.hpp"
#include <clause/abi/builtins.hpp>
#include <set>

namespace clause::semantic::types {
namespace {
// Integers folded exactly have at most this many bits; larger operands or results become integer().
constexpr std::size_t FOLD_BITS = 4096;
// Shift amounts of a folded bsl or bsr have at most this many bits.
constexpr std::size_t SHIFT_BITS = 12;

// The bits of an integer's magnitude.
std::size_t size_bits(const BigInt &value) {
    return value == 0 ? 0 : boost::multiprecision::msb(boost::multiprecision::abs(value)) + 1;
}

// An integer interval of exact bounds; a missing bound is unbounded.
struct Range {
    std::optional<BigInt> low;
    std::optional<BigInt> high;
};

std::optional<BigInt> parsed(const std::optional<std::string> &decimal) {
    return decimal ? std::optional{decimal_number(*decimal)} : std::nullopt;
}

std::optional<std::string> printed(const std::optional<BigInt> &value) {
    return value ? std::optional{decimal_integer(*value)} : std::nullopt;
}

// The integers a fact's numbers hold, as a range.
Range range(const Numbers &numbers) { return {parsed(numbers.low), parsed(numbers.high)}; }

// Whether a range holds one integer.
bool singleton(const Range &range) { return range.low && range.high && *range.low == *range.high; }

// Whether every integer of a range is zero or more.
bool non_negative(const Range &range) { return range.low && *range.low >= 0; }

// `operation` of two bounds that are both known; unbounded otherwise.
template <typename Operation>
std::optional<BigInt> combine(const std::optional<BigInt> &left, const std::optional<BigInt> &right,
                              Operation operation) {
    return left && right ? std::optional{operation(*left, *right)} : std::nullopt;
}

// Whether a fact's numbers include any number.
bool numeric(const Numbers &numbers) { return numbers.integers || numbers.floats; }

// The atom results of an operation on booleans, by the truth values that give them.
std::vector<bool> truths(Lattice &lattice, const Id fact) {
    std::vector<bool> result;
    if (lattice.holds_atom(fact, "false")) {
        result.push_back(false);
    }
    if (lattice.holds_atom(fact, "true")) {
        result.push_back(true);
    }
    return result;
}

// The two operands of a binary operator, and their integers as ranges.
struct Operands {
    Id left;
    Id right;
};

struct Ranges {
    Range left;
    Range right;
};

// and, or or xor of two truth values.
bool logic(const std::u32string_view name, const bool x, const bool y) {
    if (name == U"and") {
        return x && y;
    }
    return name == U"or" ? x || y : x != y;
}

// Which orders two facts' values can compare in: less, equal, greater.
struct Outcomes {
    bool less = true;
    bool equal = true;
    bool greater = true;
};

// Whether a fact holds only integers, or only numbers.
bool integers_only(const Numbers &numbers) { return numbers.integers && !numbers.floats && !numbers.others; }

bool numbers_only(const Numbers &numbers) { return numeric(numbers) && !numbers.others; }

// Whether bound `first` can be below bound `second` (a missing bound can be anything), and whether it surely is.
bool may_be_below(const std::optional<BigInt> &first, const std::optional<BigInt> &second) {
    return !first || !second || *first < *second;
}

bool surely_below(const std::optional<BigInt> &first, const std::optional<BigInt> &second) {
    return first && second && *first < *second;
}

// The orders integers of two ranges can compare in.
Outcomes range_order(const Range &left, const Range &right) {
    return {may_be_below(left.low, right.high),
            !surely_below(left.high, right.low) && !surely_below(right.high, left.low),
            may_be_below(right.low, left.high)};
}

// Operators, builtins and their result facts, by operand facts.
class Operations final {
  public:
    explicit Operations(Graph &graph) : graph_(graph), lattice_(graph) {}

    // An operator by its spelling, with one or two operands.
    Id unary(std::u32string_view name, Id operand);
    Id binary(std::u32string_view name, Id left, Id right);
    // andalso (`conjunction`) or orelse: the right operand runs only when the left one does not decide.
    Id short_circuit(bool conjunction, const Operands &operands);
    // A builtin call by its module:name/arity.
    Id builtin(const std::string &signature, std::span<const Id> arguments);

    using Unary = Id (Operations::*)(Id);
    using Binary = Id (Operations::*)(std::u32string_view, const Operands &);
    using Builtin = Id (Operations::*)(std::span<const Id>);
    using Bits = Id (Operations::*)(const Ranges &);

  private:
    Id arithmetic(std::u32string_view name, const Operands &operands);
    Id divide(std::u32string_view name, const Operands &operands);
    Id bitwise(std::u32string_view name, const Operands &operands);
    Id compare(std::u32string_view name, const Operands &operands);
    Id logical(std::u32string_view name, const Operands &operands);
    Id second(std::u32string_view name, const Operands &operands);
    Id concatenate(std::u32string_view name, const Operands &operands);
    Id negate(Id operand);
    Id plus(Id operand);
    Id complement(Id operand);
    Id negation(Id operand);
    Id absolute(std::span<const Id> arguments);
    Id rounded(std::span<const Id> arguments);
    Id to_float(std::span<const Id> arguments);
    Id either(std::span<const Id> arguments);
    Id message(std::span<const Id> arguments);

    Id list_head(std::span<const Id> arguments) { return head(lattice_, arguments[0]); }

    Id list_tail(std::span<const Id> arguments) { return tail(lattice_, arguments[0]); }

    Id tuple_at(std::span<const Id> arguments) { return element(lattice_, {arguments[0], arguments[1]}); }

    Id tuple_set(std::span<const Id> arguments) {
        return set_element(lattice_, {arguments[0], arguments[1]}, arguments[2]);
    }

    Id tuple_elements(std::span<const Id> arguments) { return tuple_list(lattice_, arguments[0]); }

    Id map_lookup(std::span<const Id> arguments) { return map_value(lattice_, {arguments[1], arguments[0]}); }

    Id list_append(std::span<const Id> arguments) { return append(lattice_, {arguments[0], arguments[1]}); }

    Id list_subtract(std::span<const Id> arguments) { return subtract(lattice_, arguments[0]); }

    // Integer results of +, - and * on ranges: folded singletons, else interval arithmetic.
    Id integer_arithmetic(std::u32string_view name, const Range &left, const Range &right);
    // Integer results of div, rem, band, bor, bxor, bsl and bsr on ranges that are not both singletons.
    Id integer_bits(std::u32string_view name, const Range &left, const Range &right);
    Id bit_and(const Ranges &ranges);
    Id remainder(const Ranges &ranges);
    Id quotient(const Ranges &ranges);
    Id shift_right(const Ranges &ranges);
    Id shift_left(const Ranges &ranges);
    Id bit_or(const Ranges &ranges);
    // `name` applied to two integers; none() when it raises, integer() past the integer limit.
    Id fold(std::u32string_view name, const BigInt &left, const BigInt &right);
    // The orders values of the two facts can compare in.
    Outcomes outcomes(const Operands &operands);

    // The fact of a range, and the atoms of a set of truth values.
    Id integers(const Range &range) { return lattice_.interval(printed(range.low), printed(range.high)); }

    Id booleans(const std::set<bool> &values);
    // The numbers of a fact: its integers and floats, without other values.
    Id number_part(const Numbers &numbers);

    Graph &graph_;
    Lattice lattice_;
};

Id Operations::unary(const std::u32string_view name, const Id operand) {
    static const std::map<std::u32string_view, Unary> RULES{{U"-", &Operations::negate},
                                                            {U"+", &Operations::plus},
                                                            {U"bnot", &Operations::complement},
                                                            {U"not", &Operations::negation}};
    const auto found = RULES.find(name);
    return found == RULES.end() ? graph_.top() : (this->*found->second)(operand);
}

Id Operations::binary(const std::u32string_view name, const Id left, const Id right) {
    static const std::map<std::u32string_view, Binary> RULES{
        {U"+", &Operations::arithmetic},  {U"-", &Operations::arithmetic}, {U"*", &Operations::arithmetic},
        {U"/", &Operations::divide},      {U"div", &Operations::bitwise},  {U"rem", &Operations::bitwise},
        {U"band", &Operations::bitwise},  {U"bor", &Operations::bitwise},  {U"bxor", &Operations::bitwise},
        {U"bsl", &Operations::bitwise},   {U"bsr", &Operations::bitwise},  {U"==", &Operations::compare},
        {U"/=", &Operations::compare},    {U"=:=", &Operations::compare},  {U"=/=", &Operations::compare},
        {U"<", &Operations::compare},     {U"=<", &Operations::compare},   {U">", &Operations::compare},
        {U">=", &Operations::compare},    {U"and", &Operations::logical},  {U"or", &Operations::logical},
        {U"xor", &Operations::logical},   {U"!", &Operations::second},     {U"++", &Operations::concatenate},
        {U"--", &Operations::concatenate}};
    const auto found = RULES.find(name);
    return found == RULES.end() ? graph_.top() : (this->*found->second)(name, {left, right});
}

Id Operations::arithmetic(const std::u32string_view name, const Operands &operands) {
    const auto [left, right] = operands;
    const auto a = lattice_.numbers(left);
    const auto b = lattice_.numbers(right);
    if (!numeric(a) || !numeric(b)) {
        return graph_.bottom();
    }
    std::vector<Id> results;
    if (a.integers && b.integers) {
        results.push_back(integer_arithmetic(name, range(a), range(b)));
    }
    if (a.floats || b.floats) {
        results.push_back(lattice_.category("float"));
    }
    return lattice_.join(results, 0);
}

Id Operations::integer_arithmetic(const std::u32string_view name, const Range &left, const Range &right) {
    if (singleton(left) && singleton(right)) {
        return fold(name, *left.low, *right.low);
    }
    if (name == U"+") {
        return integers({combine(left.low, right.low, std::plus<>()), combine(left.high, right.high, std::plus<>())});
    }
    if (name == U"-") {
        return integers({combine(left.low, right.high, std::minus<>()), combine(left.high, right.low, std::minus<>())});
    }
    if (non_negative(left) && non_negative(right)) {
        return integers({*left.low * *right.low, combine(left.high, right.high, std::multiplies<>())});
    }
    return lattice_.category("integer");
}

Id Operations::divide(std::u32string_view, const Operands &operands) {
    const auto [left, right] = operands;
    const auto a = lattice_.numbers(left);
    const auto b = lattice_.numbers(right);
    const auto divisor = range(b);
    const bool zero = !b.floats && singleton(divisor) && *divisor.low == 0;
    return !numeric(a) || !numeric(b) || zero ? graph_.bottom() : lattice_.category("float");
}

Id Operations::bitwise(const std::u32string_view name, const Operands &operands) {
    const auto [left, right] = operands;
    const auto a = lattice_.numbers(left);
    const auto b = lattice_.numbers(right);
    if (!a.integers || !b.integers) {
        return graph_.bottom();
    }
    const auto x = range(a);
    const auto y = range(b);
    return singleton(x) && singleton(y) ? fold(name, *x.low, *y.low) : integer_bits(name, x, y);
}

Id Operations::integer_bits(const std::u32string_view name, const Range &left, const Range &right) {
    static const std::map<std::u32string_view, Bits> RULES{
        {U"band", &Operations::bit_and},    {U"rem", &Operations::remainder},  {U"div", &Operations::quotient},
        {U"bsr", &Operations::shift_right}, {U"bsl", &Operations::shift_left}, {U"bor", &Operations::bit_or},
        {U"bxor", &Operations::bit_or}};
    return (this->*RULES.at(name))({left, right});
}

Id Operations::bit_and(const Ranges &ranges) {
    const auto &[left, right] = ranges;
    if (!non_negative(left) && !non_negative(right)) {
        return lattice_.category("integer");
    }
    // A nonnegative operand bounds the result from 0 to its largest value.
    std::optional<BigInt> high;
    for (const auto *operand : {&left, &right}) {
        if (non_negative(*operand) && operand->high) {
            high = high ? std::min(*high, *operand->high) : *operand->high;
        }
    }
    return integers({BigInt(0), high});
}

Id Operations::remainder(const Ranges &ranges) {
    const auto &[left, right] = ranges;
    if (!right.low || !right.high) {
        return non_negative(left) ? lattice_.category("non_neg_integer") : lattice_.category("integer");
    }
    // The result has the dividend's sign and is smaller than the divisor in magnitude.
    const auto largest = std::max(abs(*right.low), abs(*right.high)) - 1;
    return largest < 0 ? graph_.bottom() : integers({non_negative(left) ? BigInt(0) : BigInt(-largest), largest});
}

Id Operations::quotient(const Ranges &ranges) {
    const auto &[left, right] = ranges;
    return non_negative(left) && non_negative(right) ? integers({BigInt(0), left.high}) : lattice_.category("integer");
}

Id Operations::shift_right(const Ranges &ranges) {
    const auto &[left, right] = ranges;
    if (!non_negative(left)) {
        return lattice_.category("integer");
    }
    return non_negative(right) ? integers({BigInt(0), left.high}) : lattice_.category("non_neg_integer");
}

Id Operations::shift_left(const Ranges &ranges) {
    const auto &left = ranges.left;
    return lattice_.category(non_negative(left) ? "non_neg_integer" : "integer");
}

Id Operations::bit_or(const Ranges &ranges) {
    const auto &[left, right] = ranges;
    return lattice_.category(non_negative(left) && non_negative(right) ? "non_neg_integer" : "integer");
}

Id Operations::fold(const std::u32string_view name, const BigInt &left, const BigInt &right) {
    const bool shift = name == U"bsl" || name == U"bsr";
    if (size_bits(left) > FOLD_BITS || size_bits(right) > (shift ? SHIFT_BITS : FOLD_BITS)) {
        return lattice_.category("integer");
    }
    try {
        const auto result = integral(evaluate_operator(name, {integer(left), integer(right)}));
        return size_bits(result) > FOLD_BITS ? lattice_.category("integer") : lattice_.integer(decimal_integer(result));
    } catch (const EvaluationFailure &) {
        return graph_.bottom();
    } catch (const EvaluationLimit &) {
        return lattice_.category("integer");
    }
}

Outcomes Operations::outcomes(const Operands &operands) {
    const auto [left, right] = operands;
    const auto &a = graph_.get(left);
    const auto &b = graph_.get(right);
    if (a.kind == Kind::atom && b.kind == Kind::atom) {
        return {a.name<b.name, a.name == b.name, a.name> b.name};
    }
    const auto x = lattice_.numbers(left);
    const auto y = lattice_.numbers(right);
    if (integers_only(x) && integers_only(y)) {
        return range_order(range(x), range(y));
    }
    // Numbers compare below atoms and every other value.
    if (numbers_only(x) && !numeric(y)) {
        return {true, false, false};
    }
    return numbers_only(y) && !numeric(x) ? Outcomes{false, false, true} : Outcomes{};
}

Id Operations::compare(const std::u32string_view name, const Operands &operands) {

    static const std::map<std::u32string_view, std::array<bool, 3>> HOLDS{
        {U"==", {false, true, false}}, {U"=:=", {false, true, false}}, {U"/=", {true, false, true}},
        {U"=/=", {true, false, true}}, {U"<", {true, false, false}},   {U"=<", {true, true, false}},
        {U">", {false, false, true}},  {U">=", {false, true, true}}};
    const auto order = outcomes(operands);
    const auto &holds = HOLDS.at(name);
    std::set<bool> results;
    for (const auto [possible, index] : {std::pair{order.less, 0}, {order.equal, 1}, {order.greater, 2}}) {
        if (possible) {
            results.insert(holds.at(index));
        }
    }
    return booleans(results);
}

Id Operations::logical(const std::u32string_view name, const Operands &operands) {
    std::set<bool> results;
    for (const bool x : truths(lattice_, operands.left)) {
        for (const bool y : truths(lattice_, operands.right)) {
            results.insert(logic(name, x, y));
        }
    }
    return booleans(results);
}

Id Operations::short_circuit(const bool conjunction, const Operands &operands) {
    const auto [left, right] = operands;
    std::vector<Id> results;
    for (const bool value : truths(lattice_, left)) {
        // andalso decides on false, orelse on true; otherwise the result is the right operand.
        results.push_back(value != conjunction ? lattice_.atom(value ? "true" : "false") : right);
    }
    return lattice_.join(results, 0);
}

Id Operations::second(std::u32string_view, const Operands &operands) { return operands.right; }

Id Operations::concatenate(const std::u32string_view name, const Operands &operands) {
    return name == U"++" ? append(lattice_, {operands.left, operands.right}) : subtract(lattice_, operands.left);
}

Id Operations::negate(const Id operand) {
    const auto numbers = lattice_.numbers(operand);
    std::vector<Id> results;
    if (numbers.integers) {
        const auto negated = [](const std::optional<BigInt> &bound) {
            return bound ? std::optional{BigInt(-*bound)} : std::nullopt;
        };
        const auto bounds = range(numbers);
        results.push_back(integers({negated(bounds.high), negated(bounds.low)}));
    }
    if (numbers.floats) {
        results.push_back(lattice_.category("float"));
    }
    return lattice_.join(results, 0);
}

Id Operations::plus(const Id operand) { return number_part(lattice_.numbers(operand)); }

Id Operations::complement(const Id operand) {
    const auto numbers = lattice_.numbers(operand);
    if (!numbers.integers) {
        return graph_.bottom();
    }
    // bnot X is -X - 1.
    const auto complemented = [](const std::optional<BigInt> &bound) {
        return bound ? std::optional{BigInt(-*bound - 1)} : std::nullopt;
    };
    const auto bounds = range(numbers);
    return integers({complemented(bounds.high), complemented(bounds.low)});
}

Id Operations::negation(const Id operand) {
    std::set<bool> results;
    for (const bool value : truths(lattice_, operand)) {
        results.insert(!value);
    }
    return booleans(results);
}

Id Operations::booleans(const std::set<bool> &values) {
    std::vector<Id> atoms;
    atoms.reserve(values.size());
    for (const bool value : values) {
        atoms.push_back(lattice_.atom(value ? "true" : "false"));
    }
    return lattice_.join(atoms, 0);
}

Id Operations::number_part(const Numbers &numbers) {
    std::vector<Id> results;
    if (numbers.integers) {
        results.push_back(lattice_.interval(numbers.low, numbers.high));
    }
    if (numbers.floats) {
        results.push_back(lattice_.category("float"));
    }
    return lattice_.join(results, 0);
}

Id Operations::absolute(const std::span<const Id> arguments) {
    const auto numbers = lattice_.numbers(arguments[0]);
    std::vector<Id> results;
    if (numbers.integers) {
        const auto bounds = range(numbers);
        const auto magnitude = combine(bounds.low, bounds.high, [](const BigInt &low, const BigInt &high) {
            return std::max(abs(low), abs(high));
        });
        const auto low = non_negative(bounds) ? *bounds.low : BigInt(0);
        const bool negative = bounds.high && *bounds.high <= 0;
        results.push_back(negative ? negate(integers(bounds)) : integers({low, magnitude}));
    }
    if (numbers.floats) {
        results.push_back(lattice_.category("float"));
    }
    return lattice_.join(results, 0);
}

Id Operations::rounded(const std::span<const Id> arguments) {
    const auto numbers = lattice_.numbers(arguments[0]);
    std::vector<Id> results;
    if (numbers.integers) {
        results.push_back(lattice_.interval(numbers.low, numbers.high));
    }
    if (numbers.floats) {
        results.push_back(lattice_.category("integer"));
    }
    return lattice_.join(results, 0);
}

Id Operations::to_float(const std::span<const Id> arguments) {
    return numeric(lattice_.numbers(arguments[0])) ? lattice_.category("float") : graph_.bottom();
}

Id Operations::either(const std::span<const Id> arguments) { return lattice_.join(arguments, 0); }

Id Operations::message(const std::span<const Id> arguments) { return arguments.back(); }

// The category each builtin returns, by module:name/arity.
const std::map<std::string, std::string_view, std::less<>> &categories() {
    static const std::map<std::string, std::string_view, std::less<>> TABLE{
        {"erlang:self/0", "pid"},
        {"erlang:spawn/1", "pid"},
        {"erlang:spawn/3", "pid"},
        {"erlang:spawn_link/1", "pid"},
        {"erlang:spawn_link/3", "pid"},
        {"erlang:make_ref/0", "reference"},
        {"erlang:monitor/2", "reference"},
        {"erlang:open_port/2", "port"},
        {"erlang:list_to_port/1", "port"},
        {"erlang:length/1", "non_neg_integer"},
        {"erlang:byte_size/1", "non_neg_integer"},
        {"erlang:bit_size/1", "non_neg_integer"},
        {"erlang:tuple_size/1", "non_neg_integer"},
        {"erlang:map_size/1", "non_neg_integer"},
        {"erlang:size/1", "non_neg_integer"},
        {"erlang:list_to_integer/1", "integer"},
        {"erlang:list_to_integer/2", "integer"},
        {"erlang:list_to_atom/1", "atom"},
        {"erlang:node/0", "atom"},
        {"erlang:node/1", "atom"},
        {"erlang:atom_to_list/1", "string"},
        {"erlang:integer_to_list/1", "nonempty_string"},
        {"erlang:integer_to_list/2", "nonempty_string"},
        {"erlang:float_to_list/1", "nonempty_string"},
        {"erlang:float_to_list/2", "nonempty_string"},
        {"erlang:pid_to_list/1", "nonempty_string"},
        {"erlang:ref_to_list/1", "nonempty_string"},
        {"erlang:port_to_list/1", "nonempty_string"},
        {"erlang:list_to_binary/1", "binary"},
        {"erlang:iolist_to_binary/1", "binary"},
        {"erlang:binary_part/2", "binary"},
        {"erlang:binary_part/3", "binary"},
        {"erlang:list_to_tuple/1", "tuple"},
        {"erlang:make_tuple/2", "tuple"},
        {"erlang:make_tuple/3", "tuple"},
        {"erlang:is_process_alive/1", "boolean"},
        {"erlang:function_exported/3", "boolean"},
        {"erlang:demonitor/2", "boolean"},
        {"erlang:port_command/3", "boolean"},
        {"erlang:is_map_key/2", "boolean"},
        {"erlang:is_record/1", "boolean"},
    };
    return TABLE;
}

// The atom each builtin returns, by module:name/arity; none() for those that never return.
const std::map<std::string, std::string_view, std::less<>> &atoms() {
    static const std::map<std::string, std::string_view, std::less<>> TABLE{{"erlang:display/1", "true"},
                                                                            {"erlang:link/1", "true"},
                                                                            {"erlang:unlink/1", "true"},
                                                                            {"erlang:exit/2", "true"},
                                                                            {"erlang:register/2", "true"},
                                                                            {"erlang:unregister/1", "true"},
                                                                            {"erlang:demonitor/1", "true"},
                                                                            {"erlang:port_close/1", "true"},
                                                                            {"erlang:port_command/2", "true"},
                                                                            {"erlang:port_connect/2", "true"},
                                                                            {"erlang:raise/3", "badarg"},
                                                                            {"io:format/1", "ok"},
                                                                            {"io:format/2", "ok"},
                                                                            {"io:put_chars/1", "ok"},
                                                                            {"erlang:halt/0", ""},
                                                                            {"erlang:halt/1", ""},
                                                                            {"erlang:error/1", ""},
                                                                            {"erlang:error/2", ""},
                                                                            {"erlang:error/3", ""},
                                                                            {"erlang:exit/1", ""},
                                                                            {"erlang:throw/1", ""}};
    return TABLE;
}

Id Operations::builtin(const std::string &signature, const std::span<const Id> arguments) {
    static const std::map<std::string, Builtin, std::less<>> RULES{
        {"erlang:abs/1", &Operations::absolute},         {"erlang:trunc/1", &Operations::rounded},
        {"erlang:round/1", &Operations::rounded},        {"erlang:floor/1", &Operations::rounded},
        {"erlang:ceil/1", &Operations::rounded},         {"erlang:float/1", &Operations::to_float},
        {"erlang:min/2", &Operations::either},           {"erlang:max/2", &Operations::either},
        {"erlang:send/2", &Operations::message},         {"erlang:hd/1", &Operations::list_head},
        {"erlang:tl/1", &Operations::list_tail},         {"erlang:element/2", &Operations::tuple_at},
        {"erlang:setelement/3", &Operations::tuple_set}, {"erlang:tuple_to_list/1", &Operations::tuple_elements},
        {"erlang:map_get/2", &Operations::map_lookup},   {"erlang:++/2", &Operations::list_append},
        {"erlang:--/2", &Operations::list_subtract}};
    if (const auto rule = RULES.find(signature); rule != RULES.end()) {
        return (this->*rule->second)(arguments);
    }
    if (const auto category = categories().find(signature); category != categories().end()) {
        return lattice_.category(category->second);
    }
    if (const auto atom = atoms().find(signature); atom != atoms().end()) {
        return atom->second.empty() ? graph_.bottom() : lattice_.atom(atom->second);
    }
    return signature.starts_with("erlang:is_") ? lattice_.category("boolean") : graph_.top();
}

// The recorded facts of operands; term() for one that is not evaluated.
std::vector<Id> operand_facts(const Inference &inference, const ast::Module &syntax,
                              const std::vector<ast::ExprId> &operands) {
    std::vector<Id> result;
    result.reserve(operands.size());
    for (const auto &operand : operands) {
        const auto found = inference.expressions.find(&syntax.expression(operand));
        result.push_back(found == inference.expressions.end() ? inference.graph.top() : found->second.type);
    }
    return result;
}

// A builtin call: an operator called as a function, or a builtin by its name.
Id builtin_call(Inference &inference, const ServiceResolution &service, const std::vector<Id> &arguments) {
    Operations operations(inference.graph);
    const auto &name = service.identity.name;
    if (operator_signature(name, arguments.size())) {
        return arguments.size() == 1 ? operations.unary(name, arguments[0])
                                     : operations.binary(name, arguments[0], arguments[1]);
    }
    const auto module = service.builtin ? abi::v1::bridge_builtins.at(*service.builtin).module : "erlang";
    return operations.builtin(std::string(module) + ':' + utf8(name) + '/' + std::to_string(arguments.size()),
                              arguments);
}

// A binary operator, short-circuit ones included.
Id binary_operator(Inference &inference, const ast::Module &syntax, const ast::BinaryExpression &value) {
    const auto facts = operand_facts(inference, syntax, {value.left, value.right});
    Operations operations(inference.graph);
    if (value.operation == ast::BinaryOperator::and_also || value.operation == ast::BinaryOperator::or_else) {
        return operations.short_circuit(value.operation == ast::BinaryOperator::and_also, {facts[0], facts[1]});
    }
    if (std::ranges::contains(facts, inference.graph.bottom())) {
        return inference.graph.bottom();
    }
    return operations.binary(operator_spelling(value.operation), facts[0], facts[1]);
}
} // namespace

std::optional<Id> operation_fact(Inference &inference, const FunctionRef function, const ast::Expression &expression) {
    const auto &syntax = *function.module->syntax;
    if (const auto *value = std::get_if<ast::BinaryExpression>(&expression.value)) {
        return binary_operator(inference, syntax, *value);
    }
    if (const auto *value = std::get_if<ast::UnaryExpression>(&expression.value)) {
        const auto operand = operand_facts(inference, syntax, {value->operand}).front();
        return operand == inference.graph.bottom()
                   ? operand
                   : Operations(inference.graph).unary(operator_spelling(value->operation), operand);
    }
    const auto *call = std::get_if<ast::CallExpression>(&expression.value);
    const auto service = function.function->services.find(&expression);
    if (!call || service == function.function->services.end() || service->second.apply()) {
        return std::nullopt;
    }
    const auto arguments = operand_facts(inference, syntax, call->arguments);
    return std::ranges::contains(arguments, inference.graph.bottom())
               ? inference.graph.bottom()
               : builtin_call(inference, service->second, arguments);
}
} // namespace clause::semantic::types
