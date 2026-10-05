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

// Walk only admitted executable children; atom call targets are metadata rather than term expressions.
void record_atoms(const semantic::Module &module, const ast::ExprValue &value, std::set<std::string> &result) {
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

// Walk only admitted executable children; atom call targets are metadata rather than term expressions.
std::set<std::string> spellings(const semantic::Module &module) {
    std::set<std::string> result;
    std::vector<ast::ExprId> pending;
    for (const auto &function : module.functions) {
        roots(module, function, pending, result);
    }
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = module.syntax->expression(id);
        record_atoms(module, expression.value, result);
        if (booleans(expression.value) || guarded(expression.value)) {
            result.insert("true");
            result.insert("false");
        }
        if (const auto *atom = std::get_if<ast::Atom>(&expression.value)) {
            result.insert(utf8(atom->name));
        }
        if (implicit_throw(expression.value)) {
            result.insert("throw");
        }
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

llvm::Value *lower_atom(ExpressionLowering &state, const ast::Atom &atom) {
    auto &output = *state.entry.getParent();
    auto &builder = state.builder;
    auto *slot = output.getNamedGlobal(slot_name(utf8(atom.name)))->getInitializer();
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
