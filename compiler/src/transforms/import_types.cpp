// Added for parse transforms: abstract types as tokens (inverse of export_types).
#include "import_writer.hpp"
#include <algorithm>
#include <array>

namespace clause::transforms {
namespace {
// A {type, A, Name, Arguments} node: its name, arguments term and place.
struct TypeNode {
    std::u32string_view name_;
    TermId arguments_;
    Place at_;
};

// Brackets and separator around a list of member types.
struct Marks {
    std::u32string_view open_;
    std::u32string_view separator_;
    std::u32string_view close_;
};

// `Name(Args)`; tuple() and map() have `any` instead of arguments.
void application(FormImporter &importer, const TypeNode &node) {
    importer.writer().atom(node.name_, node.at_);
    importer.writer().symbol(U"(", node.at_);
    if (!importer.terms().is_atom(node.arguments_, U"any")) {
        importer.types(node.arguments_);
    }
    importer.writer().symbol(U")", node.at_);
}

// Member types between brackets; members of unions and ranges are not top types, so unions inside get parentheses.
void members(FormImporter &importer, const TypeNode &node, const Marks &marks, const bool top) {
    if (!marks.open_.empty()) {
        importer.writer().symbol(marks.open_, node.at_);
    }
    const auto &items = importer.items(node.arguments_);
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index != 0) {
            importer.writer().symbol(marks.separator_, node.at_);
        }
        importer.type(items[index], top);
    }
    if (!marks.close_.empty()) {
        importer.writer().symbol(marks.close_, node.at_);
    }
}

// Map field types `K => V`, `K := V` and record field types `name :: T`.
void fields(FormImporter &importer, const std::span<const TermId> items) {
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto &parts = importer.children<4>(items[index]);
        const auto &pair = importer.items(parts[3]);
        const auto at = importer.place(items[index]);
        const auto kind = importer.atom(parts[2]);
        if (pair.size() != 2) {
            importer.fail(items[index], "malformed field type");
        }
        if (index != 0) {
            importer.writer().symbol(U",", at);
        }
        importer.type(pair[0]);
        importer.writer().symbol(kind == U"map_field_exact" ? U":=" : kind == U"field_type" ? U"::" : U"=>", at);
        importer.type(pair[1]);
    }
}

void map_type(FormImporter &importer, const TypeNode &node) {
    importer.writer().symbol(U"#", node.at_);
    importer.writer().symbol(U"{", node.at_);
    fields(importer, importer.items(node.arguments_));
    importer.writer().symbol(U"}", node.at_);
}

// #r{...} or #m:r{...}: the name, then refined field types.
void record_type(FormImporter &importer, const TypeNode &node) {
    const auto &items = importer.items(node.arguments_);
    if (items.empty()) {
        importer.error("record type without a name");
    }
    importer.writer().symbol(U"#", node.at_);
    if (importer.tag(items[0]) == U"tuple") {
        const auto &name = importer.items(importer.children<3>(items[0])[2]);
        if (name.size() != 2) {
            importer.error("malformed record type name");
        }
        importer.expression(name[0]);
        importer.writer().symbol(U":", node.at_);
        importer.expression(name[1]);
    } else {
        importer.expression(items[0]);
    }
    importer.writer().symbol(U"{", node.at_);
    fields(importer, std::span(items).subspan(1));
    importer.writer().symbol(U"}", node.at_);
}

// <<>>, <<_:B>>, <<_:_*U>>, <<_:B, _:_*U>> with zero sizes left out.
void binary_type(FormImporter &importer, const TypeNode &node) {
    const auto &items = importer.items(node.arguments_);
    if (items.size() != 2) {
        importer.error("malformed binary type");
    }
    const auto zero = [&](const TermId id) {
        return importer.tag(id) == U"integer" && importer.terms().small_integer(importer.children<3>(id)[2]) == 0;
    };
    auto &writer = importer.writer();
    writer.symbol(U"<<", node.at_);
    if (!zero(items[0])) {
        writer.variable(U"_", node.at_);
        writer.symbol(U":", node.at_);
        importer.type(items[0], false);
    }
    if (!zero(items[1])) {
        if (!zero(items[0])) {
            writer.symbol(U",", node.at_);
        }
        writer.variable(U"_", node.at_);
        writer.symbol(U":", node.at_);
        writer.variable(U"_", node.at_);
        writer.symbol(U"*", node.at_);
        importer.type(items[1], false);
    }
    writer.symbol(U">>", node.at_);
}

// fun(), fun((...) -> T), fun((Args) -> T).
void fun_type(FormImporter &importer, const TypeNode &node) {
    auto &writer = importer.writer();
    writer.keyword(U"fun", node.at_);
    writer.symbol(U"(", node.at_);
    const auto &items = importer.items(node.arguments_);
    if (items.size() == 2) {
        writer.symbol(U"(", node.at_);
        const auto &arguments = importer.terms().node(items[0]).children_;
        if (arguments.size() == 4) {
            importer.types(arguments[3]);
        } else {
            writer.symbol(U"...", node.at_);
        }
        writer.symbol(U")", node.at_);
        writer.symbol(U"->", node.at_);
        importer.type(items[1]);
    } else if (!items.empty()) {
        importer.error("malformed fun type");
    }
    writer.symbol(U")", node.at_);
}

// [T, ...]
void nonempty_list(FormImporter &importer, const TypeNode &node) {
    members(importer, node, {.open_ = U"[", .separator_ = U",", .close_ = U""}, true);
    importer.writer().symbol(U",", node.at_);
    importer.writer().symbol(U"...", node.at_);
    importer.writer().symbol(U"]", node.at_);
}

using Writer = void (*)(FormImporter &, const TypeNode &);

struct Special {
    std::u32string_view name_;
    Writer writer_;
};

// Types written as members between brackets; union and range members are not top types.
struct Bracketed {
    std::u32string_view name_;
    Marks marks_;
    bool top_;
};

constexpr std::array<Bracketed, 5> BRACKETED{{{U"union", {U"", U"|", U""}, false},
                                              {U"range", {U"", U"..", U""}, false},
                                              {U"nil", {U"[", U",", U"]"}, true},
                                              {U"list", {U"[", U",", U"]"}, true},
                                              {U"tuple", {U"{", U",", U"}"}, true}}};

constexpr std::array<Special, 5> SPECIALS{{{U"map", map_type},
                                           {U"record", record_type},
                                           {U"binary", binary_type},
                                           {U"fun", fun_type},
                                           {U"nonempty_list", nonempty_list}}};

// How many arguments make a predefined type name use its own syntax: [T], [T, ...], <<...>>; others are calls.
bool own_syntax(const std::u32string_view name, const std::size_t arguments) {
    if (name == U"list" || name == U"nonempty_list") {
        return arguments == 1;
    }
    return name != U"binary" || arguments == 2;
}

// {type, A, Name, Arguments}: the forms with their own syntax, else `Name(Args)`.
void predefined_type(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<4>(id);
    const TypeNode node{.name_ = importer.atom(parts[2]), .arguments_ = parts[3], .at_ = importer.place(id)};
    const auto &arguments = importer.terms().node(parts[3]);
    const bool plain =
        importer.terms().is_atom(parts[3], U"any") || !own_syntax(node.name_, arguments.children_.size());
    const auto bracketed = std::ranges::find(BRACKETED, node.name_, &Bracketed::name_);
    const auto special = std::ranges::find(SPECIALS, node.name_, &Special::name_);
    if (!plain && bracketed != BRACKETED.end()) {
        members(importer, node, bracketed->marks_, bracketed->top_);
    } else if (!plain && special != SPECIALS.end()) {
        special->writer_(importer, node);
    } else {
        application(importer, node);
    }
}

// {remote_type, A, [Module, Name, Arguments]}.
void remote_type(FormImporter &importer, const TermId id) {
    const auto &parts = importer.children<3>(id);
    const auto &items = importer.items(parts[2]);
    if (items.size() != 3) {
        importer.fail(id, "malformed remote type");
    }
    const auto at = importer.place(id);
    importer.expression(items[0]);
    importer.writer().symbol(U":", at);
    importer.expression(items[1]);
    importer.writer().symbol(U"(", at);
    importer.types(items[2]);
    importer.writer().symbol(U")", at);
}

// {op, A, Op, Operand} and {op, A, Op, Left, Right} in types.
void operator_type(FormImporter &importer, const TermId id) {
    const auto size = importer.terms().node(id).children_.size();
    const auto &parts = importer.children<4, 5>(id);
    const auto name = importer.atom(parts[2]);
    const auto at = importer.place(id);
    const auto token = [&] {
        if (name.front() >= U'a' && name.front() <= U'z') {
            importer.writer().keyword(name, at);
        } else {
            importer.writer().symbol(name, at);
        }
    };
    if (size == 4) {
        token();
        importer.type(parts[3], false);
        return;
    }
    importer.type(parts[3], false);
    token();
    importer.type(parts[4], false);
}
} // namespace

void import_type(FormImporter &importer, const TermId id) {
    const auto kind = importer.tag(id);
    if (kind == U"type") {
        predefined_type(importer, id);
    } else if (kind == U"user_type") {
        const auto &parts = importer.children<4>(id);
        application(importer, {.name_ = importer.atom(parts[2]), .arguments_ = parts[3], .at_ = importer.place(id)});
    } else if (kind == U"remote_type") {
        remote_type(importer, id);
    } else if (kind == U"ann_type") {
        const auto &parts = importer.children<3>(id);
        const auto &pair = importer.items(parts[2]);
        if (pair.size() != 2) {
            importer.fail(id, "malformed annotated type");
        }
        importer.expression(pair[0]);
        importer.writer().symbol(U"::", importer.place(id));
        importer.type(pair[1]);
    } else if (kind == U"op") {
        operator_type(importer, id);
    } else {
        import_expression(importer, id);
    }
}
} // namespace clause::transforms
