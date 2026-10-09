#include <array>
#include <chrono>
#include <clause/runtime/code_server.hpp>
#include <clause/runtime/runtime.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Code server lookups by hash index (plan step 62A): export frames by Module:Function/Arity atoms and record, fun
// and module bindings by descriptor, over many registered modules with many exports. Present, missing and
// wrong-arity names are checked, a rejected registration indexes nothing, and lookup cost is printed for few and
// many modules (descriptive, never a threshold).
namespace {
using namespace clause::runtime;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

constexpr std::size_t MODULES = 400;
constexpr std::size_t EXPORTS = 40;

// Stand-ins for the compiler's descriptors: only their addresses are keys, never dereferenced.
std::array<char, MODULES + 2> module_descriptors{};
std::array<char, MODULES + 2> record_descriptors{};
std::array<char, MODULES + 2> fun_descriptors{};
std::array<char, EXPORTS> frames{};

// The bindings of module `index`: its descriptor, one record, one fun, and fun_J/1 exports with distinct frames.
std::shared_ptr<ModuleAtoms> bindings(AtomStorage &atoms, std::size_t index, std::size_t exports) {
    auto result = std::make_shared<ModuleAtoms>();
    result->descriptor = &module_descriptors.at(index);
    result->module = atoms.intern("lookup_" + std::to_string(index)).value().word();
    result->slots.push_back(atoms.intern("lookup_" + std::to_string(index)).value());
    RecordDefinition record;
    record.descriptor = &record_descriptors.at(index);
    result->records.push_back(record);
    FunDefinition fun;
    fun.descriptor = &fun_descriptors.at(index);
    result->funs.push_back(fun);
    for (std::size_t item = 0; item < exports; ++item) {
        const auto function = atoms.intern("fun_" + std::to_string(item)).value().word();
        result->exports.push_back({.function = function, .arity = 1, .frame = &frames.at(item)});
    }
    return result;
}

// Register module `index` with its bindings.
CodeResult<std::shared_ptr<const LoadedModule>> load(CodeServer &server, std::string name,
                                                     std::shared_ptr<ModuleAtoms> atoms) {
    return server.load({std::move(name), CodeImage::linked(), std::make_unique<ModuleRegistry>(), std::move(atoms)});
}

// The frame of lookup_M:fun_F/A, or null.
const void *frame(CodeServer &server, AtomStorage &atoms, std::size_t module, std::size_t function, std::size_t arity) {
    const auto name = atoms.intern("lookup_" + std::to_string(module)).value().word();
    return server.export_frame({name, atoms.intern("fun_" + std::to_string(function)).value().word(), arity});
}

// Nanoseconds per export lookup of present names, averaged over many lookups.
double lookup_cost(CodeServer &server, AtomStorage &atoms, std::size_t modules) {
    std::vector<FunctionAtoms> names;
    for (std::size_t index = 0; index < 1'000; ++index) {
        const auto module = atoms.intern("lookup_" + std::to_string(index % modules)).value().word();
        names.push_back({module, atoms.intern("fun_" + std::to_string(index % EXPORTS)).value().word(), 1});
    }
    const auto start = std::chrono::steady_clock::now();
    std::size_t hits = 0;
    for (std::size_t round = 0; round < 200; ++round) {
        for (const auto &name : names) {
            hits += server.export_frame(name) != nullptr;
        }
    }
    const auto elapsed = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    require(hits == names.size() * 200, "a present export was not found");
    return elapsed / static_cast<double>(hits);
}

void lookups() {
    auto runtime = Runtime::start().value();
    auto &server = *runtime->code_server();
    auto &atoms = *runtime->atom_storage();
    for (std::size_t index = 0; index < 10; ++index) {
        require(load(server, "lookup_" + std::to_string(index), bindings(atoms, index, EXPORTS)).has_value(),
                "registration failed");
    }
    const auto few = lookup_cost(server, atoms, 10);
    for (std::size_t index = 10; index < MODULES; ++index) {
        require(load(server, "lookup_" + std::to_string(index), bindings(atoms, index, EXPORTS)).has_value(),
                "registration failed");
    }
    const auto many = lookup_cost(server, atoms, MODULES);
    for (const std::size_t module : {std::size_t{0}, MODULES / 2, MODULES - 1}) {
        require(frame(server, atoms, module, 7, 1) == &frames.at(7), "export frame not found");
        require(frame(server, atoms, module, 7, 2) == nullptr, "wrong arity found");
        require(frame(server, atoms, module, EXPORTS, 1) == nullptr, "missing function found");
        require(server.record_definition(&record_descriptors.at(module))->descriptor == &record_descriptors.at(module),
                "record definition not found");
        require(server.fun_definition(&fun_descriptors.at(module))->descriptor == &fun_descriptors.at(module),
                "fun definition not found");
        require(server.atom_word(&module_descriptors.at(module), 0).value() ==
                    atoms.intern("lookup_" + std::to_string(module)).value().word(),
                "atom slot not found");
    }
    require(frame(server, atoms, MODULES, 0, 1) == nullptr, "missing module found");
    require(server.record_definition(&record_descriptors.at(MODULES)) == nullptr, "unknown record descriptor found");
    require(server.fun_definition(&fun_descriptors.at(MODULES)) == nullptr, "unknown fun descriptor found");
    // A rejected registration indexes nothing: a duplicate name, and bindings reusing a registered descriptor.
    require(load(server, "lookup_0", bindings(atoms, MODULES, EXPORTS)).error() == CodeError::duplicate_module,
            "duplicate module accepted");
    auto reused = bindings(atoms, MODULES + 1, EXPORTS);
    reused->descriptor = &module_descriptors.at(0);
    require(load(server, "lookup_" + std::to_string(MODULES + 1), reused).error() == CodeError::invalid_module,
            "reused descriptor accepted");
    for (const std::size_t module : {MODULES, MODULES + 1}) {
        require(frame(server, atoms, module, 0, 1) == nullptr, "a rejected module's export was indexed");
        require(server.fun_definition(&fun_descriptors.at(module)) == nullptr, "a rejected module's fun was indexed");
    }
    std::cout << "export lookup: " << few << " ns with 10 modules, " << many << " ns with " << MODULES << " modules of "
              << EXPORTS << " exports (descriptive)\n";
}
} // namespace

int main() {
    try {
        lookups();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
