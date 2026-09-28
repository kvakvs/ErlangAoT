#pragma once
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace test_records {
// Stream the oracle projection with explicit nesting, without retaining a second AST.
class RecordPrinter {
  public:
    // Child counts close each parent after its last projected child has been printed.
    template <typename... Fields> void node(std::string_view kind, std::size_t children, const Fields &...fields) {
        if (!remaining_.empty()) {
            --remaining_.back();
        }
        std::cout << std::string(remaining_.size() * 2, ' ') << '(' << kind;
        static_cast<void>((std::cout << ... << fields));
        if (children != 0) {
            remaining_.push_back(children);
            std::cout << '\n';
            return;
        }
        std::cout << ")\n";
        close_parents();
    }

    // Reject an incomplete projection instead of silently emitting unbalanced output.
    void finish() const {
        if (!remaining_.empty()) {
            throw std::logic_error("unfinished AST record");
        }
    }

  private:
    // The explicit stack holds the number of children still owed to each open parent.
    std::vector<std::size_t> remaining_;

    // A completed child can finish several enclosing parents at once.
    void close_parents() {
        while (!remaining_.empty() && remaining_.back() == 0) {
            remaining_.pop_back();
            std::cout << std::string(remaining_.size() * 2, ' ') << ")\n";
        }
    }
};

// All projection visitors contribute to the same ordered output stream.
inline RecordPrinter records;
} // namespace test_records
