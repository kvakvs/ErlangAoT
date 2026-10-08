#include "text.hpp"
#include "typed.hpp"
#include <array>
#include <cstdlib>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// The os builtins of the bridge (docs/ports.md#subprocesses): os:type/0 and os:getenv/1; os:cmd/1 is library
// Erlang over a port.
namespace clause::runtime::builtins {
namespace {
// The atom `name`.
Term atom(ProcessContext &context, std::string_view name) { return need(TermFactory(context).atom(name)); }

// os:type(): {win32, nt}, {unix, linux} or {unix, darwin}.
TermResult<Term> type(ProcessContext &context) {
#ifdef _WIN32
    const std::array names{atom(context, "win32"), atom(context, "nt")};
#elif defined(__APPLE__)
    const std::array names{atom(context, "unix"), atom(context, "darwin")};
#else
    const std::array names{atom(context, "unix"), atom(context, "linux")};
#endif
    return TermFactory(context).tuple(names);
}

// The value of environment variable `name` as UTF-8, if it is set.
std::optional<std::string> variable(const std::string &name) {
#ifdef _WIN32
    const auto wide = [](std::string_view text) {
        std::wstring result(text.size(), L'\0');
        const auto size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                                              static_cast<int>(result.size()));
        result.resize(static_cast<std::size_t>(size));
        return result;
    };
    const auto key = wide(name);
    const auto size = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (size == 0) {
        return std::nullopt;
    }
    std::wstring value(size, L'\0');
    value.resize(GetEnvironmentVariableW(key.c_str(), value.data(), size));
    std::string text(value.size() * 3, '\0');
    text.resize(
        static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                                     text.data(), static_cast<int>(text.size()), nullptr, nullptr)));
    return text;
#else
    const auto *value = std::getenv(name.c_str());
    return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

// os:getenv(Name): the value of the variable as a string, or false when it is not set; badarg unless Name is a
// string.
TermResult<Term> getenv(ProcessContext &context, const ListArgument &name) {
    std::u32string codes;
    for (const auto &code : name.elements) {
        const auto value = small(code.word());
        if (!value || !unicode_character(*value)) {
            bad_argument();
        }
        codes.push_back(static_cast<char32_t>(*value));
    }
    const auto value = variable(utf8(codes));
    if (!value) {
        return atom(context, "false");
    }
    std::vector<Term> characters;
    for (const auto code : code_points(*value)) {
        characters.push_back(need(Term::from_word(need(encode_integer(code)))));
    }
    return TermFactory(context).list(characters);
}

constexpr std::array OS_BUILTINS{
    typed_entry<type>("os", "type"),
    typed_entry<getenv>("os", "getenv"),
};
} // namespace
} // namespace clause::runtime::builtins

namespace clause::runtime {
std::span<const BuiltinEntry> os_builtins() noexcept { return builtins::OS_BUILTINS; }
} // namespace clause::runtime
