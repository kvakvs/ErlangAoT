#include <erlang_aot/compiler/lexer.hpp>
#include "parsing/boost_parser.hpp"
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

namespace erlang_aot {
namespace {
namespace bp = boost::parser;
// Consume digits with optional single separators between them.
std::size_t digits(std::u32string_view input, bool letters)
{
    const auto digit = bp::char_(U'0', U'9');
    const auto extended = digit | bp::char_(U'a', U'z') | bp::char_(U'A', U'Z');
    auto current = input.begin();
    if (letters) { bp::prefix_parse(current, input.end(), bp::omit[extended >> *(-bp::lit(U'_') >> extended)]); }
    else { bp::prefix_parse(current, input.end(), bp::omit[digit >> *(-bp::lit(U'_') >> digit)]); }
    return static_cast<std::size_t>(current - input.begin());
}
// Convert a digit from Erlang's base-2 through base-36 alphabet.
unsigned digit_value(char32_t value)
{
    if (value >= U'a' && value <= U'z') { return static_cast<unsigned>(value - U'a') + 10; }
    if (value >= U'A' && value <= U'Z') { return static_cast<unsigned>(value - U'A') + 10; }
    return static_cast<unsigned>(value - U'0');
}
// Accumulate arbitrary precision using a small decimal representation.
void multiply_add(std::string& decimal, unsigned base, unsigned carry)
{
    for (auto digit = decimal.rbegin(); digit != decimal.rend(); ++digit) {
        const auto value = static_cast<unsigned>(*digit - '0') * base + carry;
        *digit = static_cast<char>('0' + value % 10); carry = value / 10;
    }
    while (carry != 0) { decimal.insert(decimal.begin(), static_cast<char>('0' + carry % 10)); carry /= 10; }
}
// Parse a based integer without depending on the future Erlang runtime.
Integer integer(std::u32string_view digits, unsigned base)
{
    std::string decimal = "0";
    for (const auto value : digits) {
        if (value == U'_') { continue; }
        const auto digit = digit_value(value);
        if (digit >= base) { throw std::invalid_argument("digit outside integer base"); }
        multiply_add(decimal, base, digit);
    }
    return {decimal};
}
// Parse floating values with a fixed locale and reject non-finite results.
double floating(std::u32string_view spelling)
{
    auto text = utf8(spelling);
    std::erase(text, '_');
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    double value = 0;
    input >> value;
    if (!input || !std::isfinite(value)) { throw std::invalid_argument("invalid floating-point value"); }
    return value;
}
// Follow OTP's based-float conversion: integer significand times a base power.
double based_value(std::u32string text, std::u32string exponent, unsigned base)
{
    if (base == 10) { return floating(text + exponent); }
    std::erase(text, U'_');
    while (text.starts_with(U"0") && text[1] != U'.') { text.erase(0, 1); }
    while (text.ends_with(U"0") && text[text.size() - 2] != U'.') { text.pop_back(); }
    const auto fraction = text.size() - text.find(U'.') - 1;
    std::erase(text, U'.');
    const auto decimal = integer(text, base).decimal;
    const auto mantissa = floating(std::u32string(decimal.begin(), decimal.end()));
    std::erase(exponent, U'_');
    const auto power = exponent.empty() ? 0.0 : std::stod(utf8(std::u32string_view(exponent).substr(1)));
    const auto result = mantissa * std::pow(static_cast<double>(base), power - static_cast<double>(fraction));
    if (!std::isfinite(result)) { throw std::invalid_argument("based float exceeds finite range"); }
    return result;
}
} // namespace

Token Lexer::number()
{
    const auto begin = cursor_;
    cursor_ += digits(rest(), false);
    try {
        if (rest().starts_with(U"#")) { return based_number(begin); }
        if (rest().size() > 1 && rest()[0] == U'.' && rest()[1] >= U'0' && rest()[1] <= U'9') {
            return floating_number(begin);
        }
        number_end();
        return token(TokenKind::integer, integer(std::u32string_view(source_->text).substr(begin, cursor_ - begin), 10), begin, cursor_);
    } catch (const std::invalid_argument& error) { fail(DiagnosticCode::invalid_number, error.what(), begin); }
    catch (const std::out_of_range& error) { fail(DiagnosticCode::invalid_number, error.what(), begin); }
}

Token Lexer::based_number(std::size_t begin)
{
    const auto base_text = integer(std::u32string_view(source_->text).substr(begin, cursor_ - begin), 10).decimal;
    if (base_text.size() > 2) { throw std::invalid_argument("invalid integer base"); }
    const auto base = static_cast<unsigned>(std::stoul(base_text));
    if (base < 2 || base > 36) { throw std::invalid_argument("integer base must be 2 through 36"); }
    const auto value_begin = ++cursor_;
    cursor_ += digits(rest(), true);
    if (cursor_ == value_begin) { throw std::invalid_argument("missing based integer digits"); }
    if (rest().size() > 1 && rest().front() == U'.' && rest()[1] < 128 && word_length(rest().substr(1)) != 0) {
        return based_float(begin, BasedMantissa{value_begin, base});
    }
    number_end();
    return token(TokenKind::integer, integer(std::u32string_view(source_->text).substr(value_begin, cursor_ - value_begin), base), begin, cursor_);
}

// Reject adjacent name characters as OTP does, rather than splitting bad numbers.
void Lexer::number_end() const
{
    if (!rest().empty() && rest().front() < 128 && word_length(rest()) != 0) {
        throw std::invalid_argument("name character after number");
    }
}

// Consume a decimal exponent after the appropriate float marker.
std::u32string Lexer::exponent(bool based)
{
    if (based) {
        if (!rest().starts_with(U"#e") && !rest().starts_with(U"#E")) { return {}; }
        ++cursor_;
    } else if (!rest().starts_with(U"e") && !rest().starts_with(U"E")) { return {}; }
    const auto begin = cursor_++;
    if (rest().starts_with(U"+") || rest().starts_with(U"-")) { ++cursor_; }
    const auto count = digits(rest(), false);
    cursor_ += count;
    if (count == 0) { throw std::invalid_argument("missing exponent digits"); }
    return source_->text.substr(begin, cursor_ - begin);
}

Token Lexer::based_float(std::size_t begin, BasedMantissa syntax)
{
    ++cursor_;
    const auto count = digits(rest(), true);
    cursor_ += count;
    if (count == 0) { throw std::invalid_argument("missing fraction digits"); }
    const auto mantissa = source_->text.substr(syntax.begin, cursor_ - syntax.begin);
    const auto power = exponent(true);
    number_end();
    return token(TokenKind::floating, based_value(mantissa, power, syntax.base), begin, cursor_);
}

Token Lexer::floating_number(std::size_t begin)
{
    ++cursor_;
    cursor_ += digits(rest(), false);
    static_cast<void>(exponent(false));
    number_end();
    return token(TokenKind::floating, floating(std::u32string_view(source_->text).substr(begin, cursor_ - begin)), begin, cursor_);
}
} // namespace erlang_aot
