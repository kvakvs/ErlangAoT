#pragma once
#include <deque>
#include <erlang_aot/compiler/diagnostic.hpp>
#include <optional>
#include <set>
#include <variant>

namespace erlang_aot {
enum class TokenKind : std::uint8_t {
    atom,
    variable,
    integer,
    floating,
    character,
    string,
    sigil_prefix,
    sigil_suffix,
    keyword,
    symbol,
    dot,
    comment
};

struct Integer {
    // Store canonical decimal digits without limiting Erlang integer precision.
    std::string decimal;
    bool operator==(const Integer &) const = default;
};

using TokenValue = std::variant<std::u32string, Integer, double>;

struct LogicalLocation {
    // Preserve logical coordinates separately from the physical spelling span.
    std::string file;
    std::size_t line;
    std::size_t column;
};

struct Token {
    // Describe the lexical category and decoded value.
    TokenKind kind;
    TokenValue value;
    // Keep original spelling and logical position even after source owners
    // exit.
    Span spelling;
    LogicalLocation location;
    // Later expansion/include stages can attach origins without rewriting
    // spelling.
    std::vector<Span> origins;
    // Return decoded text for textual categories, or an empty view for numbers.
    std::u32string_view text() const;
};

class Lexer {
  public:
    // Own the scanned source and choose whether comments are returned.
    explicit Lexer(SourcePtr source, bool comments = false);
    // Scan one token; EOF is empty and malformed input throws LexicalError.
    std::optional<Token> next();
    // Collect through the next form-ending dot, leaving later forms unscanned.
    std::vector<Token> form();
    // Discard through the next lexical form boundary after an error, or stop at
    // EOF.
    void recover_form();
    // Change keyword classification before scanning the next form.
    void set_keywords(std::set<std::u32string> keywords);
    // Map subsequent tokens to a logical filename and line at the current
    // cursor.
    void set_location(std::string file, std::size_t line);
    // Expose the current decoded offset for progress and recovery checks.
    std::size_t offset() const;

  private:
    struct Indentation {
        // Distinguish the closing delimiter's prefix from multiline content.
        std::u32string_view prefix;
    };

    struct BasedMantissa {
        // Locate the numeric significand and identify its digit alphabet.
        std::size_t begin;
        unsigned base;
    };

    // Shared source lifetime and the next unconsumed decoded character.
    SourcePtr source_;
    std::size_t cursor_ = 0;
    // Control comment reporting and feature-sensitive reserved words.
    bool comments_;
    std::set<std::u32string> keywords_;
    // Map physical line movement onto a caller-selected logical origin.
    std::string logical_file_;
    std::size_t physical_base_ = 1;
    std::size_t logical_base_ = 1;
    // Sigils produce prefix, string, and suffix tokens from one lexical unit.
    std::deque<Token> pending_;
    // Reject adjacent quoted strings without separating whitespace/comments.
    bool previous_string_ = false;

    // Inspect the remaining characters without extending their lifetime.
    std::u32string_view rest() const;
    // Create a token with shared spelling and current logical coordinates.
    Token token(TokenKind kind, TokenValue value, std::size_t begin, std::size_t end) const;
    // Raise a diagnostic at a valid source boundary.
    [[noreturn]] void fail(DiagnosticCode code, std::string message, std::size_t begin) const;
    // Consume each lexical category without parsing Erlang expressions.
    Token word();
    Token number();
    Token based_number(std::size_t begin);
    Token based_float(std::size_t begin, BasedMantissa syntax);
    std::u32string exponent(bool based);
    void number_end() const;
    Token floating_number(std::size_t begin);
    Token punctuation();
    Token character();
    Token quoted(char32_t delimiter, bool verbatim, TokenKind kind);
    Token triple(bool verbatim);
    Token triple_content(std::size_t begin, std::u32string_view delimiter, bool verbatim);
    Token sigil();
    Token literal();
    // Decode escapes and normalized multiline content.
    char32_t escape();
    std::u32string strip_indent(std::u32string_view text, Indentation indent, std::size_t begin) const;
    // Skip whitespace and optionally return a comment token.
    std::optional<Token> trivia();
};

// Match Erlang lexical categories using reusable Boost.Parser character rules.
std::size_t word_length(std::u32string_view input);
bool atom_start(char32_t value);
bool variable_start(char32_t value);
bool whitespace(char32_t value);
} // namespace erlang_aot
