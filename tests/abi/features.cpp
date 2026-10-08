#include <erlang_aot/abi/feature_diagnostic.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot::abi::v1;

// Check the catalog as a compatibility snapshot, not only against its own lookup implementation.
constexpr std::array<std::string_view, 26> names{"pattern matching",
                                                 "guards",
                                                 "multiple clauses",
                                                 "arithmetic",
                                                 "bignum expressions",
                                                 "atom expressions",
                                                 "heap expressions",
                                                 "dynamic calls",
                                                 "recursive calls",
                                                 "closures",
                                                 "exceptions",
                                                 "receive",
                                                 "behavior-changing attributes",
                                                 "term services",
                                                 "builtins",
                                                 "process execution",
                                                 "message passing",
                                                 "scheduling",
                                                 "allocation",
                                                 "garbage collection",
                                                 "atom collection",
                                                 "dynamic modules",
                                                 "executable linking",
                                                 "send expressions",
                                                 "expression sequences",
                                                 "ports"};

// Keep assertions active under NDEBUG.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Preserve stable ID spelling without coupling runtime compatibility to test names or plan numbering.
void check_catalog() {
    require(feature_catalog.size() == names.size(), "update the catalog compatibility snapshot");
    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto *feature = find_feature(static_cast<FeatureId>(index + 1));
        require(feature != nullptr, "stable feature ID missing");
        require(feature->name == names[index], "stable feature spelling changed");
    }
    require(find_feature(FeatureId::invalid) == nullptr, "zero became a valid feature");
}

// This ABI-only executable proves catalog/formatting need neither compiler nor runtime libraries.
int main() {
    try {
        check_catalog();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
