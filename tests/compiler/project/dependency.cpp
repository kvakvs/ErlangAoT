#include <stdexcept>
#include <toml++/toml.hpp>

// Verify the pinned dependency accepts TOML 1.0 and rejects malformed input.
int main() {
    const auto document = toml::parse("schema_version = 1");
    if (document["schema_version"].value<int>() != 1) {
        throw std::runtime_error("TOML value lost");
    }
    try {
        (void)toml::parse("broken = [");
    } catch (const toml::parse_error &) {
        return 0;
    }
    throw std::runtime_error("Invalid TOML accepted");
}
