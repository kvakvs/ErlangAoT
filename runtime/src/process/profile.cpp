#include "profile.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

namespace clause::runtime::detail {
namespace {
// The spelling of a compiled atom slot of a function's module, read from its descriptor.
std::string_view atom(const abi::v1::ModuleDescriptor &module, const std::size_t slot) {
    return slot < module.atom_count && module.atoms[slot].spelling
               ? std::string_view(module.atoms[slot].spelling, module.atoms[slot].size)
               : std::string_view{};
}

// module:function/arity of a frame; frames without a module (bottom frames) are the runtime's.
std::string name(const abi::v1::FrameDescriptor *frame) {
    if (!frame || !frame->module) {
        return "(runtime)";
    }
    return std::string(atom(*frame->module, frame->module_atom)) + ":" +
           std::string(atom(*frame->module, frame->function_atom)) + "/" + std::to_string(frame->arity);
}

// One report row: right-aligned numbers, then the name.
std::string row(const std::uint64_t nanoseconds, const std::string &share, const std::uint64_t entries,
                const std::string &label) {
    std::array<char, 64> numbers{};
    static_cast<void>(std::snprintf(numbers.data(), numbers.size(), "%12llu %7s %12llu  ",
                                    static_cast<unsigned long long>(nanoseconds / 1000), share.c_str(),
                                    static_cast<unsigned long long>(entries)));
    return numbers.data() + label + "\n";
}

// A share of the total as a percentage with one decimal.
std::string percent(const std::uint64_t part, const std::uint64_t total) {
    std::array<char, 16> text{};
    const auto value = total == 0 ? 0.0 : 100.0 * static_cast<double>(part) / static_cast<double>(total);
    static_cast<void>(std::snprintf(text.data(), text.size(), "%.1f%%", value));
    return text.data();
}

// Functions by descending self time; ties by name, so equal costs print in a stable order.
std::vector<std::pair<const abi::v1::FrameDescriptor *, FunctionCost>> ranked(const FunctionCosts &costs) {
    std::vector<std::pair<const abi::v1::FrameDescriptor *, FunctionCost>> result(costs.begin(), costs.end());
    std::ranges::sort(result, [](const auto &left, const auto &right) {
        if (left.second.nanoseconds != right.second.nanoseconds) {
            return left.second.nanoseconds > right.second.nanoseconds;
        }
        return name(left.first) < name(right.first);
    });
    return result;
}

// The sum of a set of costs.
FunctionCost total(const FunctionCosts &costs) {
    FunctionCost result;
    for (const auto &[frame, cost] : costs) {
        result.entries += cost.entries;
        result.nanoseconds += cost.nanoseconds;
    }
    return result;
}
} // namespace

void ProcessProfile::enter(const abi::v1::FrameDescriptor &function) noexcept {
    try {
        ++functions_[&function].entries;
    } catch (...) {
        // A sample lost to memory exhaustion leaves the program running; the report counts it.
        ++lost_;
    }
    run(&function);
}

void ProcessProfile::run(const abi::v1::FrameDescriptor *function) noexcept {
    const auto now = Clock::now();
    if (running_) {
        try {
            functions_[running_].nanoseconds +=
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - since_).count());
        } catch (...) {
            ++lost_;
        }
    }
    running_ = function;
    since_ = now;
}

void RuntimeProfile::add(const Word pid, const ProcessProfile &profile) noexcept {
    try {
        const std::scoped_lock lock(mutex_);
        ProcessRecord record{pid, total(profile.functions()), nullptr};
        const auto order = ranked(profile.functions());
        record.top = order.empty() ? nullptr : order.front().first;
        for (const auto &[frame, cost] : profile.functions()) {
            auto &merged = functions_[frame];
            merged.entries += cost.entries;
            merged.nanoseconds += cost.nanoseconds;
        }
        processes_.push_back(record);
        lost_ += profile.lost();
    } catch (...) {
        // A process lost to memory exhaustion leaves the program running; the report counts its samples.
        lost_ += 1 + profile.lost();
    }
}

std::string RuntimeProfile::report() const {
    const std::scoped_lock lock(mutex_);
    const auto sum = total(functions_);
    std::string text = "Clause profile: " + std::to_string(processes_.size()) + " processes\n";
    if (lost_ != 0) {
        text += "Samples lost to memory exhaustion: " + std::to_string(lost_.load()) + "\n";
    }
    text += "Functions by self time:\n     time_us   share      entries  function\n";
    for (const auto &[frame, cost] : ranked(functions_)) {
        text += row(cost.nanoseconds, percent(cost.nanoseconds, sum.nanoseconds), cost.entries, name(frame));
    }
    auto processes = processes_;
    std::ranges::sort(processes, [](const auto &left, const auto &right) {
        return left.total.nanoseconds != right.total.nanoseconds ? left.total.nanoseconds > right.total.nanoseconds
                                                                 : left.pid < right.pid;
    });
    text += "Processes by time:\n     time_us   share      entries  process: top function\n";
    for (const auto &process : processes) {
        const auto label = "<0." + std::to_string(process.pid) + ".0>: " + name(process.top);
        text += row(process.total.nanoseconds, percent(process.total.nanoseconds, sum.nanoseconds),
                    process.total.entries, label);
    }
    return text;
}
} // namespace clause::runtime::detail
