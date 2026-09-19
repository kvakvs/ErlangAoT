#include "ast/builder.hpp"
#include <erlang_aot/compiler/preprocessor.hpp>
#include <optional>
#include <stdexcept>
#include <type_traits>

using namespace erlang_aot;
namespace ast = erlang_aot::ast;
static_assert(!std::is_default_constructible_v<ast::ExprId>);
static_assert(!std::is_convertible_v<ast::ExprId, ast::FormId>);
static_assert(!std::is_convertible_v<ast::PatternSyntaxId, ast::ExprId>);
static_assert(!std::is_convertible_v<ast::TypeId, ast::ExprId>);
static_assert(!std::is_copy_constructible_v<ast::Module>);
static_assert(std::is_nothrow_move_constructible_v<ast::Module>);

// Fail equally in debug and release builds.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("typed AST check failed");
    }
}

// Check public rejection paths without relying on assertion-enabled builds.
template <typename Error, typename Function> void rejects(Function function) {
    try {
        function();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error("expected AST invariant rejection");
}

// Make owned tokens whose source survives the local source manager and scanner.
std::vector<Token> tokens() {
    SourceManager manager;
    Lexer lexer(manager.add("owned.erl", "f() -> 42."));
    return lexer.form();
}

// Exhaustive private dump proves typed visiting without an unchecked fallback.
struct LiteralDump {
    std::string operator()(const ast::ListComprehension &) const { return "list_comprehension"; }

    std::string operator()(const ast::MapComprehension &) const { return "map_comprehension"; }

    std::string operator()(const ast::BinaryComprehension &) const { return "binary_comprehension"; }

    std::string operator()(const ast::LocalFunReference &) const { return "local_fun"; }

    std::string operator()(const ast::RemoteFunReference &) const { return "remote_fun"; }

    std::string operator()(const ast::FunExpression &) const { return "fun"; }

    std::string operator()(const ast::TryExpression &) const { return "try"; }

    std::string operator()(const ast::MaybeExpression &) const { return "maybe"; }

    std::string operator()(const ast::BlockExpression &) const { return "block"; }

    std::string operator()(const ast::CaseExpression &) const { return "case"; }

    std::string operator()(const ast::IfExpression &) const { return "if"; }

    std::string operator()(const ast::ReceiveExpression &) const { return "receive"; }

    std::string operator()(const ast::MapExpression &) const { return "map"; }

    std::string operator()(const ast::RecordExpression &) const { return "record"; }

    std::string operator()(const ast::RecordAccess &) const { return "record_access"; }

    std::string operator()(const ast::RecordIndex &) const { return "record_index"; }

    std::string operator()(const ast::UnaryExpression &) const { return "unary"; }

    std::string operator()(const ast::BinaryExpression &) const { return "binary"; }

    std::string operator()(const ast::MatchExpression &) const { return "match"; }

    std::string operator()(const ast::CatchExpression &) const { return "catch"; }

    std::string operator()(const ast::RemoteExpression &) const { return "remote"; }

    std::string operator()(const ast::CallExpression &) const { return "call"; }

    std::string operator()(const ast::Tuple &) const { return "tuple"; }

    std::string operator()(const ast::List &) const { return "list"; }

    std::string operator()(const ast::Group &) const { return "group"; }

    std::string operator()(const ast::Bitstring &) const { return "bitstring"; }

    std::string operator()(const ast::Atom &atom) const { return "atom:" + utf8(atom.name); }

    std::string operator()(const ast::Variable &var) const { return "var:" + utf8(var.name); }

    std::string operator()(const ast::IntegerLiteral &value) const { return "integer:" + value.value.decimal; }

    std::string operator()(const ast::FloatLiteral &) const { return "float"; }

    std::string operator()(const ast::CharacterLiteral &) const { return "character"; }

    std::string operator()(const ast::StringLiteral &value) const { return "string:" + utf8(value.value); }
};

// Visit every currently supported form kind using named fields, not positional children.
struct FormDump {
    // Borrow the immutable AST while visiting its expression handles.
    const ast::Module &module;

    std::string operator()(const ast::TypeDeclaration &) const { return "type"; }

    std::string operator()(const ast::Specification &) const { return "specification"; }

    std::string operator()(const ast::ModuleAttribute &value) const { return "module:" + utf8(value.name.name); }

    std::string operator()(const ast::FileAttribute &value) const { return "file:" + utf8(value.name); }

    std::string operator()(const ast::ExportAttribute &) const { return "export"; }

    std::string operator()(const ast::ImportAttribute &) const { return "import"; }

    std::string operator()(const ast::ImportRecordAttribute &) const { return "import_record"; }

    std::string operator()(const ast::GenericAttribute &) const { return "attribute"; }

    std::string operator()(const ast::RecordDeclaration &) const { return "record"; }

    std::string operator()(const ast::DocumentationAttribute &) const { return "documentation"; }

    std::string operator()(const ast::Function &value) const {
        std::string result = utf8(value.name.name);
        for (const auto &id : value.clauses.front().body) {
            result += ":" + module.visit(id, LiteralDump{});
        }
        return result;
    }
};

// Allocate many nodes, commit a root, and keep category handles stable through owner moves.
void growth_and_moves() {
    ast::Builder builder;
    const auto input = tokens();
    auto tx = builder.begin(input, input.back());
    std::vector<ast::ExprId> body;
    for (int i = 0; i != 8192; ++i) {
        body.push_back(builder.expression(ast::IntegerLiteral{Integer{std::to_string(i)}}, builder.source(4, 5, 4)));
    }
    const auto first = body.front();
    const auto last = body.back();
    const auto root = builder.form(ast::Function{{U"f"}, {{{}, {}, std::move(body), builder.source(0, 6, 0)}}},
                                   builder.source(0, 6, 0));
    tx.commit(root);
    auto module = std::move(builder).finish();
    ast::Module moved(std::move(module));
    require(moved.forms().size() == 1 && moved.expression_count() == 8192);
    require(moved.visit(first, LiteralDump{}) == "integer:0");
    require(moved.visit(last, LiteralDump{}) == "integer:8191");
    require(moved.anchor(moved.form(root).source).location.file == "owned.erl");
    rejects<std::logic_error>([&] { (void)module.forms(); });
    ast::Module assigned;
    assigned = std::move(moved);
    require(assigned.form(root).source.end == 6);
}

// Reused slots must never make rolled-back expression/form/source handles valid again.
void rollback() {
    ast::Builder builder;
    auto input = tokens();
    std::optional<ast::ExprId> stale;
    std::optional<ast::FormId> stale_form;
    std::optional<ast::NodeSource> stale_source;
    {
        auto tx = builder.begin(input, input.back());
        stale_source = builder.source(0, 6, 0);
        stale = builder.expression(ast::Atom{U"discarded"}, *stale_source);
        stale_form = builder.form(ast::ModuleAttribute{{U"discarded"}}, *stale_source);
    }
    require(builder.view().forms().empty() && builder.view().expression_count() == 0);
    {
        auto tx = builder.begin(input, input.back());
        const auto value = builder.expression(ast::Atom{U"kept"}, builder.source(4, 5, 4));
        const auto root =
            builder.form(ast::Function{{U"f"}, {{{}, {}, {value}, builder.source(0, 6, 0)}}}, builder.source(0, 6, 0));
        tx.commit(root);
    }
    const auto module = std::move(builder).finish();
    rejects<std::invalid_argument>([&] { (void)module.expression(*stale); });
    rejects<std::invalid_argument>([&] { (void)module.form(*stale_form); });
    rejects<std::invalid_argument>([&] { (void)module.anchor(*stale_source); });
}

// Cross-owner children, empty bodies, nested transactions, and malformed extents are rejected.
void invariants() {
    ast::Builder first;
    ast::Builder second;
    const auto input = tokens();
    auto a = first.begin(input, input.back());
    auto b = second.begin(input, input.back());
    const auto value = first.expression(ast::Variable{U"X"}, first.source(4, 5, 4));
    rejects<std::invalid_argument>([&] { (void)second.view().expression(value); });
    rejects<std::invalid_argument>([&] {
        second.form(ast::Function{{U"f"}, {{{}, {}, {value}, second.source(0, 6, 0)}}}, second.source(0, 6, 0));
    });
    rejects<std::invalid_argument>([&] { first.form(ast::Function{{U"f"}, {}}, first.source(0, 6, 0)); });
    rejects<std::invalid_argument>([&] { second.expression(ast::Tuple{{value}}, second.source(0, 6, 0)); });
    rejects<std::invalid_argument>([&] { second.expression(ast::Group{value}, second.source(0, 6, 0)); });
    rejects<std::invalid_argument>([&] { first.expression(ast::List{{}, value}, first.source(0, 6, 0)); });
    rejects<std::logic_error>([&] { (void)first.begin(input, input.back()); });
    rejects<std::logic_error>([&] { (void)std::move(first).finish(); });
    rejects<std::out_of_range>([&] { (void)first.source(4, 3, 4); });
    rejects<std::out_of_range>([&] { (void)first.source(0, 7, 0); });
    rejects<std::out_of_range>([&] { (void)first.source(0, 6, 6); });
}

// Recover a failed later form without invalidating earlier committed roots.
void committed_survives() {
    ast::Builder builder;
    auto input = tokens();
    ast::FormId root = [&] {
        auto tx = builder.begin(input, input.back());
        auto id = builder.form(ast::ModuleAttribute{{U"m"}}, builder.source(0, 6, 0));
        tx.commit(id);
        rejects<std::logic_error>([&] { tx.commit(id); });
        return id;
    }();
    {
        auto tx = builder.begin(input, input.back());
        builder.expression(ast::Atom{U"bad"}, builder.source(0, 1, 0));
        rejects<std::invalid_argument>([&] { tx.commit(root); });
    }
    const auto module = std::move(builder).finish();
    require(module.forms().size() == 1 && module.expression_count() == 0);
    require(std::get<ast::ModuleAttribute>(module.form(root).value).name.name == U"m");
}

// Synthetic/empty extents use an explicit EOF location and never touch absent tokens.
void empty_origin() {
    ast::Builder builder;
    Token eof{TokenKind::dot, std::u32string{}, {}, {"synthetic.erl", 9, 3}, {}};
    auto tx = builder.begin({}, eof);
    const auto root = builder.form(ast::ModuleAttribute{{U"synthetic"}}, builder.source(0, 0, 0));
    tx.commit(root);
    const auto module = std::move(builder).finish();
    require(module.extent(module.form(root).source).empty());
    require(module.anchor(module.form(root).source).location.line == 9);
}

// Macro-expanded form origins can span buffers and must outlive all preprocessing objects.
ast::Module expanded_module() {
    SourceManager sources;
    auto source = sources.add("main.erl", "-include(\"value.hrl\").\nf() -> ?VALUE.\n");
    PreprocessorOptions options;
    options.read_file = [](const auto &) { return std::optional<std::string>("-define(VALUE, 42).\n"); };
    PreprocessorSession session(source, options);
    ast::Builder builder;
    while (auto event = session.next()) {
        if (const auto *form = std::get_if<OrdinaryForm>(&*event)) {
            if (form->tokens.front().text() != U"f") {
                continue;
            }
            auto tx = builder.begin(form->tokens, form->tokens.back());
            const auto literal = builder.expression(ast::IntegerLiteral{Integer{"42"}}, builder.source(4, 5, 4));
            const auto root = builder.form(ast::Function{{U"f"}, {{{}, {}, {literal}, builder.source(0, 6, 0)}}},
                                           builder.source(0, 6, 0));
            tx.commit(root);
        }
    }
    require(!session.failed());
    return std::move(builder).finish();
}

// Check both physical definition spelling and logical macro invocation locations.
void provenance() {
    const auto module = expanded_module();
    require(module.forms().size() == 1);
    const auto &form = module.form(module.forms().front());
    const auto &body = std::get<ast::Function>(form.value).clauses.front().body;
    const auto &literal = module.expression(body.front());
    const auto &origin = module.anchor(literal.source);
    require(origin.location.file == "main.erl" && origin.location.line == 2);
    require(!origin.related.empty());
    const auto origins = module.extent(form.source);
    require(origins.front().spelling.source != origin.spelling.source);
    require(module.visit(body.front(), LiteralDump{}) == "integer:42");
    require(module.visit(module.forms().front(), FormDump{module}) == "f:integer:42");
}

// Exercise construction, lifetime, rollback, invariants, and exhaustive visitation.
int main() {
    growth_and_moves();
    rollback();
    invariants();
    committed_survives();
    empty_origin();
    provenance();
}
