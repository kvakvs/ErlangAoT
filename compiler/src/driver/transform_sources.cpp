// Added for parse transforms: a project transform module's sources and those of the project modules it calls.
#include "transform_sources.hpp"
#include "../semantic/calls.hpp"
#include <algorithm>
#include <clause/compiler/parser.hpp>
#include <set>

namespace clause::cli {
namespace {
// Module.erl among the inputs, then in the directories; empty when there is none.
std::filesystem::path locate(const std::u32string &module, const SourcePlaces &places) {
    const auto bytes = utf8(module) + ".erl";
    const std::filesystem::path file(std::u8string(bytes.begin(), bytes.end()));
    const auto input = std::ranges::find_if(places.inputs_, [&](const auto &path) { return path.filename() == file; });
    if (input != places.inputs_.end()) {
        return *input;
    }
    for (const auto &directory : places.directories_) {
        std::error_code error;
        if (std::filesystem::is_regular_file(directory / file, error)) {
            return directory / file;
        }
    }
    return {};
}

// The modules a source calls by literal name; none when it does not parse here (the host compiler decides).
std::vector<std::u32string> called_modules(const std::filesystem::path &path, const PreprocessorOptions &options) {
    try {
        SourceManager sources;
        PreprocessorSession session(sources.read(path), options);
        const auto parsed = parse_module(session);
        std::vector<std::u32string> result;
        for (const auto &[name, site] : semantic::referenced_modules(parsed.module)) {
            result.push_back(name);
        }
        return result;
    } catch (const std::exception &) {
        return {};
    }
}
} // namespace

std::vector<std::filesystem::path> transform_sources(const std::u32string &module, const SourcePlaces &places,
                                                     const PreprocessorOptions &options) {
    std::vector<std::filesystem::path> result;
    std::set<std::u32string> seen{module};
    std::vector<std::u32string> pending{module};
    while (!pending.empty()) {
        const auto name = pending.back();
        pending.pop_back();
        auto path = locate(name, places);
        if (path.empty()) {
            continue;
        }
        for (auto &called : called_modules(path, options)) {
            if (seen.insert(called).second) {
                pending.push_back(std::move(called));
            }
        }
        result.push_back(std::move(path));
    }
    return result;
}
} // namespace clause::cli
