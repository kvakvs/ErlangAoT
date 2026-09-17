#include <erlang_aot/compiler/source.hpp>
#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <utility>

namespace erlang_aot {
namespace {
// Match a coding marker only in the comment portion of a physical line.
std::optional<Encoding> encoding_comment(std::string_view line)
{
    const auto comment = line.find('%');
    if (comment == std::string_view::npos) { return std::nullopt; }
    line.remove_prefix(comment);
    const std::regex marker(R"(coding\s*[:=]\s*([-a-zA-Z0-9]+))");
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(line.begin(), line.end(), match, marker)) { return std::nullopt; }
    const std::regex latin("latin-1", std::regex::icase);
    return std::regex_match(match[1].str(), latin) ? Encoding::latin1 : Encoding::utf8;
}
// Recognize the first encoding marker in a comment on the first two lines.
Encoding detect_encoding(std::string_view bytes)
{
    for (int line = 0; line < 2; ++line) {
        const auto end = bytes.find('\n');
        const auto current = bytes.substr(0, end);
        if (const auto encoding = encoding_comment(current)) { return *encoding; }
        if (end == std::string_view::npos) { break; }
        bytes.remove_prefix(end + 1);
    }
    return Encoding::utf8;
}

// Classify UTF-8 leaders without accepting overlong two-byte sequences.
unsigned sequence_length(unsigned char first)
{
    if (first < 0x80) { return 1; }
    if (first >= 0xc2 && first <= 0xdf) { return 2; }
    if (first >= 0xe0 && first <= 0xef) { return 3; }
    if (first >= 0xf0 && first <= 0xf4) { return 4; }
    return 0;
}

// Reject non-scalar Unicode values and overlong UTF-8 sequences.
bool scalar(char32_t value, unsigned length)
{
    constexpr std::array<char32_t, 5> minimum{0, 0, 0x80, 0x800, 0x10000};
    return value >= minimum[length] && value <= 0x10ffff
        && !(value >= 0xd800 && value <= 0xdfff);
}

// Consume exactly one UTF-8 scalar, reporting the beginning of malformed input.
char32_t decode(std::string_view bytes, std::size_t& offset)
{
    const auto start = offset;
    const auto first = static_cast<unsigned char>(bytes[offset]);
    const auto length = sequence_length(first);
    if (length == 0 || bytes.size() - offset < length) {
        throw EncodingError(start, "invalid UTF-8 sequence");
    }
    constexpr std::array<unsigned, 5> masks{0, 0x7f, 0x1f, 0x0f, 0x07};
    auto value = static_cast<char32_t>(first & masks[length]);
    for (unsigned part = 1; part < length; ++part) {
        const auto byte = static_cast<unsigned char>(bytes[offset + part]);
        if ((byte & 0xc0) != 0x80) { throw EncodingError(start, "invalid UTF-8 continuation"); }
        value = (value << 6) | (byte & 0x3f);
    }
    if (!scalar(value, length)) { throw EncodingError(start, "invalid Unicode scalar"); }
    offset += length;
    return value;
}
} // namespace

EncodingError::EncodingError(std::size_t offset, const std::string& message)
    : std::runtime_error(message), byte(offset) {}

Source::Source(std::size_t identity, std::string filename, std::string contents)
    : id(identity), name(std::move(filename)), bytes(std::move(contents)), encoding(detect_encoding(bytes))
{
    Position current{0, 1, 1};
    while (current.byte < bytes.size()) {
        positions_.push_back(current);
        const auto value = encoding == Encoding::latin1
            ? static_cast<char32_t>(static_cast<unsigned char>(bytes[current.byte++]))
            : decode(bytes, current.byte);
        text.push_back(value);
        if (value == U'\n') { ++current.line; current.column = 1; }
        else { ++current.column; }
    }
    positions_.push_back(current);
}

Position Source::position(std::size_t offset) const { return positions_.at(offset); }

std::string_view Source::spelling(std::size_t begin, std::size_t end) const
{
    return std::string_view(bytes).substr(position(begin).byte, position(end).byte - position(begin).byte);
}

SourcePtr SourceManager::add(std::string name, std::string bytes)
{
    auto source = std::make_shared<Source>(sources_.size(), std::move(name), std::move(bytes));
    sources_.push_back(source);
    return source;
}

SourcePtr SourceManager::read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) { throw std::runtime_error("cannot read source: " + path.string()); }
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad()) { throw std::runtime_error("failed reading source: " + path.string()); }
    return add(path.string(), std::move(bytes));
}

std::string utf8(std::u32string_view text)
{
    std::string result;
    for (const auto value : text) {
        if (value < 0x80) { result.push_back(static_cast<char>(value)); continue; }
        const unsigned count = value < 0x800 ? 2 : (value < 0x10000 ? 3 : 4);
        constexpr std::array<unsigned, 5> leaders{0, 0, 0xc0, 0xe0, 0xf0};
        result.push_back(static_cast<char>(leaders[count] | (value >> (6 * (count - 1)))));
        for (unsigned part = count - 1; part > 0; --part) {
            result.push_back(static_cast<char>(0x80 | ((value >> (6 * (part - 1))) & 0x3f)));
        }
    }
    return result;
}
} // namespace erlang_aot
