#include "terms.hpp"
#include "terms/structural_order.hpp"
#include <array>
#include <erlang_aot/abi/funs.hpp>
#include <erlang_aot/abi/modules.hpp>
#include <erlang_aot/runtime/modules.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Fun cells, erlang_aot_make_fun_v1 and erlang_aot_apply_v1 over hand-written descriptors (docs/funs.md).
namespace {
using namespace erlang_aot;
using namespace erlang_aot::runtime;
using abi::v1::FrameDescriptor;
using abi::v1::FunDescriptor;

// Keep assertions active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Atom slots: 0 module, 1 the anonymous fun, 2 a local function, 3 and 4 an external module and function.
constexpr std::array<abi::v1::AtomDescriptor, 5> ATOMS{
    {{"fm", 2}, {"-f/0-fun-0-", 11}, {"g", 1}, {"other", 5}, {"h", 1}}};

extern const abi::v1::ModuleDescriptor fm;
// Code is never entered here: frames only name the functions and their arities (one argument, two captured values).
const FrameDescriptor LAMBDA{&fm, 0, 1, 3, nullptr, 3, 3};
const FrameDescriptor LOCAL{&fm, 0, 2, 1, nullptr, 1, 1};
const std::array<FunDescriptor, 3> FUNS{
    {{&fm, 0, 1, 1, 0, 0, &LAMBDA}, {&fm, 0, 2, 1, 1, 0, &LOCAL}, {&fm, 3, 4, 2, 0, 1, nullptr}}};
const abi::v1::ModuleDescriptor fm{abi::v1::version, sizeof(Word) * 8, "fm",    2, nullptr,     0,
                                   ATOMS.data(),     ATOMS.size(),     nullptr, 0, FUNS.data(), FUNS.size()};

// Build a fun of a descriptor inside a generated invocation; the status of a refused build.
TermResult<Term> make(ProcessContext &context, const FunDescriptor &descriptor, std::span<const Term> captures) {
    std::vector<Word> words;
    for (const auto &value : captures) {
        words.push_back(value.word());
    }
    Word output = 0;
    GeneratedInvocation scope(context.generated_calls());
    const auto status = static_cast<abi::v1::Status>(
        erlang_aot_make_fun_v1(&context, &descriptor, words.data(), words.size(), &output));
    if (status != abi::v1::Status::ok) {
        return std::unexpected(status == abi::v1::Status::wrong_owner ? TermError::wrong_owner
                                                                      : TermError::invalid_argument);
    }
    return Term::from_word(output, context);
}

// The result of preparing a call: the frame to enter, the registers after it and the error it raised, as text.
struct Applied {
    const void *frame;
    std::vector<Word> registers;
    std::string error;
};

// Prepare calling `fun` with `arguments` in a register array.
Applied apply(ProcessContext &context, const Term &fun, std::span<const Term> arguments) {
    std::vector<Word> registers(abi::v1::register_count, 0);
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        registers[i] = arguments[i].word();
    }
    GeneratedInvocation scope(context.generated_calls());
    const auto *frame = erlang_aot_apply_v1(&context, fun.word(), arguments.size(), registers.data());
    const auto &failure = context.generated_calls().failure();
    std::string error;
    if (failure && failure->reason) {
        const auto name = std::to_string(static_cast<int>(*failure->reason));
        error = failure->value ? name + ":" + format_term(*failure->value, TermStyle::write).value() : name;
    }
    return {frame, std::move(registers), std::move(error)};
}

// Render a value as erlang:display text.
std::string text(const Term &value) { return format_term(value, TermStyle::display).value(); }

// Funs keep their definition and captured values, print like OTP's and pass is_function.
void construction(ProcessContext &context) {
    TermFactory factory(context);
    const auto one = factory.integer(1).value();
    const auto pair = factory.tuple(std::array{one, factory.atom("x").value()}).value();
    const auto closure = make(context, FUNS[0], std::array{pair, one}).value();
    const auto local = make(context, FUNS[1], {}).value();
    const auto external = make(context, FUNS[2], {}).value();
    require(closure.is_function() && closure.is_function(1) && !closure.is_function(3) && external.is_function(2) &&
                !pair.is_function(),
            "is_function differs");
    require(text(closure) == "#Fun<fm.0.0>" && text(local) == "#Fun<fm.1.0>" && text(external) == "fun other:h/2" &&
                format_term(external, TermStyle::write).value() == "fun other:h/2",
            "fun text differs");
    require(make(context, FUNS[0], std::array{one}).error() == TermError::invalid_argument,
            "captured value count mismatch accepted");
    const FunDescriptor forged = FUNS[1];
    require(make(context, forged, {}).error() == TermError::wrong_owner, "unregistered descriptor accepted");
}

// Funs order after atoms and before tuples; local before external; same definitions by captured values.
void order(ProcessContext &context) {
    TermFactory factory(context);
    const auto one = factory.integer(1).value();
    const auto real = factory.floating(1.0).value();
    const auto two = factory.integer(2).value();
    const auto compare = [](const Term &left, const Term &right, bool exact) {
        return detail::structural_order(left, right, exact).value();
    };
    const auto a = make(context, FUNS[0], std::array{one, two}).value();
    const auto b = make(context, FUNS[0], std::array{one, two}).value();
    const auto c = make(context, FUNS[0], std::array{two, one}).value();
    const auto d = make(context, FUNS[0], std::array{real, two}).value();
    const auto local = make(context, FUNS[1], {}).value();
    const auto external = make(context, FUNS[2], {}).value();
    require(compare(a, b, true) == 0 && compare(a, c, true) < 0 && compare(a, d, true) != 0 &&
                compare(a, d, false) == 0,
            "captured value order differs");
    require(compare(a, local, true) < 0 && compare(local, external, true) < 0, "fun order differs");
    require(compare(factory.atom("zzz").value(), a, true) < 0 && compare(external, factory.tuple({}).value(), true) < 0,
            "fun category order differs");
}

// A call checks the value and its arity, then appends the captured values after the arguments.
void calls(ProcessContext &context) {
    TermFactory factory(context);
    const auto one = factory.integer(1).value();
    const auto x = factory.atom("x").value();
    const auto closure = make(context, FUNS[0], std::array{one, x}).value();
    const auto ready = apply(context, closure, std::array{x});
    require(ready.frame == &LAMBDA && ready.error.empty() && ready.registers[0] == x.word() &&
                ready.registers[1] == one.word() && ready.registers[2] == x.word(),
            "captured values were not appended");
    const auto arity = apply(context, closure, std::array{x, one});
    require(!arity.frame && arity.error == "23:{#Fun<fm.0.0>,[x,1]}", "badarity differs");
    const auto value = apply(context, one, {});
    require(!value.frame && value.error == "22:1", "badfun differs");
    const auto external = make(context, FUNS[2], {}).value();
    const auto missing = apply(context, external, std::array{x, x});
    require(!missing.frame && missing.error == "24", "undef differs");
}

// Captured values survive collections and copies between heaps.
void memory(Runtime &runtime) {
    auto &source = *runtime.create_context().value();
    auto &destination = *runtime.create_context().value();
    TermFactory factory(source);
    const auto inner = factory.tuple(std::array{factory.integer(7).value()}).value();
    const auto list = factory.list(std::array{inner, inner}).value();
    const auto closure = make(source, FUNS[0], std::array{list, inner}).value();
    require(source.heap().verify().has_value(), "fun heap does not verify");
    const auto copy = closure.copy_to(destination.heap()).value();
    require(copy.exactly_equal(closure).value() && destination.heap().verify().has_value(), "copied fun differs");
    std::array roots{closure.word()};
    require(source.heap().collect(roots).has_value() && source.heap().verify().has_value(), "collection failed");
    const auto moved = Term::from_word(roots[0], source).value();
    require(moved.exactly_equal(copy).value(), "fun lost its captured values in a collection");
    require(runtime.destroy_context(&source) == abi::v1::Status::ok && copy.is_function(1),
            "copy depended on its source");
}

// Registration binds only funs that name their own module, and local funs need code.
void registration(Runtime &runtime) {
    auto bad = fm;
    bad.name = "bad";
    bad.name_size = 3;
    const FunDescriptor stray{&fm, 0, 2, 1, 0, 0, &LOCAL};
    bad.funs = &stray;
    bad.fun_count = 1;
    require(register_module(runtime, bad).error() == CodeError::invalid_module, "foreign fun descriptor accepted");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        require(register_module(*runtime, fm).has_value(), "fun module rejected");
        registration(*runtime);
        auto &context = *runtime->create_context().value();
        construction(context);
        order(context);
        calls(context);
        memory(*runtime);
        std::cout << "funs passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
