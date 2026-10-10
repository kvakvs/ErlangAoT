#include "behaviours.hpp"
#include <algorithm>
#include <clause/compiler/printing.hpp>
#include <format>
#include <set>

namespace clause::semantic {
namespace {
// The predefined function OTP generates from -callback declarations.
FunctionKey behaviour_info() { return {U"behaviour_info", 1}; }

// A -callback declaration's function; OTP's behaviour_info/1 leaves out callbacks qualified with a module.
std::optional<FunctionKey> callback_key(const ast::FormValue &value) {
    const auto *spec = std::get_if<ast::Specification>(&value);
    if (!spec || !spec->callback || spec->module) {
        return std::nullopt;
    }
    return FunctionKey{spec->name.name, spec->arity};
}

// The callbacks a module declares, in source order.
std::vector<FunctionKey> callbacks(const ast::Module &syntax) {
    std::vector<FunctionKey> result;
    for (const auto &id : syntax.forms()) {
        if (const auto key = callback_key(syntax.form(id).value)) {
            result.push_back(*key);
        }
    }
    return result;
}

// One {Name, Arity} element of a metadata list, as OTP's is_fa_list accepts it.
std::optional<FunctionKey> name_arity(const ast::Module &syntax, const ast::TermId &id) {
    const auto *tuple = std::get_if<ast::TermTuple>(&syntax.term(id).value);
    if (!tuple || tuple->elements.size() != 2) {
        return std::nullopt;
    }
    const auto *name = std::get_if<ast::Atom>(&syntax.term(tuple->elements[0]).value);
    const auto *count = std::get_if<ast::IntegerLiteral>(&syntax.term(tuple->elements[1]).value);
    const auto value = count ? arity(count->value) : std::nullopt;
    return name && value ? std::optional<FunctionKey>{{name->name, *value}} : std::nullopt;
}

// Append a well-formed -optional_callbacks list; OTP skips a list with any malformed element.
void optional_list(const ast::Module &syntax, const ast::TermId &value, std::vector<FunctionKey> &result) {
    const auto *list = std::get_if<ast::TermList>(&syntax.term(value).value);
    if (!list || list->tail) {
        return;
    }
    std::vector<FunctionKey> keys;
    for (const auto &element : list->elements) {
        const auto key = name_arity(syntax, element);
        if (!key) {
            return;
        }
        keys.push_back(*key);
    }
    result.insert(result.end(), keys.begin(), keys.end());
}

// The optional callbacks of every -optional_callbacks attribute, in source order (OTP get_optional_callbacks).
std::vector<FunctionKey> optional_callbacks(const ast::Module &syntax) {
    std::vector<FunctionKey> result;
    for (const auto &id : syntax.forms()) {
        const auto *attribute = std::get_if<ast::GenericAttribute>(&syntax.form(id).value);
        if (attribute && attribute->name.name == U"optional_callbacks") {
            optional_list(syntax, attribute->value, result);
        }
    }
    return result;
}

// The function's name and arity as Erlang source text: name/arity.
std::string function_text(const FunctionKey &key) {
    return atom_source(utf8(key.name)) + "/" + std::to_string(key.arity);
}

// A list of {Name, Arity} tuples as Erlang source text.
std::string list_source(const std::vector<FunctionKey> &keys) {
    std::string result = "[";
    for (const auto &key : keys) {
        result +=
            (result.size() > 1 ? ",{" : "{") + atom_source(utf8(key.name)) + "," + std::to_string(key.arity) + "}";
    }
    return result + "]";
}
} // namespace

std::string behaviour_info_source(const ast::Module &syntax) {
    const auto declared = callbacks(syntax);
    if (declared.empty() || defines_function(syntax, behaviour_info())) {
        return {};
    }
    return "behaviour_info(callbacks) -> " + list_source(declared) + ";\nbehaviour_info(optional_callbacks) -> " +
           list_source(optional_callbacks(syntax)) + ".\n";
}

std::vector<std::u32string> declared_behaviours(const ast::Module &syntax) {
    std::vector<std::u32string> result;
    for (const auto &id : syntax.forms()) {
        const auto *attribute = std::get_if<ast::GenericAttribute>(&syntax.form(id).value);
        const bool behaviour =
            attribute && (attribute->name.name == U"behaviour" || attribute->name.name == U"behavior");
        const auto *name = behaviour ? std::get_if<ast::Atom>(&syntax.term(attribute->value).value) : nullptr;
        if (name) {
            result.push_back(name->name);
        }
    }
    return result;
}

void index_callbacks(Module &module, const Reporter &out) {
    if (module.behaviour_info_ || !module.lookup.contains(behaviour_info())) {
        return;
    }
    for (const auto &id : module.syntax->forms()) {
        const auto &form = module.syntax->form(id);
        const auto *spec = std::get_if<ast::Specification>(&form.value);
        if (spec && spec->callback) {
            // OTP's message spells "attibute".
            report(attribute_name(*module.syntax, form),
                   "cannot define callback attibute for " + function_text({spec->name.name, spec->arity}) +
                       " when behaviour_info is defined",
                   out);
        }
    }
}

namespace {
enum class Resolution : std::uint8_t { callbacks, unknown, undefined };

struct Behaviour {
    // The attribute and its value, which names the behaviour module when it is an atom.
    const ast::Form *form_;
    ast::TermId value_;
    std::optional<std::u32string> name_;
    // What behaviour_info/1 of the module gives: its callbacks, nothing Clause can evaluate, or a failure.
    Resolution resolution_ = Resolution::undefined;
    std::vector<FunctionKey> required_ = {};
    std::vector<FunctionKey> optional_ = {};
};

struct Checker {
    // The module checked, its exports, the nowarn_* options of its -compile attributes and the batch by name.
    const Module &module_;
    std::set<FunctionKey> exports_;
    std::set<std::u32string> suppressed_;
    const std::map<std::u32string, const Module *> &batch_;
    const Reporter &out_;

    // Whether erl_lint reports the warning category, which -compile(nowarn_Category) turns off.
    [[nodiscard]] bool enabled(const std::u32string_view category) const {
        return !suppressed_.contains(std::u32string(category));
    }

    [[nodiscard]] const ast::TokenOrigin &origin(const Behaviour &behaviour) const {
        return attribute_name(*module_.syntax, *behaviour.form_);
    }

    void warn(const Behaviour &behaviour, std::string message) const {
        report(origin(behaviour), std::move(message), out_, Severity::warning);
    }

    void error(const Behaviour &behaviour, std::string message) const {
        report(origin(behaviour), std::move(message), out_);
    }

    void resolve(Behaviour &behaviour) const;
    void check_name(const Behaviour &behaviour) const;
    void undefined(const Behaviour &behaviour) const;
    void missing(const Behaviour &behaviour) const;
    void conflicts(const std::vector<Behaviour> &behaviours, std::size_t index,
                   std::map<FunctionKey, std::size_t> &first) const;
};

// The -behaviour/-behavior attributes of a module, in source order.
std::vector<Behaviour> behaviours(const ast::Module &syntax) {
    std::vector<Behaviour> result;
    for (const auto &id : syntax.forms()) {
        const auto &form = syntax.form(id);
        const auto *attribute = std::get_if<ast::GenericAttribute>(&form.value);
        if (attribute && (attribute->name.name == U"behaviour" || attribute->name.name == U"behavior")) {
            const auto *atom = std::get_if<ast::Atom>(&syntax.term(attribute->value).value);
            result.push_back({&form, attribute->value, atom ? std::optional{atom->name} : std::nullopt});
        }
    }
    return result;
}

// Whether the character is one OTP's check_module_name counts as invisible: space, no-break space, soft hyphen.
bool invisible(const char32_t value) { return value == U' ' || value == 0xA0 || value == 0xAD; }

bool control(const char32_t value) { return value < 0x20 || (value >= 0x7F && value < 0xA0); }

// Find what behaviour_info/1 of the named batch module gives, as erl_lint's call of it would.
void Checker::resolve(Behaviour &behaviour) const {
    const auto found = behaviour.name_ ? batch_.find(*behaviour.name_) : batch_.end();
    if (found == batch_.end()) {
        return;
    }
    const auto &peer = *found->second;
    if (peer.behaviour_info_) {
        behaviour.resolution_ = Resolution::callbacks;
        behaviour.required_ = callbacks(*peer.syntax);
        behaviour.optional_ = optional_callbacks(*peer.syntax);
        return;
    }
    // A hand-written behaviour_info/1 would run in OTP; Clause does not evaluate it and checks nothing.
    const auto function = peer.lookup.find(behaviour_info());
    if (function != peer.lookup.end() && peer.functions[function->second].exported) {
        behaviour.resolution_ = Resolution::unknown;
    }
}

// Report an unusable behaviour module name as OTP's check_module_name does.
void Checker::check_name(const Behaviour &behaviour) const {
    if (!behaviour.name_) {
        error(behaviour, "the module name must be an atom");
        return;
    }
    const auto &name = *behaviour.name_;
    if (name.empty()) {
        error(behaviour, "the module name must not be empty");
        return;
    }
    if (std::ranges::all_of(name, invisible)) {
        error(behaviour, "the module name must contain at least one visible character");
        return;
    }
    if (std::ranges::any_of(name, [](const char32_t value) { return value > 0xFF; })) {
        error(behaviour, "module names with non-latin1 characters are not supported");
    }
    if (std::ranges::any_of(name, control)) {
        error(behaviour, "the module name must not contain control characters");
    }
}

// A behaviour whose module is not in the batch or has no behaviour_info/1.
void Checker::undefined(const Behaviour &behaviour) const {
    if (enabled(U"undefined_behaviour")) {
        const auto text =
            behaviour.name_ ? atom_source(utf8(*behaviour.name_)) : term_source(*module_.syntax, behaviour.value_);
        warn(behaviour, "behaviour " + text + " undefined");
    }
    check_name(behaviour);
}

// Each required callback, in order, that the module does not export.
void Checker::missing(const Behaviour &behaviour) const {
    if (!enabled(U"undefined_behaviour_func")) {
        return;
    }
    std::set<FunctionKey> required(behaviour.required_.begin(), behaviour.required_.end());
    for (const auto &key : behaviour.optional_) {
        required.erase(key);
    }
    const auto name = atom_source(utf8(*behaviour.name_));
    for (const auto &key : required) {
        if (!exports_.contains(key)) {
            warn(behaviour, "undefined callback function " + function_text(key) + " (behaviour '" + name + "')");
        }
    }
}

// Callbacks this behaviour shares with an earlier one; an optional callback counts only when it is exported.
void Checker::conflicts(const std::vector<Behaviour> &behaviours, const std::size_t index,
                        std::map<FunctionKey, std::size_t> &first) const {
    const auto &behaviour = behaviours[index];
    const std::set<FunctionKey> optional(behaviour.optional_.begin(), behaviour.optional_.end());
    std::set<FunctionKey> kept;
    for (const auto &key : behaviour.required_) {
        if (!optional.contains(key) || exports_.contains(key)) {
            kept.insert(key);
        }
    }
    for (const auto &key : kept) {
        const auto [found, added] = first.emplace(key, index);
        if (added || !enabled(U"conflicting_behaviours")) {
            continue;
        }
        const auto &earlier = behaviours[found->second];
        const auto &where = origin(earlier).location;
        warn(behaviour, std::format("conflicting behaviours -- callback {} required by both '{}' and '{}' (line {}, "
                                    "column {})",
                                    function_text(key), atom_source(utf8(*behaviour.name_)),
                                    atom_source(utf8(*earlier.name_)), where.line, where.column));
    }
}

// Check one module's behaviours; diagnostics of one attribute come in OTP's sorted order.
void check_module(const Module &module, const std::map<std::u32string, const Module *> &batch, const Reporter &out) {
    Checker checker{module, {}, disabled_warnings(*module.syntax), batch, out};
    if (!checker.enabled(U"behaviours")) {
        return;
    }
    for (const auto &function : module.functions) {
        if (function.exported) {
            checker.exports_.insert(function.key);
        }
    }
    auto declared = behaviours(*module.syntax);
    for (auto &behaviour : declared) {
        checker.resolve(behaviour);
    }
    std::map<FunctionKey, std::size_t> first;
    for (std::size_t index = 0; index < declared.size(); ++index) {
        checker.conflicts(declared, index, first);
        if (declared[index].resolution_ == Resolution::undefined) {
            checker.undefined(declared[index]);
        } else {
            checker.missing(declared[index]);
        }
    }
}
} // namespace

void check_behaviours(const std::span<const std::unique_ptr<Module>> modules, const Reporter &out) {
    std::map<std::u32string, const Module *> batch;
    for (const auto &module : modules) {
        batch.emplace(module->name, module.get());
    }
    for (const auto &module : modules) {
        check_module(*module, batch, out);
    }
}
} // namespace clause::semantic
