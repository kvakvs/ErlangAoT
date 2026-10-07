#include "funs.hpp"
#include "capabilities.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
bool fun_call(const ast::Module &syntax, const ast::CallExpression &call) {
    const auto &target = syntax.expression(ungroup(syntax, call.target)).value;
    return !std::holds_alternative<ast::Atom>(target) && !std::holds_alternative<ast::RemoteExpression>(target);
}

const Function *fun_target(const Module &module, const ast::LocalFunReference &reference) {
    const auto count = arity(reference.arity);
    if (!count) {
        return nullptr;
    }
    const auto found = module.lookup.find({reference.name.name, *count});
    return found == module.lookup.end() ? nullptr : &module.functions.at(found->second);
}

std::optional<std::tuple<std::u32string, std::u32string, std::size_t>>
external_fun(const ast::RemoteFunReference &reference) {
    const auto *module = std::get_if<ast::Atom>(&reference.module);
    const auto *name = std::get_if<ast::Atom>(&reference.name);
    const auto *number = std::get_if<Integer>(&reference.arity);
    const auto count = number ? arity(*number) : std::nullopt;
    if (!module || !name || !count || *count > 255) {
        return {};
    }
    return std::tuple{module->name, name->name, *count};
}

namespace {
// Give each distinct fun value one entry; a local fun F/A named twice is one value, as in OTP.
class FunIndexer {
  public:
    // Fill the tables of `module`, which start empty.
    explicit FunIndexer(Module &module) : module_(module) {}

    // Record the entry of one fun expression; other expressions and unresolved funs are left alone.
    void visit(const ast::Expression &expression) {
        if (const auto *local = std::get_if<ast::LocalFunReference>(&expression.value)) {
            if (const auto *target = fun_target(module_, *local)) {
                add(expression, {false, module_.name, target->key.name, target->key.arity, 0, target->symbol});
            }
        } else if (const auto *remote = std::get_if<ast::RemoteFunReference>(&expression.value)) {
            if (const auto names = external_fun(*remote)) {
                const auto &[owner, name, count] = *names;
                add(expression, {true, owner, name, count, 0, {}});
            }
        }
    }

  private:
    // Reuse the entry of an equal value or append a new one; local funs take the next index.
    void add(const ast::Expression &expression, FunEntry entry) {
        const auto key = std::tuple{entry.external, entry.module, entry.function, entry.arity};
        const auto [found, fresh] = known_.try_emplace(key, module_.funs.size());
        if (fresh) {
            entry.index = entry.external ? 0 : locals_++;
            module_.funs.push_back(std::move(entry));
        }
        module_.fun_entries.emplace(&expression, found->second);
    }

    Module &module_;
    // Entries of the values seen so far, and the number of local funs among them.
    std::map<std::tuple<bool, std::u32string, std::u32string, std::size_t>, std::size_t> known_;
    std::size_t locals_ = 0;
};
} // namespace

void index_funs(Module &module) {
    FunIndexer indexer(module);
    for (const auto &function : module.functions) {
        auto pending = function_roots(std::get<ast::Function>(module.syntax->form(function.form).value));
        std::ranges::reverse(pending);
        while (!pending.empty()) {
            const auto &expression = module.syntax->expression(pending.back());
            pending.pop_back();
            indexer.visit(expression);
            const auto children = expression_children(module, expression);
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
    }
}

const FunEntry &fun_entry(const Module &module, const ast::Expression &expression) {
    return module.funs.at(module.fun_entries.at(&expression));
}
} // namespace erlang_aot::semantic
