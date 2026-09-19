#include "forms.hpp"

namespace erlang_aot {
ast::TypeValue FormParser::type_primary() {
    if (cursor_.take_syntax(U"(")) {
        auto type = top_type();
        expect(U")");
        return ast::TypeGroup{std::move(type)};
    }
    if (cursor_.take_syntax(U"{"))
        return ast::TupleType{false, type_elements(U"}")};
    if (cursor_.take_syntax(U"["))
        return list_type();
    if (cursor_.take_syntax(U"#"))
        return hash_type();
    if (cursor_.take_syntax(U"<<"))
        return bitstring_type();
    if (cursor_.take_syntax(U"fun")) {
        expect(U"(");
        if (cursor_.take_syntax(U")"))
            return ast::FunType{};
        auto result = fun_type();
        expect(U")");
        return result;
    }
    return named_type();
}

ast::ListType FormParser::list_type() {
    if (cursor_.take_syntax(U"]"))
        return {{}, false};
    auto element = top_type();
    const auto nonempty = cursor_.take_syntax(U",");
    if (nonempty)
        expect(U"...");
    expect(U"]");
    return {std::move(element), nonempty};
}

ast::FunType FormParser::fun_type() {
    expect(U"(");
    std::optional<std::vector<ast::TypeId>> arguments;
    if (cursor_.take_syntax(U"..."))
        expect(U")");
    else
        arguments = type_elements(U")");
    expect(U"->");
    return {std::move(arguments), top_type()};
}
} // namespace erlang_aot
