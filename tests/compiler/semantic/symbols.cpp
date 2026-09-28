#include "semantic/symbols.hpp"
#include <array>
#include <set>
#include <stdexcept>
using namespace erlang_aot::semantic;

// Symbols cannot be inspected through emitted Erlang objects until lowering is implemented.
int main() {
    const std::array<std::string, 7> names{"", "a", "a_b", "ab", "'quoted'", "lambda_\xce\xbb", "_0"};
    std::set<std::string> encoded;
    for (const auto &module : names) {
        for (const auto &function : names) {
            for (const std::size_t arity : {0U, 1U, 255U}) {
                const SymbolIdentity identity{module, function, arity};
                const auto name = encode_symbol(identity);
                if (!encoded.insert(name).second || decode_symbol(name) != identity) {
                    throw std::runtime_error("symbol collision or failed round trip");
                }
            }
        }
    }
    for (const auto invalid :
         {"other_61_62_0", "eav1_6_62_0", "eav1_61_62_256", "eav1_61_62_00", "eav1_zz_62_0", "eav1_61_62_-1"}) {
        if (decode_symbol(invalid)) {
            throw std::runtime_error("accepted malformed symbol");
        }
    }
}
