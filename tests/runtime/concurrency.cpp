#include <algorithm>
#include <array>
#include <atomic>
#include <clause/runtime/code_server.hpp>
#include <clause/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Runtime services shared by scheduler workers (docs/runtime.md#threads), stressed from several host threads: the
// atom table interns overlapping spellings concurrently and keeps one stable word per spelling; the code server
// publishes, finds and calls modules concurrently, and published modules outlive the runtime through their pins.
namespace {
using namespace clause::runtime;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Threads racing on every spelling; enough to interleave inserts and lookups, few enough for a Debug run.
constexpr std::size_t THREADS = 8;
constexpr std::size_t SPELLINGS = 2'000;

// Run `work(index)` on THREADS threads at once and join them; false when any thread threw.
template <typename Work> bool parallel(Work work) {
    std::atomic<bool> failed{false};
    std::atomic<std::size_t> ready{0};
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < THREADS; ++index) {
        threads.emplace_back([&, index] {
            // Start together so the threads contend from the first operation.
            ++ready;
            while (ready < THREADS) {
                std::this_thread::yield();
            }
            try {
                work(index);
            } catch (...) {
                failed = true;
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    return !failed;
}

// Every thread interns all spellings, each starting at a different offset, and reads them back by word: each
// spelling gets one word, whichever thread created it, and the table holds each spelling once.
void atoms(Runtime &runtime) {
    auto &storage = *runtime.atom_storage();
    const auto before = storage.size();
    std::vector<std::vector<Word>> words(THREADS, std::vector<Word>(SPELLINGS));
    const bool joined = parallel([&](std::size_t thread) {
        for (std::size_t step = 0; step < SPELLINGS; ++step) {
            const auto index = (step + thread * SPELLINGS / THREADS) % SPELLINGS;
            const auto atom = storage.intern("atom_" + std::to_string(index)).value();
            require(storage.lookup(atom.word()).value().atom_spelling().value() == atom.atom_spelling().value(),
                    "lookup by word found another spelling");
            words[thread][index] = atom.word();
        }
    });
    require(joined, "a thread failed to intern or look up an atom");
    require(std::ranges::all_of(words, [&](const auto &seen) { return seen == words[0]; }),
            "threads got different words for one spelling");
    require(storage.size() == before + SPELLINGS, "a spelling was stored twice");
}

// Modules every thread tries to publish; each name is published once, the other attempts are duplicates.
constexpr std::size_t MODULES = 200;

// The name of module `index`.
std::string module_name(std::size_t index) { return "module_" + std::to_string(index); }

// A module exporting id/1, which returns its argument.
ModuleDefinition identity_module(std::size_t index) {
    auto functions = std::make_unique<ModuleRegistry>();
    const auto added =
        functions->add("id", 1, [](ProcessContext &, std::span<const Term> arguments) -> CallResult<Term> {
            return arguments.front();
        });
    require(added.has_value(), "cannot build a module");
    return {module_name(index), CodeImage::linked(), std::move(functions)};
}

// Publish module `index` from `thread`, counting a success; any failure other than a duplicate is an error.
void publish(CodeServer &server, std::size_t index, std::atomic<std::size_t> &published) {
    const auto loaded = server.load(identity_module(index));
    require(loaded || loaded.error() == CodeError::duplicate_module, "publication failed");
    if (loaded) {
        ++published;
    }
}

// Find module `index` and call its id/1 in `context`; a module not published yet is not found. Keeps the module.
void call(CodeServer &server, ProcessContext &context, std::size_t index,
          std::vector<std::shared_ptr<const LoadedModule>> &pins) {
    const auto function = server.resolve({.module = module_name(index), .function = "id", .arity = 1});
    if (!function) {
        require(function.error() == CodeError::module_not_found, "lookup failed");
        return;
    }
    const auto argument = context.atom_storage().intern("ping").value();
    require(function->call(context, std::array{argument}).value().word() == argument.word(), "a call failed");
    pins.push_back(server.find_module(module_name(index)).value());
}

// Threads publish every module, each in its own order, while they look modules up, call them and build one external
// fun definition; then the runtime is destroyed and the threads read and release the modules they pinned.
void modules(std::unique_ptr<Runtime> runtime) {
    auto &server = *runtime->code_server();
    std::vector<ProcessContext *> contexts;
    for (std::size_t thread = 0; thread < THREADS; ++thread) {
        contexts.push_back(runtime->create_context().value());
    }
    const auto module = runtime->atom_storage()->intern(module_name(0)).value();
    const auto function = runtime->atom_storage()->intern("id").value();
    std::vector<std::atomic<std::size_t>> published(MODULES);
    std::vector<std::vector<std::shared_ptr<const LoadedModule>>> pins(THREADS);
    std::vector<const FunDefinition *> funs(THREADS);
    require(parallel([&](std::size_t thread) {
                for (std::size_t step = 0; step < MODULES; ++step) {
                    const auto index = (step + thread * MODULES / THREADS) % MODULES;
                    publish(server, index, published[index]);
                    call(server, *contexts[thread], (index + MODULES / 2) % MODULES, pins[thread]);
                }
                funs[thread] = &server.external_fun(module, function, 1);
                require(server.owns(*funs[thread]) && funs[thread]->frame == nullptr, "external fun not owned");
            }),
            "a thread failed to publish, find or call a module");
    require(std::ranges::all_of(published, [](const auto &count) { return count == 1; }),
            "a module was published twice or never");
    require(std::ranges::all_of(funs, [&](const auto *fun) { return fun == funs[0]; }),
            "threads built different definitions of one external fun");
    for (auto *context : contexts) {
        require(runtime->destroy_context(context) == clause::abi::v1::Status::ok, "context teardown failed");
    }
    runtime.reset();
    require(parallel([&](std::size_t thread) {
                for (auto &pin : pins[thread]) {
                    require(pin->name().starts_with("module_") && pin->functions().find("id", 1).has_value(),
                            "a pinned module did not outlive its runtime");
                }
                pins[thread].clear();
            }),
            "a thread lost a pinned module");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        atoms(*runtime);
        modules(Runtime::start().value());
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
