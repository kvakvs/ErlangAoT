#pragma once
#include <erlang_aot/runtime/terms.hpp>
#include <utility>
#include <vector>

// Pid numbers (docs/terms.md#pids-and-references): one process-wide sequence that is never recycled, so the pid word
// of another runtime or a number never issued is rejected like a forged word.
namespace erlang_aot::runtime::detail {
class ProcessNumbers final {
  public:
    // Reserve the next number for a new process of this runtime; resource_limit once the pid payload is exhausted.
    TermResult<Word> issue();
    // Whether this runtime issued `number`; pids of exited processes stay valid identities.
    bool issued(Word number) const noexcept;

  private:
    // Issued numbers as ascending [first, end) runs: a single run while one runtime creates every process.
    std::vector<std::pair<Word, Word>> runs_;
};

// The immediate pid word of a process number, and the number of a pid word.
Word pid_word(Word number) noexcept;
Word pid_number(Word word) noexcept;
} // namespace erlang_aot::runtime::detail
