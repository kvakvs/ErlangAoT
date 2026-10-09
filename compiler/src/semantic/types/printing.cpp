#include "printing.hpp"
#include "../../preprocessor/value.hpp"
#include <algorithm>
#include <clause/compiler/printing.hpp>
#include <span>

namespace clause::semantic::types {
namespace {
// Deeper nesting than this prints as `...`, like an exhausted budget.
constexpr std::size_t MAX_DEPTH = 32;

class Printer final {
  public:
    Printer(const Graph &graph, std::size_t budget, const RecordFields *records)
        : graph_(graph), budget_(budget), records_(records) {}

    // The text of one node and, within the budget, its children.
    std::string text(Id id, std::size_t depth) {
        if (budget_ == 0 || depth >= MAX_DEPTH) {
            return "...";
        }
        --budget_;
        const auto &node = graph_.get(id);
        if (auto leaf = scalar(node); !leaf.empty()) {
            return leaf;
        }
        return compound(node, depth + 1);
    }

  private:
    // Leaves: term(), none(), atoms, integers and type variables.
    static std::string scalar(const Node &node) {
        switch (node.kind) {
        case Kind::top:
            return std::string(TERM_SOURCE);
        case Kind::bottom:
            return "none()";
        case Kind::atom:
            return atom_source(node.name);
        case Kind::integer:
        case Kind::variable:
            return node.name;
        default:
            return {};
        }
    }

    // Children separated by `separator`.
    std::string join(std::span<const Id> children, std::size_t depth, std::string_view separator = ", ") {
        std::string result;
        for (const auto child : children) {
            result += (result.empty() ? "" : std::string(separator)) + text(child, depth);
        }
        return result;
    }

    // A named type with arguments; predefined erlang types drop their module.
    std::string named(const Node &node, std::size_t depth) {
        const auto prefix =
            node.module.empty() || node.module == "erlang" ? std::string() : atom_source(node.module) + ':';
        return prefix + atom_source(node.name) + '(' + join(node.children, depth) + ')';
    }

    std::string list(const Node &node, std::size_t depth) {
        if (node.children.empty()) {
            return "[]";
        }
        return '[' + text(node.children.front(), depth) + (node.name == "nonempty" ? ", ...]" : "]");
    }

    // Map fields alternate keys and values; each field's label is its operator (0: =>, 1: :=).
    std::string map(const Node &node, std::size_t depth) {
        if (node.name == "any") {
            return "map()";
        }
        std::string fields;
        for (std::size_t i = 0; i + 1 < node.children.size(); i += 2) {
            const bool exact = i / 2 < node.labels.size() && node.labels[i / 2] == "1";
            fields += (fields.empty() ? "" : ", ") + text(node.children[i], depth) + (exact ? " := " : " => ") +
                      text(node.children[i + 1], depth);
        }
        return "#{" + fields + '}';
    }

    // Record fields pair each label with its type.
    std::string record(const Node &node, std::size_t depth) {
        std::string fields;
        for (std::size_t i = 0; i < node.children.size() && i < node.labels.size(); ++i) {
            fields +=
                (fields.empty() ? "" : ", ") + atom_source(node.labels[i]) + " :: " + text(node.children[i], depth);
        }
        const auto module = node.module.empty() ? std::string() : atom_source(node.module) + ':';
        return '#' + module + atom_source(node.name) + '{' + fields + '}';
    }

    // <<_:Base, _:_*Unit>> with the parts its labels name.
    std::string bitstring(const Node &node, std::size_t depth) {
        std::string parts;
        for (std::size_t i = 0; i < node.children.size() && i < node.labels.size(); ++i) {
            const auto size = text(node.children[i], depth);
            parts += (parts.empty() ? "" : ", ") + (node.labels[i] == "unit" ? "_:_*" + size : "_:" + size);
        }
        return "<<" + parts + ">>";
    }

    // fun((Inputs) -> Result); a fun of several function types in a Clause notation: fun((1) -> one; (_) -> other).
    std::string function(const Node &node, std::size_t depth) {
        const bool result = std::ranges::contains(node.labels, std::string("result"));
        if (!result) {
            return "fun()";
        }
        if (node.name != "clauses") {
            return "fun(" + signature(node, depth) + ')';
        }
        std::string types;
        for (const auto type : node.children) {
            types += (types.empty() ? "" : "; ") + signature(graph_.get(type), depth);
        }
        return "fun(" + types + ')';
    }

    // `(Inputs) -> Result` of a fun of one function type.
    std::string signature(const Node &node, std::size_t depth) {
        const auto arguments = std::span(node.children).first(node.children.size() - 1);
        const auto input = node.name == "any_arguments" ? std::string("...") : join(arguments, depth);
        return '(' + input + ") -> " + text(node.children.back(), depth);
    }

    // Unary and binary type operators carry their operator code as the node's name.
    std::string operation(const Node &node, std::size_t depth) {
        const auto code = static_cast<std::uint8_t>(std::stoul(node.name));
        if (node.kind == Kind::unary) {
            return operator_source(static_cast<ast::UnaryOperator>(code)) + ' ' + text(node.children.front(), depth);
        }
        return text(node.children.front(), depth) + ' ' + operator_source(static_cast<ast::BinaryOperator>(code)) +
               ' ' + text(node.children.back(), depth);
    }

    // An annotation without a name is a parenthesized type.
    std::string annotation(const Node &node, std::size_t depth) {
        const auto inner = text(node.children.front(), depth);
        return node.name.empty() ? '(' + inner + ')' : node.name + " :: " + inner;
    }

    // A union's members separated by ` | `, its integers in value order where the first one stood, consecutive
    // ones as a range (1 | 2 | 3 | 7 prints 1..3 | 7).
    std::string alternatives(const Node &node, std::size_t depth) {
        std::string result;
        bool integers = false;
        for (const auto child : node.children) {
            const bool integer = graph_.get(child).kind == Kind::integer;
            if (integer && integers) {
                continue;
            }
            result += (result.empty() ? "" : " | ") + (integer ? runs(sorted_integers(node)) : text(child, depth));
            integers = integers || integer;
        }
        return result;
    }

    // The integer members of a union in value order.
    std::vector<BigInt> sorted_integers(const Node &node) const {
        std::vector<BigInt> result;
        for (const auto child : node.children) {
            if (graph_.get(child).kind == Kind::integer) {
                result.push_back(decimal_number(graph_.get(child).name));
            }
        }
        std::ranges::sort(result);
        return result;
    }

    // Sorted integers as ranges of consecutive values and single values, separated by ` | `.
    static std::string runs(const std::vector<BigInt> &integers) {
        std::string result;
        for (std::size_t first = 0; first < integers.size();) {
            auto last = first;
            while (last + 1 < integers.size() && integers[last + 1] == integers[last] + 1) {
                ++last;
            }
            result += (result.empty() ? "" : " | ") + decimal_integer(integers[first]);
            result += last == first ? std::string() : ".." + decimal_integer(integers[last]);
            first = last + 1;
        }
        return result;
    }

    // Containers and ranges; the other kinds go to `operations`.
    std::string compound(const Node &node, std::size_t depth) {
        switch (node.kind) {
        case Kind::range:
            return join(node.children, depth, "..");
        case Kind::tuple:
            return node.name == "any" ? "tuple()" : tuple(node, depth);
        case Kind::list:
            return list(node, depth);
        case Kind::map:
            return map(node, depth);
        case Kind::record:
            return record(node, depth);
        case Kind::positional:
            return positional(node, depth);
        default:
            return operations(node, depth);
        }
    }

    // Bitstrings, funs, operators, annotations, unions and named types.
    std::string operations(const Node &node, std::size_t depth) {
        switch (node.kind) {
        case Kind::bitstring:
            return bitstring(node, depth);
        case Kind::function:
            return function(node, depth);
        case Kind::unary:
        case Kind::binary:
            return operation(node, depth);
        case Kind::annotation:
            return annotation(node, depth);
        case Kind::union_type:
            return alternatives(node, depth);
        default:
            return named(node, depth);
        }
    }

    // A positional list: [A, B] with an empty tail, else [A, B | Tail].
    std::string positional(const Node &node, std::size_t depth) {
        const std::span elements(node.children.begin(), node.children.end() - 1);
        const auto &tail = graph_.get(node.children.back());
        const bool proper = tail.kind == Kind::list && tail.children.empty();
        std::string text_elements;
        for (const auto element : elements) {
            // An element printed with `|` is parenthesized, so it cannot read as the tail's.
            const auto part = text(element, depth);
            const bool grouped = part.find(" | ") != std::string::npos;
            text_elements += (text_elements.empty() ? "" : ", ") + (grouped ? '(' + part + ')' : part);
        }
        return '[' + text_elements + (proper ? "" : " | " + text(node.children.back(), depth)) + ']';
    }

    // A tuple, or the record of the same name and size: #r{f :: T}.
    std::string tuple(const Node &node, std::size_t depth) {
        const auto *fields = record_fields(node);
        if (!fields) {
            return '{' + join(node.children, depth) + '}';
        }
        std::string text_fields;
        for (std::size_t index = 1; index < node.children.size(); ++index) {
            text_fields += (text_fields.empty() ? "" : ", ") + atom_source(fields->at(index - 1)) +
                           " :: " + text(node.children[index], depth);
        }
        return '#' + atom_source(graph_.get(node.children.front()).name) + '{' + text_fields + '}';
    }

    // The field names of the record a tuple's tag and size name; null when none does.
    const std::vector<std::string> *record_fields(const Node &node) const {
        if (!records_ || node.children.empty() || graph_.get(node.children.front()).kind != Kind::atom) {
            return nullptr;
        }
        const auto found = records_->find({graph_.get(node.children.front()).name, node.children.size()});
        return found == records_->end() ? nullptr : &found->second;
    }

    // The graph whose nodes are printed, the nodes left to print, and the records tuples print as.
    const Graph &graph_;
    std::size_t budget_;
    const RecordFields *records_;
};
} // namespace

std::string type_source(const Graph &graph, Id type, std::size_t budget, const RecordFields *records) {
    return Printer(graph, budget, records).text(type, 0);
}

namespace {
// The name argument `index` prints as: its name in `names`, else `_argumentN`.
std::string argument_name(std::span<const std::string> names, const std::size_t index) {
    return index < names.size() ? names[index] : "_argument" + std::to_string(index + 1);
}

// `(Inputs) -> Result` of one function type.
std::string type_text(const Graph &graph, const FunctionText &type, std::span<const std::string> names,
                      const RecordFields *records) {
    const bool related = type.argument && graph.get(type.result).kind == Kind::top;
    std::string inputs;
    for (std::size_t index = 0; index < type.inputs.size(); ++index) {
        const bool named = related && *type.argument == index && type.inputs[index] == graph.top();
        inputs +=
            (inputs.empty() ? "" : ", ") +
            (named ? argument_name(names, index) : type_source(graph, type.inputs[index], DEFAULT_BUDGET, records));
    }
    const auto output =
        related ? argument_name(names, *type.argument) : type_source(graph, type.result, DEFAULT_BUDGET, records);
    return '(' + inputs + ") -> " + output;
}

// `X :: 1, Y :: _`: a dependent clause's parameter facts, each after its parameter's name.
std::string dependent_inputs(const Graph &graph, std::span<const std::string> parameters, const FunctionText &type,
                             const RecordFields *records) {
    std::string result;
    for (std::size_t index = 0; index < type.inputs.size() && index < parameters.size(); ++index) {
        result += result.empty() ? "" : ", ";
        result += parameters[index];
        result += " :: ";
        result += type_source(graph, type.inputs[index], DEFAULT_BUDGET, records);
    }
    return result;
}
} // namespace

std::string dependent_source(const Graph &graph, const DependentText &dependent, std::span<const std::string> names,
                             const RecordFields *records) {
    std::string result(dependent.name);
    for (const auto &type : dependent.types) {
        result += result.size() == dependent.name.size() ? "(" : "; (";
        result += dependent_inputs(graph, dependent.parameters, type, records);
        result += ") -> ";
        const bool related = type.argument && graph.get(type.result).kind == Kind::top;
        result +=
            related ? argument_name(names, *type.argument) : type_source(graph, type.result, DEFAULT_BUDGET, records);
    }
    return result;
}

std::string function_source(const Graph &graph, std::span<const FunctionText> types, std::span<const std::string> names,
                            const RecordFields *records) {
    std::string result;
    for (const auto &type : types) {
        result += (result.empty() ? "" : "; ") + type_text(graph, type, names, records);
    }
    return result;
}
} // namespace clause::semantic::types
