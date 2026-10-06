#include "module_atoms.hpp"
#include "../semantic/capabilities.hpp"
#include "../semantic/records.hpp"
#include "../semantic/services.hpp"
#include "../semantic/symbols.hpp"
#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <algorithm>
#include <set>

namespace erlang_aot::codegen {
namespace {
// Reuse the collision-free symbol encoding for private literal-slot metadata.
std::string slot_name(const std::string &spelling) { return "atom.slot." + semantic::encode_symbol({spelling, "", 0}); }

// Normalized patterns can contain atoms folded from grouped syntax; collect their runtime spellings too.
void pattern_atoms(const semantic::Function &function, std::set<std::string> &result) {
    for (const auto &pattern : function.patterns) {
        if (!pattern.literal) {
            continue;
        }
        if (const auto *atom = std::get_if<ast::Atom>(&*pattern.literal)) {
            result.insert(utf8(atom->name));
        }
    }
}

// Guard roots and service outputs require preinitialized true/false slots alongside body literals.
void roots(const semantic::Module &module, const semantic::Function &function, std::vector<ast::ExprId> &pending,
           std::set<std::string> &result) {
    const auto &definition = std::get<ast::Function>(module.syntax->form(function.form).value);
    const auto expressions = semantic::function_roots(definition);
    pending.insert(pending.end(), expressions.begin(), expressions.end());
    const auto keys = semantic::pattern_reads(module, function);
    pending.insert(pending.end(), keys.begin(), keys.end());
    if (!function.services.empty() ||
        std::ranges::any_of(definition.clauses, [](const auto &clause) { return clause.guard.has_value(); })) {
        result.insert("true");
        result.insert("false");
    }
    pattern_atoms(function, result);
}

// Boolean syntax needs both canonical slots even when all source operands are incoming variables.
bool booleans(const ast::ExprValue &value) {
    if (const auto *unary = std::get_if<ast::UnaryExpression>(&value)) {
        return unary->operation == ast::UnaryOperator::logical_not;
    }
    const auto *binary = std::get_if<ast::BinaryExpression>(&value);
    return binary &&
           (semantic::immediate_operator(binary->operation) || binary->operation == ast::BinaryOperator::and_also ||
            binary->operation == ast::BinaryOperator::or_else);
}

// Case, if and try clause guards compare each test with the canonical true atom.
bool guarded(const ast::ExprValue &value) {
    return std::ranges::any_of(semantic::branch_clauses(value),
                               [](const auto &clause) { return clause.guard != nullptr; });
}

// A catch clause without a class matches the class atom throw.
bool implicit_throw(const ast::ExprValue &value) {
    return std::ranges::any_of(semantic::branch_clauses(value),
                               [](const auto &clause) { return clause.handler && !clause.handler->exception_class; });
}

// Record tags, the undefined default and the field names a record_info(fields, R) list holds.
void record_atoms(const semantic::Module &module, const ast::Expression &expression, std::set<std::string> &result) {
    const auto &value = expression.value;
    if (const auto info = semantic::record_info(module, expression); info && info->fields) {
        for (const auto &field : info->layout.fields) {
            result.insert(utf8(field.name.name));
        }
    }
    const ast::RecordIdentity *identity = nullptr;
    if (const auto *record = std::get_if<ast::RecordExpression>(&value)) {
        identity = &record->identity;
        result.insert("undefined");
    } else if (const auto *access = std::get_if<ast::RecordAccess>(&value)) {
        identity = &access->identity;
    }
    if (identity) {
        result.insert(utf8(semantic::record_layout(module, *identity)->name.name));
    }
}

// erlang:raise/3 evaluates to badarg when its class or stack is invalid.
bool raises_stack(const ast::Module &syntax, const ast::ExprValue &value) {
    const auto *call = std::get_if<ast::CallExpression>(&value);
    const auto *remote =
        call ? std::get_if<ast::RemoteExpression>(&syntax.expression(semantic::ungroup(syntax, call->target)).value)
             : nullptr;
    if (!remote || call->arguments.size() != 3) {
        return false;
    }
    const auto *owner = std::get_if<ast::Atom>(&syntax.expression(semantic::ungroup(syntax, remote->module)).value);
    const auto *name = std::get_if<ast::Atom>(&syntax.expression(semantic::ungroup(syntax, remote->function)).value);
    return owner && name && owner->name == U"erlang" && name->name == U"raise";
}

// Collect the atoms one expression needs: its literal, record names and the atoms its lowering produces.
void expression_atoms(const semantic::Module &module, const ast::Expression &expression,
                      std::set<std::string> &result) {
    const auto &value = expression.value;
    record_atoms(module, expression, result);
    if (booleans(value) || guarded(value) || semantic::comprehension_qualifiers(value)) {
        result.insert("true");
        result.insert("false");
    }
    if (const auto *atom = std::get_if<ast::Atom>(&value)) {
        result.insert(utf8(atom->name));
    }
    if (implicit_throw(value)) {
        result.insert("throw");
    }
    if (raises_stack(*module.syntax, value)) {
        result.insert("badarg");
    }
}

// Walk only admitted executable children; atom call targets are metadata rather than term expressions.
std::set<std::string> spellings(const semantic::Module &module) {
    std::set<std::string> result{utf8(module.name)};
    std::vector<ast::ExprId> pending;
    for (const auto &function : module.functions) {
        result.insert(utf8(function.key.name));
        roots(module, function, pending, result);
    }
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = module.syntax->expression(id);
        expression_atoms(module, expression, result);
        const auto children = semantic::expression_children(module, expression);
        pending.insert(pending.end(), children.begin(), children.end());
    }
    return result;
}
} // namespace

llvm::Constant *emit_atom_table(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
                                std::size_t &count) {
    auto &context = output.getContext();
    auto *type = llvm::StructType::get(llvm::PointerType::get(context, 0), word);
    std::vector<llvm::Constant *> entries;
    for (const auto &name : spellings(module)) {
        auto *bytes = llvm::ConstantDataArray::getString(context, name, false);
        auto *text = new llvm::GlobalVariable(output, bytes->getType(), true, llvm::GlobalValue::PrivateLinkage, bytes,
                                              "atom.spelling");
        auto *slot = llvm::cast<llvm::GlobalVariable>(output.getOrInsertGlobal(slot_name(name), word));
        slot->setConstant(true);
        slot->setLinkage(llvm::GlobalValue::PrivateLinkage);
        slot->setInitializer(llvm::ConstantInt::get(word, entries.size()));
        entries.push_back(llvm::ConstantStruct::get(type, text, llvm::ConstantInt::get(word, name.size())));
    }
    count = entries.size();
    auto *array = llvm::ConstantArray::get(llvm::ArrayType::get(type, count), entries);
    return new llvm::GlobalVariable(output, array->getType(), true, llvm::GlobalValue::PrivateLinkage, array,
                                    "module.atoms");
}

llvm::Constant *atom_slot(llvm::Module &output, const std::string &spelling) {
    return output.getNamedGlobal(slot_name(spelling))->getInitializer();
}

llvm::Value *lower_atom(ExpressionLowering &state, const ast::Atom &atom) {
    auto &output = *state.entry.getParent();
    auto &builder = state.builder;
    auto *slot = atom_slot(output, utf8(atom.name));
    const auto prefix = semantic::encode_symbol({utf8(state.module.name), "", 0});
    auto *descriptor = output.getNamedGlobal(prefix + ".descriptor");
    const auto symbol = services::symbol<services::Atom>(output.getTargetTriple());
    auto service = output.getOrInsertFunction(
        symbol, llvm::FunctionType::get(state.word, {builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *value = builder.CreateCall(service, {state.entry.getArg(0), slot, descriptor}, "atom.value");
    propagate_failure(state);
    return value;
}
} // namespace erlang_aot::codegen
