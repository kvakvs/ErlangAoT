#pragma once
#include <array>
#include <clause/compiler/ast/module.hpp>
#include <cstdint>
#include <string>

namespace clause::semantic {
struct ModuleFacts {
    // Escript sources implicitly export main/1.
    bool escript_ = false;
    // The module's absolute source path and the compiler version, reported by module_info(compile).
    std::string source_;
    std::string version_;
    // A digest of the module's code standing in for OTP's BEAM MD5; it also derives a missing -vsn.
    std::array<std::uint8_t, 16> md5_{};
};

// OTP's predefined module_info/0,1 as Erlang source returning literal data; an arity the module defines itself is
// left out (and diagnosed later). `behaviour_info` tells whether behaviour_info/1 is generated before them.
std::string module_info_source(const ast::Module &syntax, const ModuleFacts &facts, bool behaviour_info);
} // namespace clause::semantic
