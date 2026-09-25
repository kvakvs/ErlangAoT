#include <algorithm>
#include <erlang_aot/compiler/lexer.hpp>
#include <utility>

namespace erlang_aot {
namespace {
// Accept Unicode scalar values for both ordinary and escaped characters.
bool valid_scalar(const char32_t value) { return value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff); }

// Decode the standard hexadecimal alphabet.
unsigned hex_digit(const char32_t value) {
    if (value >= U'0' && value <= U'9') {
        return static_cast<unsigned>(value - U'0');
    }
    if (value >= U'a' && value <= U'f') {
        return static_cast<unsigned>(value - U'a') + 10;
    }
    if (value >= U'A' && value <= U'F') {
        return static_cast<unsigned>(value - U'A') + 10;
    }
    return 16;
}

struct EscapeDigits {
    // Select the numeric alphabet and maximum number of consumed digits.
    unsigned base;
    std::size_t limit;
};

// Consume a bounded numeric escape without overflowing a Unicode scalar.
char32_t numeric_escape(std::u32string_view &input, const EscapeDigits syntax) {
    char32_t value = 0;
    std::size_t count = 0;
    while (!input.empty() && count < syntax.limit) {
        const auto digit = hex_digit(input.front());
        if (digit >= syntax.base) {
            break;
        }
        if (value > 0x10ffff / syntax.base) {
            throw std::invalid_argument("escape exceeds Unicode range");
        }
        value = value * syntax.base + digit;
        input.remove_prefix(1);
        ++count;
    }
    if (count == 0 || !valid_scalar(value)) {
        throw std::invalid_argument("invalid numeric escape");
    }
    return value;
}

// Handle both fixed-width and braced Erlang hexadecimal escapes.
char32_t hexadecimal(std::u32string_view &input) {
    if (input.starts_with(U'{')) {
        input.remove_prefix(1);
        const auto value = numeric_escape(input, {.base = 16, .limit = input.size()});
        if (!input.starts_with(U'}')) {
            throw std::invalid_argument("missing hexadecimal closing brace");
        }
        input.remove_prefix(1);
        return value;
    }
    const auto before = input.size();
    const auto value = numeric_escape(input, {.base = 16, .limit = 2});
    if (before - input.size() != 2) {
        throw std::invalid_argument("hexadecimal escape needs two digits");
    }
    return value;
}

// Translate a control-character escape using Erlang's accepted alphabet.
char32_t caret(std::u32string_view &input) {
    if (input.empty()) {
        throw std::invalid_argument("incomplete control escape");
    }
    const auto value = input.front();
    input.remove_prefix(1);
    if (value == U'?') {
        return 127;
    }
    if ((value >= U'@' && value <= U'_') || (value >= U'a' && value <= U'z')) {
        return value & 31u;
    }
    throw std::invalid_argument("invalid control escape");
}

// Decode one escape after the backslash; unknown simple escapes quote
// themselves.
char32_t escaped(std::u32string_view &input) {
    if (input.empty()) {
        throw std::invalid_argument("incomplete escape");
    }
    if (input.front() >= U'0' && input.front() <= U'7') {
        return numeric_escape(input, {.base = 8, .limit = 3});
    }
    const auto value = input.front();
    input.remove_prefix(1);
    if (value == U'x') {
        return hexadecimal(input);
    }
    if (value == U'^') {
        return caret(input);
    }
    constexpr std::u32string_view names = U"nrtvbfesd";
    constexpr std::u32string_view values = U"\n\r\t\v\b\f\x1b \x7f";
    const auto index = names.find(value);
    return index == std::u32string_view::npos ? value : values[index];
}

// Decode multiline content after applying indentation to physical lines.
std::u32string unescape(std::u32string_view input) {
    std::u32string result;
    while (!input.empty()) {
        const auto value = input.front();
        input.remove_prefix(1);
        result.push_back(value == U'\\' ? escaped(input) : value);
    }
    return result;
}

// Return the closing delimiter for an Erlang sigil, or zero for invalid syntax.
char32_t closing(const char32_t opening) {
    constexpr std::u32string_view openings = U"([{</|#`'\"";
    constexpr std::u32string_view closings = U")]}>/|#`'\"";
    const auto index = openings.find(opening);
    return index == std::u32string_view::npos ? 0 : closings[index];
}
} // namespace

char32_t Lexer::escape() {
    const auto begin = cursor_++;
    auto input = rest();
    try {
        const auto value = escaped(input);
        cursor_ = source_->text.size() - input.size();
        return value;
    } catch (const std::invalid_argument &error) {
        cursor_ = source_->text.size() - input.size();
        fail(DiagnosticCode::invalid_escape, error.what(), begin);
    }
}

Token Lexer::character() {
    const auto begin = cursor_++;
    if (rest().empty()) {
        fail(DiagnosticCode::unterminated_literal, "missing character after $", begin);
    }
    const auto value = rest().front() == U'\\' ? escape() : source_->text[cursor_++];
    return token(TokenKind::character, Integer{std::to_string(static_cast<unsigned>(value))}, begin, cursor_);
}

Token Lexer::quoted(const char32_t delimiter, const bool verbatim, const TokenKind kind) {
    const auto begin = cursor_++;
    std::u32string value;
    while (!rest().empty() && rest().front() != delimiter) {
        if (!verbatim && rest().front() == U'\\') {
            value.push_back(escape());
        } else {
            value.push_back(source_->text[cursor_++]);
        }
    }
    if (rest().empty()) {
        fail(DiagnosticCode::unterminated_literal, "unterminated quoted literal", begin);
    }
    ++cursor_;
    if (kind == TokenKind::atom && value.size() > 255) {
        fail(DiagnosticCode::invalid_character, "atom exceeds 255 characters", begin);
    }
    return token(kind, std::move(value), begin, cursor_);
}

std::u32string Lexer::strip_indent(const std::u32string_view text, const Indentation indent,
                                   const std::size_t begin) const {
    std::u32string result;
    std::u32string_view remaining(text);
    while (!remaining.empty()) {
        const auto newline = remaining.find(U'\n');
        const auto count = newline == std::u32string_view::npos ? remaining.size() : newline + 1;
        auto line = remaining.substr(0, count);
        if (line.starts_with(indent.prefix)) {
            line.remove_prefix(indent.prefix.size());
        } else if (line != U"\n" && line != U"\r\n") {
            fail(DiagnosticCode::string_indentation, "invalid multiline indentation", begin);
        }
        result += line;
        remaining.remove_prefix(count);
    }
    if (result.ends_with(U'\n')) {
        result.pop_back();
    }
    if (result.ends_with(U'\r')) {
        result.pop_back();
    }
    return result;
}

Token Lexer::triple(const bool verbatim) {
    const auto begin = cursor_;
    while (rest().starts_with(U'\"')) {
        ++cursor_;
    }
    const std::u32string delimiter(cursor_ - begin, U'"');
    const auto first_line = rest().find(U'\n');
    if (first_line == std::u32string_view::npos) {
        cursor_ = source_->text.size();
        fail(DiagnosticCode::unterminated_literal, "unterminated multiline literal", begin);
    }
    if (!std::ranges::all_of(rest().substr(0, first_line), whitespace)) {
        fail(DiagnosticCode::string_indentation, "text after multiline opening delimiter", begin);
    }
    cursor_ += first_line + 1;
    return triple_content(begin, delimiter, verbatim);
}

Token Lexer::triple_content(const std::size_t begin, const std::u32string_view delimiter, const bool verbatim) {
    const auto content = cursor_;
    while (!rest().empty() && !rest().starts_with(delimiter)) {
        if (!verbatim && rest().front() == U'\\') {
            static_cast<void>(escape());
        } else {
            ++cursor_;
        }
    }
    if (rest().empty()) {
        fail(DiagnosticCode::unterminated_literal, "unterminated multiline literal", begin);
    }
    const auto prefix = std::u32string_view(source_->text).substr(content, cursor_ - content);
    const auto newline = prefix.rfind(U'\n');
    const auto indent_begin = newline == std::u32string_view::npos ? 0 : newline + 1;
    const auto indent = prefix.substr(indent_begin);
    if (!std::ranges::all_of(indent, whitespace)) {
        fail(DiagnosticCode::string_indentation, "invalid closing indentation", cursor_);
    }
    auto value = strip_indent(prefix.substr(0, indent_begin), Indentation{indent}, begin);
    cursor_ += delimiter.size();
    if (!verbatim) {
        value = unescape(value);
    }
    return token(TokenKind::string, std::move(value), begin, cursor_);
}

Token Lexer::sigil() {
    const auto begin = cursor_++;
    const auto length = word_length(rest());
    const auto name = std::u32string(rest().substr(0, length));
    cursor_ += length;
    auto prefix = token(TokenKind::sigil_prefix, name, begin, cursor_);
    if (rest().empty() || closing(rest().front()) == 0) {
        fail(DiagnosticCode::invalid_character, "invalid sigil delimiter", begin);
    }
    const bool escaped_sigil = name == U"s" || name == U"b";
    auto value = rest().starts_with(U"\"\"\"")
                     ? triple(!escaped_sigil)
                     : quoted(closing(rest().front()), !(escaped_sigil || name.empty()), TokenKind::string);
    pending_.push_back(std::move(value));
    const auto suffix = cursor_;
    cursor_ += word_length(rest());
    pending_.push_back(token(TokenKind::sigil_suffix, source_->text.substr(suffix, cursor_ - suffix), suffix, cursor_));
    previous_string_ = cursor_ == suffix;
    return prefix;
}

Token Lexer::literal() {
    const auto first = rest().front();
    if (first == U'"') {
        if (previous_string_) {
            fail(DiagnosticCode::adjacent_strings, "adjacent strings need whitespace", cursor_);
        }
        previous_string_ = true;
        return rest().starts_with(U"\"\"\"") ? triple(true) : quoted(U'"', false, TokenKind::string);
    }
    previous_string_ = false;
    if (first == U'\'') {
        return quoted(U'\'', false, TokenKind::atom);
    }
    if (first == U'$') {
        return character();
    }
    if (first == U'~') {
        return sigil();
    }
    return punctuation();
}
} // namespace erlang_aot
