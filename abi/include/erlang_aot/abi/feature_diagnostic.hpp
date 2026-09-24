#pragma once
#include "features.hpp"
#include <cstddef>
#include <string>

namespace erlang_aot::abi::v1 {
struct FeatureContext {
    // Borrow available source coordinates for this synchronous report; zero coordinates mean unknown.
    std::string_view source{};
    std::size_t line = 0;
    std::size_t column = 0;
    // Borrow optional semantic module, selected target and attempted operation descriptions.
    std::string_view module{};
    std::string_view target{};
    std::string_view operation{};
};

namespace detail {
// Escape control bytes and delimiters so user-controlled names cannot create extra diagnostic lines.
inline void append_escaped(std::string &output, std::string_view input) {
    constexpr std::string_view hex = "0123456789abcdef";
    for (const unsigned char byte : input) {
        if (byte < 32 || byte == 127) {
            output += "\\x";
            output += hex[byte >> 4];
            output += hex[byte & 15];
        } else {
            if (byte == '\\' || byte == '"') {
                output += '\\';
            }
            output += static_cast<char>(byte);
        }
    }
}

enum class ContextField : std::uint8_t { module, target, operation };

// Add labeled context only when known; the enum prevents accidentally swapping the label and value.
inline void append_field(std::string &output, ContextField field, std::string_view value) {
    constexpr std::array<std::string_view, 3> labels{"module", "target", "operation"};
    if (value.empty()) {
        return;
    }
    output += " [";
    output += labels[static_cast<std::size_t>(field)];
    output += "=\"";
    append_escaped(output, value);
    output += "\"]";
}

// Source-only messages retain the familiar file:line:column spelling from frontend diagnostics.
inline void append_source(std::string &output, const FeatureContext &context) {
    if (context.source.empty()) {
        return;
    }
    output += ": ";
    append_escaped(output, context.source);
    if (context.line != 0) {
        output += ':' + std::to_string(context.line);
    }
    if (context.column != 0 && context.line != 0) {
        output += ':' + std::to_string(context.column);
    }
}
} // namespace detail

// Owners call this once and retain/emit the same message; callers must propagate the resulting failure.
inline std::string format_feature_failure(FeatureId id, const FeatureContext &context = {}) {
    const auto *feature = find_feature(id);
    std::string message;
    if (feature == nullptr) {
        message = "invalid deferred feature ID " + std::to_string(static_cast<std::uint32_t>(id));
    } else {
        message = '[' + std::string(feature->name) + "] notimpl";
    }
    detail::append_source(message, context);
    detail::append_field(message, detail::ContextField::module, context.module);
    detail::append_field(message, detail::ContextField::target, context.target);
    detail::append_field(message, detail::ContextField::operation, context.operation);
    return message;
}
} // namespace erlang_aot::abi::v1
