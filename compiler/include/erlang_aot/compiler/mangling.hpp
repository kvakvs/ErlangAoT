#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Compile-time C++ symbol spelling for native runtime services called by generated code.
//
// C++ has no language-standard name-mangling scheme. Mangling is part of the platform/compiler ABI, so different compiler families use different encodings.
// Why Itanium you would ask? The major schemes you will encounter are:
//
// Scheme	                    Typical users	                                    Example
// Itanium C++ ABI	            GCC, Clang on Linux/macOS/most Unix-like systems	_ZN3Foo3barEi
// Microsoft Visual C++ ABI	    MSVC, clang-cl on Windows	                        ?bar@Foo@@QEAAXH@Z
// Older GCC schemes	        GCC 2.x and earlier	                                historical, mostly obsolete
// Borland/Embarcadero schemes	Borland C++, C++Builder	                            proprietary
// IBM / AIX ABI	            IBM XL C++ and related platforms	                platform-specific
// Sun/Oracle C++ ABI	        Older Solaris toolchains	                        platform-specific
namespace erlang_aot::mangling {
// C++ ABI family whose symbol spelling the emitted target links against.
enum class Scheme : std::uint8_t { itanium, microsoft };

// Target facts that change a mangled symbol; the host compiler's own ABI is irrelevant.
struct Target {
    // Selects Itanium (Linux, macOS) or Microsoft (Windows MSVC) spelling.
    Scheme scheme = Scheme::itanium;
    // True for 64-bit pointers; widens Size and adds Microsoft __ptr64 markers.
    bool wide = true;
};

// Literal text usable as a template argument, such as a qualified "ns::Name".
template <std::size_t N> struct FixedString {
    // Characters including the terminating null.
    std::array<char, N> text{};

    // Copy a string literal; implicit so literals can spell template arguments directly.
    constexpr FixedString(const char (&source)[N]) { std::copy_n(source, N, text.begin()); }

    // Characters without the terminating null.
    [[nodiscard]] constexpr std::string_view view() const { return {text.data(), N - 1}; }
};

namespace detail {
// Shape of one signature type; builtin order matches the encoding tables below.
enum class Kind : std::uint8_t {
    void_type,
    boolean,
    character,
    unsigned_character,
    integer,
    unsigned_integer,
    size,
    pointer,
    enumeration
};

// One signature type, linked to its pointee for pointer chains.
struct TypeNode {
    // Builtin, pointer or named enumeration shape.
    Kind kind = Kind::void_type;
    // Const qualification of this type itself; only meaningful beneath a pointer.
    bool constant = false;
    // Pointed-to type for Kind::pointer, null otherwise.
    const TypeNode *pointee = nullptr;
    // Qualified spelling with "::" separators for Kind::enumeration.
    std::string_view name;
};

// Tag for one builtin type; public tags below derive from it.
template <Kind K> struct Builtin {
    static constexpr TypeNode node{K, false, nullptr, {}};
};
} // namespace detail

// Builtin type tags; Size is the target's std::size_t / std::uintptr_t (abi::v1::TermWord).
struct Void : detail::Builtin<detail::Kind::void_type> {};

struct Bool : detail::Builtin<detail::Kind::boolean> {};

struct Char : detail::Builtin<detail::Kind::character> {};

struct UInt8 : detail::Builtin<detail::Kind::unsigned_character> {};

struct Int32 : detail::Builtin<detail::Kind::integer> {};

struct UInt32 : detail::Builtin<detail::Kind::unsigned_integer> {};

struct Size : detail::Builtin<detail::Kind::size> {};

// Pointer to T; spell `const T *` as Pointer<Const<T>>.
template <typename T> struct Pointer {
    static constexpr detail::TypeNode node{detail::Kind::pointer, false, &T::node, {}};
};

// Const-qualified T; top-level const on parameters and results is dropped as in C++.
template <typename T> struct Const {
    static constexpr detail::TypeNode node{T::node.kind, true, T::node.pointee, T::node.name};
};

// Enumeration named by its fully qualified spelling, such as "erlang_aot::abi::v1::ErrorReason".
template <FixedString Name> struct Enum {
    static constexpr detail::TypeNode node{detail::Kind::enumeration, false, nullptr, Name.view()};
};

namespace detail {
// Split "a::b::c" into identifiers, outermost first; empty identifiers are rejected.
constexpr std::vector<std::string_view> split(std::string_view name) {
    std::vector<std::string_view> parts;
    for (auto at = name.find("::"); at != std::string_view::npos; at = name.find("::")) {
        parts.push_back(name.substr(0, at));
        name.remove_prefix(at + 2);
    }
    parts.push_back(name);
    if (std::ranges::find(parts, std::string_view{}) != parts.end()) {
        throw std::invalid_argument("mangling: empty identifier in qualified name");
    }
    return parts;
}

// Spell a number in base 10 or 36 (upper-case), as both ABIs use for lengths and indices.
constexpr std::string digits(std::size_t value, std::size_t base) {
    constexpr std::string_view alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string text(1, alphabet[value % base]);
    for (value /= base; value != 0; value /= base) {
        text.insert(text.begin(), alphabet[value % base]);
    }
    return text;
}

// Canonical, substitution-free spelling used as a back-reference key by both encoders.
constexpr std::string key(const TypeNode &node) {
    std::string text = node.constant ? "K" : "";
    if (node.kind == Kind::pointer) {
        return text + "P" + key(*node.pointee);
    }
    if (node.kind == Kind::enumeration) {
        return text + "N" + std::string(node.name);
    }
    return text + digits(static_cast<std::size_t>(node.kind), 10);
}

// Key of the first `count` identifiers of a qualified name; matches key() of an enumeration.
constexpr std::string prefix_key(const std::vector<std::string_view> &parts, std::size_t count) {
    std::string text = "N";
    for (std::size_t i = 0; i < count; ++i) {
        text += i == 0 ? "" : "::";
        text += parts[i];
    }
    return text;
}

// Copy of a node without its own const; signatures drop top-level qualifiers.
constexpr TypeNode unqualified(TypeNode node) {
    node.constant = false;
    return node;
}

// Builtins are encoded inline and never enter back-reference tables.
constexpr bool builtin(const TypeNode &node) { return node.kind < Kind::pointer; }

// Position of `item` in a back-reference table, or the table size when absent.
template <typename Item, typename Value>
constexpr std::size_t position(const std::vector<Item> &table, const Value &item) {
    return static_cast<std::size_t>(std::ranges::find(table, item) - table.begin());
}

// Itanium C++ ABI encoder (Linux, macOS) with its S_ substitution table.
class Itanium {
  public:
    // Pointer width selects the spelling of Size.
    constexpr explicit Itanium(bool wide) : wide_(wide) {}

    // Encode a non-template free function; Itanium omits its return type.
    constexpr std::string function(std::string_view name, std::span<const TypeNode *const> params) {
        out_ = "_Z";
        qualified(split(name), false);
        if (params.empty()) {
            out_ += 'v';
        }
        for (const auto *param : params) {
            type(unqualified(*param));
        }
        return out_;
    }

  private:
    // Encode a type, reusing an earlier equal type by substitution; composites become candidates.
    constexpr void type(const TypeNode &node) {
        if (builtin(node) && !node.constant) {
            out_ += builtin_code(node.kind);
            return;
        }
        if (node.kind == Kind::enumeration && !node.constant) {
            qualified(split(node.name), true);
            return;
        }
        const auto spelling = key(node);
        if (substitute(spelling)) {
            return;
        }
        out_ += node.constant ? 'K' : 'P';
        type(node.constant ? unqualified(node) : *node.pointee);
        candidates_.push_back(spelling);
    }

    // Encode a possibly nested name; namespace prefixes and type names become candidates.
    constexpr void qualified(const std::vector<std::string_view> &parts, bool is_type) {
        if (parts.front() == "std") {
            throw std::invalid_argument("mangling: std:: abbreviations are not supported");
        }
        if (is_type && substitute(prefix_key(parts, parts.size()))) {
            return;
        }
        if (parts.size() == 1) {
            identifier(parts, 0, is_type);
            return;
        }
        out_ += 'N';
        for (auto index = substituted_prefix(parts); index < parts.size(); ++index) {
            identifier(parts, index, is_type);
        }
        out_ += 'E';
    }

    // Emit one length-prefixed identifier and record the prefix it completes, except a function's name.
    constexpr void identifier(const std::vector<std::string_view> &parts, std::size_t index, bool is_type) {
        out_ += digits(parts[index].size(), 10);
        out_ += parts[index];
        if (is_type || index + 1 < parts.size()) {
            candidates_.push_back(prefix_key(parts, index + 1));
        }
    }

    // Emit the longest already-seen proper prefix of a nested name; returns identifiers covered.
    constexpr std::size_t substituted_prefix(const std::vector<std::string_view> &parts) {
        for (auto count = parts.size() - 1; count > 0; --count) {
            if (substitute(prefix_key(parts, count))) {
                return count;
            }
        }
        return 0;
    }

    // Emit S_ / S<seq-id>_ when `spelling` was seen before.
    constexpr bool substitute(const std::string &spelling) {
        const auto index = position(candidates_, spelling);
        if (index == candidates_.size()) {
            return false;
        }
        out_ += 'S';
        out_ += index == 0 ? "" : digits(index - 1, 36);
        out_ += '_';
        return true;
    }

    // Single-letter builtin spelling; Size is unsigned long on 64-bit targets.
    [[nodiscard]] constexpr char builtin_code(Kind kind) const {
        constexpr std::string_view codes = "vbchijm";
        return kind == Kind::size && !wide_ ? 'j' : codes[static_cast<std::size_t>(kind)];
    }

    // Symbol text built so far.
    std::string out_;
    // Substitution candidates in S_, S0_, S1_... order, keyed by canonical spelling.
    std::vector<std::string> candidates_;
    // True for 64-bit pointers.
    bool wide_;
};

// Microsoft C++ ABI encoder (Windows MSVC) with name and argument back-references.
class Microsoft {
  public:
    // Pointer width selects Size spelling and __ptr64 pointer markers.
    constexpr explicit Microsoft(bool wide) : wide_(wide) {}

    // Encode a __cdecl free function: name, YA, return type, parameters, empty throw spec.
    constexpr std::string function(std::string_view name, const TypeNode &result,
                                   std::span<const TypeNode *const> params) {
        out_ = "?";
        qualified(name);
        out_ += "YA";
        if (result.kind == Kind::enumeration) {
            out_ += "?A";
        }
        type(unqualified(result));
        for (const auto *param : params) {
            parameter(unqualified(*param));
        }
        out_ += params.empty() ? "XZ" : "@Z";
        return out_;
    }

  private:
    // Spell identifiers innermost first, terminated by '@'.
    constexpr void qualified(std::string_view name) {
        const auto parts = split(name);
        std::ranges::for_each(parts.rbegin(), parts.rend(), [this](std::string_view part) { identifier(part); });
        out_ += '@';
    }

    // Spell an identifier, or its digit when one of the first ten names repeats.
    constexpr void identifier(std::string_view name) {
        const auto index = position(names_, name);
        if (index < names_.size()) {
            out_ += digits(index, 10);
            return;
        }
        if (names_.size() < 10) {
            names_.push_back(name);
        }
        out_ += name;
        out_ += '@';
    }

    // Reuse an earlier multi-character parameter type by digit; results never enter this table.
    constexpr void parameter(const TypeNode &node) {
        const auto spelling = key(node);
        const auto index = position(arguments_, spelling);
        if (index < arguments_.size()) {
            out_ += digits(index, 10);
            return;
        }
        const auto start = out_.size();
        type(node);
        if (out_.size() - start > 1 && arguments_.size() < 10) {
            arguments_.push_back(spelling);
        }
    }

    // Encode a type; a const pointer beneath another pointer is spelled Q instead of P.
    constexpr void type(const TypeNode &node) {
        if (node.kind == Kind::enumeration) {
            out_ += "W4";
            qualified(node.name);
            return;
        }
        if (builtin(node)) {
            out_ += builtin_code(node.kind);
            return;
        }
        out_ += node.constant ? 'Q' : 'P';
        out_ += wide_ ? "E" : "";
        out_ += node.pointee->constant ? 'B' : 'A';
        type(*node.pointee);
    }

    // Builtin spelling; Size is unsigned __int64 on 64-bit targets.
    [[nodiscard]] constexpr std::string_view builtin_code(Kind kind) const {
        constexpr std::array<std::string_view, 7> codes{"X", "_N", "D", "E", "H", "I", "_K"};
        return kind == Kind::size && !wide_ ? "I" : codes[static_cast<std::size_t>(kind)];
    }

    // Symbol text built so far.
    std::string out_;
    // First ten distinct identifiers, referenced later by digit.
    std::vector<std::string_view> names_;
    // First ten multi-character parameter types, keyed by canonical spelling.
    std::vector<std::string> arguments_;
    // True for 64-bit pointers.
    bool wide_;
};
} // namespace detail

// Mangled symbol of `Return Name(Params...)`, precomputed at compile time for every target.
template <FixedString Name, typename Return, typename... Params> class Function {
  public:
    // Precomputed spelling for the emitted target.
    static constexpr std::string_view symbol(Target target) {
        if (target.scheme == Scheme::microsoft) {
            return target.wide ? view<Target{Scheme::microsoft, true}>() : view<Target{Scheme::microsoft, false}>();
        }
        return target.wide ? view<Target{Scheme::itanium, true}>() : view<Target{Scheme::itanium, false}>();
    }

    // Spelling for a target fixed at compile time.
    template <Target target> static constexpr std::string_view view() {
        return {storage<target>.data(), storage<target>.size()};
    }

  private:
    // Parameter type nodes in declaration order.
    static constexpr std::array<const detail::TypeNode *, sizeof...(Params)> parameters{&Params::node...};

    // Build the symbol text; only evaluated during constant evaluation of `storage`.
    static constexpr std::string encode(Target target) {
        if (target.scheme == Scheme::microsoft) {
            return detail::Microsoft{target.wide}.function(Name.view(), Return::node, parameters);
        }
        return detail::Itanium{target.wide}.function(Name.view(), parameters);
    }

    // Static storage for one target's symbol, sized exactly to its text.
    template <Target target>
    static constexpr auto storage = [] {
        std::array<char, encode(target).size()> text{};
        const auto source = encode(target);
        std::ranges::copy(source, text.begin());
        return text;
    }();
};
} // namespace erlang_aot::mangling
