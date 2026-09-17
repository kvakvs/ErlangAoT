#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace erlang_aot {
enum class Encoding : std::uint8_t { utf8, latin1 };

struct Position {
    // Physical byte offset and one-based Unicode character coordinates.
    std::size_t byte;
    std::size_t line;
    std::size_t column;
};

class EncodingError : public std::runtime_error {
  public:
    // Preserve the invalid byte position for source-loading diagnostics.
    EncodingError(std::size_t byte, const std::string &message);
    const std::size_t byte;
};

class Source {
  public:
    // Decode once while retaining an immutable copy of the original bytes.
    Source(std::size_t id, std::string name, std::string bytes);
    // Map a decoded character offset, including EOF, back to physical input.
    Position position(std::size_t offset) const;
    // Return original bytes for a half-open decoded character range.
    std::string_view spelling(std::size_t begin, std::size_t end) const;
    // Identify a buffer independently of the caller's filesystem spelling.
    const std::size_t id;
    const std::string name;
    // Preserve bytes, decoded characters, and the selected file encoding.
    const std::string bytes;
    Encoding encoding = Encoding::utf8;
    std::u32string text;

  private:
    // Include an EOF entry so every token boundary has a physical position.
    std::vector<Position> positions_;
};

using SourcePtr = std::shared_ptr<const Source>;

class SourceManager {
  public:
    // Register an in-memory buffer with a stable identity and shared lifetime.
    SourcePtr add(std::string name, std::string bytes);
    // Read bytes without platform newline conversion.
    SourcePtr read(const std::filesystem::path &path);

  private:
    // Retain source buffers until the manager and all token owners release
    // them.
    std::vector<SourcePtr> sources_;
};

// Encode decoded token values for diagnostics and private test output.
std::string utf8(std::u32string_view text);
} // namespace erlang_aot
