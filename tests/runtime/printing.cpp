#include "../compiler/codegen/match_wire.hpp"
#include <clause/abi/output.hpp>
#include <clause/runtime/output.hpp>
#include <clause/runtime/runtime.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace clause::runtime;
using clause::abi::v1::Status;

// Retain behavioral checks in optimized builds.
void require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Read a golden file as LF-separated byte lines, tolerating CRLF checkouts.
std::vector<std::string> lines(const char *path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good(), std::string("cannot read ") + path);
    std::vector<std::string> result;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        result.push_back(std::move(line));
    }
    return result;
}

// Same escaping as tests/compiler/printing/values.py: printable ASCII stays, other bytes become \xHH.
std::string escape(std::string_view text) {
    static constexpr std::string_view digits = "0123456789abcdef";
    std::string result;
    for (const auto c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte == '\\') {
            result += "\\\\";
        } else if (byte >= 0x20 && byte < 0x7F) {
            result += c;
        } else {
            result += {'\\', 'x', digits[byte >> 4U], digits[byte & 15U]};
        }
    }
    return result;
}

// Compare one rendering, describing the first difference for a readable failure.
bool same(std::size_t row, const char *style, const std::string &expected, const TermResult<std::string> &actual) {
    if (actual && *actual == expected) {
        return true;
    }
    std::cerr << "value " << row + 1 << " (" << style << "): expected " << expected << "\n  actual   "
              << (actual ? *actual : "<error " + std::to_string(static_cast<int>(actual.error())) + ">") << '\n';
    return false;
}

struct Golden {
    // Line-aligned wire inputs, OTP ~w text and escaped OTP display text.
    std::vector<std::string> values;
    std::vector<std::string> write;
    std::vector<std::string> display;
};

// OTP's display follows its internal map layout for these rows, so only ~w is compared.
constexpr std::string_view unordered = "?unordered";

// Count the mismatching renderings of one golden row.
std::size_t compare(ProcessContext &context, const Golden &golden, std::size_t row) {
    std::string_view input(golden.values[row]);
    const auto term = wire::read(context, input);
    std::size_t failures = same(row, "~w", golden.write[row], format_term(term, TermStyle::write)) ? 0 : 1;
    if (golden.display[row] != unordered) {
        const auto shown =
            format_term(term, TermStyle::display).transform([](const std::string &text) { return escape(text); });
        failures += same(row, "display", golden.display[row], shown) ? 0 : 1;
    }
    return failures;
}

// Every corpus/authored value prints exactly as OTP ~w and erlang:display/1 recorded it.
void golden(const char *values_path, const char *write_path, const char *display_path) {
    const Golden golden{lines(values_path), lines(write_path), lines(display_path)};
    const auto count = golden.values.size();
    require(count == golden.write.size() && count == golden.display.size(), "golden files differ in length");
    auto runtime = Runtime::start().value();
    std::size_t failures = 0;
    for (std::size_t start = 0; start < count; start += 500) {
        auto *context = runtime->create_context().value();
        for (auto row = start; row < std::min(count, start + 500); ++row) {
            failures += compare(*context, golden, row);
        }
        require(runtime->destroy_context(context) == Status::ok, "context teardown failed");
    }
    require(failures == 0, std::to_string(failures) + " printing mismatches");
    std::cout << count << " values match OTP ~w and erlang:display/1\n";
}

// Nesting deeper than any native stack prints iteratively in both styles.
void deep(ProcessContext &context) {
    constexpr std::size_t depth = 100'000;
    TermFactory factory(context);
    auto list = factory.atom("leaf").value();
    auto tuple = list;
    for (std::size_t i = 0; i < depth; ++i) {
        list = factory.cons(list, factory.nil().value()).value();
        tuple = factory.tuple(std::array{tuple}).value();
    }
    const auto expected_list = std::string(depth, '[') + "leaf" + std::string(depth, ']');
    const auto expected_tuple = std::string(depth, '{') + "leaf" + std::string(depth, '}');
    for (const auto style : {TermStyle::write, TermStyle::display}) {
        require(format_term(list, style).value() == expected_list, "deep list text");
        require(format_term(tuple, style).value() == expected_tuple, "deep tuple text");
    }
}

// A million-element list prints in linear work; the exact byte budget is honored at its boundary.
void wide(ProcessContext &context) {
    std::vector<Term> elements(1'000'000, Term::from_word(encode_integer(7).value()).value());
    const auto list = TermFactory(context).list(elements).value();
    const auto text = format_term(list, TermStyle::write).value();
    require(text.size() == 2'000'001 && text.starts_with("[7,7,") && text.ends_with(",7]"), "wide list text");
    require(format_term(list, TermStyle::write, text.size()).value() == text, "exact budget rejected");
    require(format_term(list, TermStyle::write, text.size() - 1) == std::unexpected(TermError::resource_limit),
            "budget overrun accepted");
}

// Shared subterms would expand exponentially; the byte budget stops the work instead.
void shared(ProcessContext &context) {
    TermFactory factory(context);
    auto value = factory.atom("x").value();
    for (int i = 0; i < 60; ++i) {
        value = factory.tuple(std::array{value, value}).value();
    }
    for (const auto style : {TermStyle::write, TermStyle::display}) {
        require(format_term(value, style, 1 << 20) == std::unexpected(TermError::resource_limit), "shared overrun");
    }
}

struct Captured {
    // Bytes delivered by the runtime; `accept` false simulates a failed write.
    std::string bytes;
    bool accept = true;
};

// Record standard output so the display line and its single newline are observable.
bool capture(void *state, std::string_view bytes) {
    auto &captured = *static_cast<Captured *>(state);
    captured.bytes += bytes;
    return captured.accept;
}

// Call the generated-code display service as compiled code does, inside one host invocation.
std::uint8_t display(ProcessContext &context, const Term &value, clause::abi::v1::TermWord *output) {
    return CLAUSE_display_v1(&context, value.word(), output);
}

// The service writes exactly one line and returns true; a rejected write becomes output_failure.
void service() {
    Captured captured;
    RuntimeOptions options;
    options.standard_output = {&captured, capture};
    auto runtime = Runtime::start(options).value();
    auto &context = *runtime->create_context().value();
    // Module registration interns the canonical true/false atoms before generated code runs.
    require(context.atom_storage().intern("true").has_value(), "true atom");
    const auto value = TermFactory(context).tuple(std::array{context.atom_storage().intern("a").value()}).value();
    clause::abi::v1::TermWord result = 0;
    {
        GeneratedInvocation scope(context.generated_calls());
        require(display(context, value, &result) == 0 && captured.bytes == "{a}\n", "display line");
        require(result == context.atom_storage().boolean(true).value().word(), "display result is not true");
    }
    captured.accept = false;
    GeneratedInvocation scope(context.generated_calls());
    require(display(context, value, &result) != 0, "failed write reported success");
    const auto &failure = context.generated_calls().failure();
    require(failure && failure->status == Status::output_failure, "write failure status lost");
}

// Missing outputs and inactive invocations fail without writing anything.
void misuse() {
    Captured captured;
    RuntimeOptions options;
    options.standard_output = {&captured, capture};
    auto runtime = Runtime::start(options).value();
    auto &context = *runtime->create_context().value();
    const auto value = context.atom_storage().intern("a").value();
    clause::abi::v1::TermWord result = 0;
    require(display(context, value, &result) != 0 && captured.bytes.empty(), "display ran outside an invocation");
    GeneratedInvocation scope(context.generated_calls());
    require(display(context, value, nullptr) != 0 && captured.bytes.empty(), "display accepted a null output");
    require(context.generated_calls().failure()->status == Status::invalid_argument, "null output status");
}
} // namespace

// Arguments: values.txt, write.txt and display.txt from tests/fixtures/printing.
int main(int argc, char **argv) {
    try {
        require(argc == 4, "usage: runtime_printing values.txt write.txt display.txt");
        golden(argv[1], argv[2], argv[3]);
        auto runtime = Runtime::start().value();
        auto &context = *runtime->create_context().value();
        deep(context);
        wide(context);
        shared(context);
        service();
        misuse();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
