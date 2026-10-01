#include "lowering_support.hpp"

// Verify emitted bytes are a native object containing executable code.
void inspect_object(cg::Compilation &compilation) {
    require(cg::emit_objects(compilation), "real-source object emission failed");
    const auto &output = compilation.result().outputs().front();
    const llvm::StringRef bytes(reinterpret_cast<const char *>(output.bytes.data()), output.bytes.size());
    auto object = llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(bytes, "fixture"));
    if (!object) {
        throw std::runtime_error(llvm::toString(object.takeError()));
    }
    bool executable = false;
    for (const auto &section : (*object)->sections()) {
        executable |= section.isText() && section.getSize() != 0;
    }
    require(executable, "real-source object has no code");
}

// Compare decoded returns, including both representable endpoints, independently of inferred types.
void constants(const char *name, const std::string &triple, unsigned bits) {
    auto compilation = fixture(name, triple);
    require(analyze_and_lower(compilation), "constant lowering failed");
    const auto minimum = -(std::int64_t{1} << (bits - 5));
    const std::vector<std::int64_t> expected{42, -42, 0, minimum, -minimum - 1};
    std::size_t index = 0;
    for (const auto &function : *cg::detail::state(compilation).modules.front()) {
        if (!semantic::decode_symbol(function.getName().str())) {
            continue;
        }
        const auto *ret = llvm::cast<llvm::ReturnInst>(function.getEntryBlock().getTerminator());
        const auto &value = llvm::cast<llvm::ConstantInt>(ret->getReturnValue())->getValue();
        require(value.getBitWidth() == bits, "literal uses host width");
        require((value.getZExtValue() & 15U) == 15U, "literal lost its integer tag");
        require(value.ashr(4).getSExtValue() == expected.at(index++), "literal was truncated or mistagged");
        require(function.getCallingConv() == llvm::CallingConv::C && function.arg_size() == 2, "wrong entry ABI");
    }
    require(index == expected.size(), "missing constant definition");
    inspect_object(compilation);
}

// A host-valid literal must still be rejected when compiling for a narrower target.
void target_overflow() {
    auto compilation = fixture("constants64.erl", "i686-unknown-linux-gnu");
    require(!analyze_and_lower(compilation), "target overflow accepted");
    require(compilation.result().status() == cg::CompilationStatus::failed, "lowering did not latch failure");
    require(!cg::emit_objects(compilation) && compilation.result().outputs().empty(), "failed batch emitted output");
}

// Project source positions directly; equal argument values cannot collapse distinct slots.
void parameters(const std::string &triple = {}, unsigned bits = sizeof(void *) * 8) {
    auto compilation = fixture("parameters.erl", triple);
    require(analyze_and_lower(compilation), "parameter lowering failed");
    const std::vector<std::uint64_t> expected{0, 0, 1, 2};
    std::size_t index = 0;
    for (const auto &function : *cg::detail::state(compilation).modules.front()) {
        if (!semantic::decode_symbol(function.getName().str())) {
            continue;
        }
        auto instruction = function.getEntryBlock().begin();
        const auto *slot = llvm::cast<llvm::GetElementPtrInst>(&*instruction++);
        const auto *load = llvm::cast<llvm::LoadInst>(&*instruction++);
        const auto *ret = llvm::cast<llvm::ReturnInst>(&*instruction);
        require(slot->hasOneUse() && *slot->user_begin() == load && ret->getReturnValue() == load,
                "projection changed the term");
        require(slot->getPointerOperand() == function.getArg(1), "parameter lost argument array");
        require(!slot->isInBounds(), "parameter adds an unjustified pointer promise");
        const auto offset = llvm::cast<llvm::ConstantInt>(slot->getOperand(1))->getZExtValue();
        require(offset == expected.at(index++), "parameter order changed");
        require(load->getAlign().value() == bits / 8, "parameter alignment differs from ABI");
    }
    require(index == expected.size(), "missing projection");
    inspect_object(compilation);
}

// Recover call targets through LLVM use lists, preserving exact decoded source identities.
std::map<const llvm::User *, std::string> call_targets(const llvm::Module &module) {
    std::map<const llvm::User *, std::string> result;
    for (const auto &function : module) {
        const auto identity = semantic::decode_symbol(function.getName().str());
        if (!identity) {
            continue;
        }
        for (const auto *user : function.users()) {
            result.emplace(user, identity->function);
        }
    }
    return result;
}

// Inspect source-order nested calls and the exact context forwarded at every ABI boundary.
void local_calls() {
    auto compilation = fixture("calls.erl");
    require(analyze_and_lower(compilation), "local call lowering failed");
    auto &module = *cg::detail::state(compilation).modules.front();
    auto *entry = module.getFunction(semantic::encode_symbol({"calls", "value", 0}));
    const auto targets = call_targets(module);
    std::vector<std::string> names;
    for (const auto &block : *entry) {
        for (const auto &instruction : block) {
            if (const auto *call = llvm::dyn_cast<llvm::CallInst>(&instruction); call && targets.contains(call)) {
                names.push_back(targets.at(call));
                require(std::ranges::count(entry->getArg(0)->users(), call) == 1, "call lost process context");
                require(call->getCallingConv() == llvm::CallingConv::C, "call uses wrong convention");
            }
        }
    }
    for (const auto &use : entry->getArg(0)->uses()) {
        require(use.getOperandNo() == 0, "call moved the process context");
    }
    require(names == std::vector<std::string>{"left", "right", "identity", "left", "project"},
            "nested calls changed source evaluation order");
    auto *equal = module.getFunction(semantic::encode_symbol({"calls", "equal", 0}));
    std::size_t stores = 0;
    for (const auto &instruction : equal->getEntryBlock()) {
        if (const auto *store = llvm::dyn_cast<llvm::StoreInst>(&instruction)) {
            const auto *value = llvm::cast<llvm::ConstantInt>(store->getValueOperand());
            require(value->getValue().ashr(4).getSExtValue() == 7, "identical call argument changed");
            ++stores;
        }
    }
    require(stores == 3, "identical arguments lost their distinct positions");
    inspect_object(compilation);
}

// Check separately emitted definitions/imports without invoking a native linker.
void imported_symbols(const cg::OutputBuffer &output, const std::string &symbol_name, bool undefined) {
    const llvm::StringRef bytes(reinterpret_cast<const char *>(output.bytes.data()), output.bytes.size());
    auto object = llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(bytes, "remote"));
    if (!object) {
        throw std::runtime_error(llvm::toString(object.takeError()));
    }
    bool found = false;
    for (const auto &symbol : (*object)->symbols()) {
        auto name = symbol.getName();
        if (!name) {
            throw std::runtime_error(llvm::toString(name.takeError()));
        }
        auto flags = symbol.getFlags();
        if (!flags) {
            throw std::runtime_error(llvm::toString(flags.takeError()));
        }
        if (*name == symbol_name || *name == "_" + symbol_name) {
            require(((*flags & llvm::object::SymbolRef::SF_Undefined) != 0) == undefined,
                    "object definition/import mismatch");
            found = true;
        }
    }
    require(found, "object lost cross-module symbol");
}

// Keep forward batch references external and match the defining module's generic signature.
void remote_calls() {
    auto compilation = fixtures({"client.erl", "answer.erl"});
    require(analyze_and_lower(compilation), "remote call lowering failed");
    auto &modules = cg::detail::state(compilation).modules;
    for (const auto &name : {"value", "identity"}) {
        const auto symbol = semantic::encode_symbol({"answer", name, name == std::string_view("identity") ? 1U : 0U});
        const auto *imported = modules.front()->getFunction(symbol);
        const auto *defined = modules.back()->getFunction(symbol);
        require(imported && imported->isDeclaration() && imported->hasExternalLinkage(),
                "missing external declaration");
        require(defined && !defined->isDeclaration() && defined->hasExternalLinkage(), "missing exported definition");
        require(imported->getFunctionType() == defined->getFunctionType(), "remote ABI signature mismatch");
    }
    require(!modules.front()->getFunction(semantic::encode_symbol({"answer", "private", 0})),
            "imported private function");
    inspect_object(compilation);
    const auto outputs = compilation.result().outputs();
    require(outputs.size() == 2, "batch did not retain separate objects");
    for (const auto &identity : {semantic::SymbolIdentity{"answer", "value", 0}, {"answer", "identity", 1}}) {
        const auto symbol = semantic::encode_symbol(identity);
        imported_symbols(outputs.front(), symbol, true);
        imported_symbols(outputs.back(), symbol, false);
    }
    auto reversed = fixtures({"answer.erl", "client.erl"});
    require(analyze_and_lower(reversed), "batch order affected remote lowering");
    inspect_object(reversed);
}

// Check source-to-object behavior without publishing artifacts or linking a program.
int main() {
    try {
        constants(sizeof(void *) == 8 ? "constants64.erl" : "constants32.erl", "", sizeof(void *) * 8);
#ifdef ERLANG_AOT_LLVM_X86
        constants("constants32.erl", "i686-unknown-linux-gnu", 32);
        constants("constants64.erl", "x86_64-unknown-linux-gnu", 64);
        target_overflow();
        parameters("i686-unknown-linux-gnu", 32);
#endif
        parameters();
        local_calls();
        remote_calls();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected lowering failure\n", stderr);
        return 1;
    }
}
