#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>

namespace {
using namespace erlang_aot::runtime;

// Keep validation active in optimized standalone runtime builds.
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

// Supply a real, validated immediate without invoking unimplemented heap factories.
Term integer(std::int64_t value) { return *Term::from_word(*encode_integer(value)); }

// Immediate host values copy without process roots and never admit fabricated runtime identities.
void check_terms() {
    for (const Word word : {*encode_integer(-42), Word{0x2b}, Word{0x3b}}) {
        const auto value = Term::from_word(word);
        require(value.has_value(), "supported immediate rejected");
        const auto copy = *value;
        require(copy.word() == word && copy.kind() == *classify_immediate(word), "term copy changed representation");
    }
    require(Term::from_word(0).error() == TermError::invalid_encoding, "invalid host word accepted");
    require(Term::from_word(0xb).error() == TermError::not_implemented, "unbound atom admitted");
    require(Term::from_word(1).error() == TermError::wrong_type, "heap word admitted");
}

// Forward one generic term with no decoding, encoding or fallback.
CallResult<Term> identity(ProcessContext &, std::span<const Term> arguments) { return arguments.front(); }

// Exercise exact keys, target state sharing, sorted metadata and fixed arities before publication.
void check_registry() {
    ModuleRegistry registry;
    require(registry.add("same", 1, identity).has_value(), "registration failed");
    require(registry.add("same", 1, identity) == std::unexpected(RegistryError::duplicate_key), "duplicate accepted");
    require(registry.add("same", 0, identity).has_value(), "distinct arity rejected");
    require(registry.add("", 1, identity) == std::unexpected(RegistryError::invalid_entry), "empty name accepted");
    require(registry.add("bad", 256, identity) == std::unexpected(RegistryError::invalid_entry), "bad arity accepted");
    require(registry.add("bad", 1, {}) == std::unexpected(RegistryError::invalid_entry), "empty target accepted");
    require(!registry.find("Same", 1) && !registry.find("same", 2), "lookup ignored exact name/arity");
    const auto keys = registry.keys();
    require(keys.size() == 2 && keys[0].argument_types.empty(), "zero arity metadata wrong");
    require(keys[1].argument_types == std::vector<std::type_index>{typeid(Term)}, "generic key is not all-Term");
}

// Publish the exact draft owner and prove that stale aliases can no longer mutate it.
void check_publication() {
    auto runtime = Runtime::start().value();
    auto *context = runtime->create_context().value();
    require(&context->code_server() == runtime->code_server(), "context has a second server");
    auto other = Runtime::start().value();
    require(other->code_server() != runtime->code_server(), "runtimes share a code server");
    auto registry = std::make_unique<ModuleRegistry>();
    auto *draft = registry.get();
    require(registry->add("id", 1, identity).has_value(), "fixture registration failed");
    auto module = runtime->code_server()->load({"demo", CodeImage::linked(), std::move(registry)});
    require(module.has_value() && &(*module)->functions() == draft, "publication copied the registry");
    require(draft->frozen(), "publication did not freeze");
    require(draft->add("later", 1, identity) == std::unexpected(RegistryError::frozen), "draft alias bypassed freeze");
    auto duplicate = runtime->code_server()->load({"demo", CodeImage::linked(), std::make_unique<ModuleRegistry>()});
    require(!duplicate && duplicate.error() == CodeError::duplicate_module, "duplicate module accepted");
    auto missing = runtime->code_server()->resolve({.module = "demo", .function = "missing", .arity = 0});
    require(!missing && missing.error() == CodeError::function_not_exported, "missing generic entry accepted");
    require(!runtime->code_server()->load({"invalid", {}, std::make_unique<ModuleRegistry>()}), "null image accepted");
    require(!runtime->code_server()->load({"invalid", CodeImage::linked(), {}}), "null registry accepted");
    require(!other->code_server()->find_module("demo"), "registration escaped its runtime");
    require(other->shutdown() == erlang_aot::abi::v1::Status::ok && other->code_server() == nullptr,
            "stopped runtime still exposes a server");
}

// Observe capture destruction while the executable image it depends on is still alive.
struct Capture final {
    // Borrow test-owned counters that outlive every module pin.
    bool &image_alive;
    bool &destroyed_safely;

    // Verify target/capture teardown precedes image teardown.
    ~Capture() { destroyed_safely = image_alive; }
};

struct Image final : CodeImage {
    // Borrow a test flag to expose the image's exact lifetime.
    bool &alive;

    // Mark the image as mapped until its last module owner releases it.
    explicit Image(bool &value) : alive(value) { alive = true; }

    // Simulate releasing executable storage.
    ~Image() override { alive = false; }
};

// Retain a callable past runtime destruction and prove its capture dies before executable storage.
void check_pinning() {
    bool alive = false;
    bool safe = false;
    std::optional<ResolvedFunction> pinned;
    {
        auto runtime = Runtime::start().value();
        auto registry = std::make_unique<ModuleRegistry>();
        auto capture = std::make_shared<Capture>(alive, safe);
        require(registry
                    ->add("state", 0,
                          [capture, count = 0](ProcessContext &, std::span<const Term>) mutable {
                              return CallResult<Term>(integer(++count));
                          })
                    .has_value(),
                "state registration failed");
        require(runtime->code_server()->load({"pin", std::make_shared<Image>(alive), std::move(registry)}).has_value(),
                "pin publication failed");
        pinned = runtime->code_server()->resolve({.module = "pin", .function = "state", .arity = 0}).value();
        auto second = runtime->code_server()->resolve({.module = "pin", .function = "state", .arity = 0}).value();
        auto *context = runtime->create_context().value();
        require(pinned->call(*context, {})->integer_value() == 1, "first state call failed");
        require(second.call(*context, {})->integer_value() == 2, "lookup copied callable state");
    }
    require(alive && !safe, "resolution did not pin target/image");
    pinned.reset();
    require(!alive && safe, "image destroyed before target capture");
}

// Check arguments before invoking a body, preserving explicit failures and host exceptions.
void check_calls() {
    auto runtime = Runtime::start().value();
    auto *context = runtime->create_context().value();
    auto registry = std::make_unique<ModuleRegistry>();
    int calls = 0;
    require(registry
                ->add("id", 1,
                      [&calls](ProcessContext &, std::span<const Term> args) {
                          ++calls;
                          return CallResult<Term>(args.front());
                      })
                .has_value(),
            "id fixture failed");
    require(runtime->code_server()->load({"calls", CodeImage::linked(), std::move(registry)}).has_value(),
            "load failed");
    const auto target = runtime->code_server()->resolve({.module = "calls", .function = "id", .arity = 1}).value();
    require(target.call(*context, {}).error().code == CallError::bad_arity, "arity unchecked");
    const std::array invalid{Term{}};
    auto bad = target.call(*context, invalid);
    require(!bad && bad.error().argument == 0 && bad.error().term_error == TermError::invalid_encoding,
            "bad argument lost");
    require(calls == 0, "invalid arguments entered target");
    const std::array valid{integer(-42)};
    require(target.call(*context, valid)->integer_value() == -42 && calls == 1, "generic identity failed");
}
} // namespace

// Validate the host registry boundary without implementing compiler BIF lowering.
int main() {
    try {
        check_terms();
        check_registry();
        check_publication();
        check_pinning();
        check_calls();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
