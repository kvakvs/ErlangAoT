#include "heap_storage.hpp"
#include "heap_walk.hpp"
#include <algorithm>
#include <erlang_aot/runtime/atoms.hpp>
#include <new>
#include <vector>

namespace erlang_aot::runtime::detail {
namespace {
using Shape = HeapCell::Shape;

struct Start {
    // Address of an object's first word and the shape a pointer to it must have.
    std::uintptr_t address;
    Shape shape;
};

// Accept only the immediates a heap may hold today; identities are not admitted yet.
bool plain_immediate(Word value) {
    const auto kind = classify_immediate(value);
    return kind && (*kind == TermKind::smallint || *kind == TermKind::empty_tuple || *kind == TermKind::empty_list);
}

// Check one heap: parse every area, then resolve every term slot and off-heap link against the parsed starts.
class Verifier final {
  public:
    // Borrow a live storage for the duration of one verification.
    explicit Verifier(const HeapStorage &storage) : storage_(storage) {}

    // Run both passes; any parse or pointer fault is corrupt_heap.
    std::expected<HeapCensus, HeapError> run() {
        if (!collect() || !std::ranges::all_of(slots_, [&](auto slots) { return slots_valid(slots); }) ||
            !off_heap_valid()) {
            return std::unexpected(HeapError::corrupt_heap);
        }
        return census_;
    }

  private:
    // Pass 1: parse the heap and every fragment, counting objects and remembering starts and traced slots.
    bool collect() {
        if (!parse(storage_.heap_) ||
            !std::ranges::all_of(storage_.fragments_, [&](auto &area) { return parse(area); })) {
            return false;
        }
        std::ranges::sort(starts_, {}, &Start::address);
        return true;
    }

    // Walk one area's used words.
    bool parse(const HeapArea &area) {
        return walk(area.used(), [&](const HeapCell &cell) { record(cell); }).has_value();
    }

    // Count one parsed object and keep what pass 2 needs.
    void record(const HeapCell &cell) {
        census_.words += cell.words.size();
        if (cell.shape == Shape::filler) {
            census_.filler_words += cell.words.size();
            return;
        }
        ++(cell.shape == Shape::cons ? census_.cons_cells : census_.boxed_objects);
        refc_cells_ += static_cast<std::size_t>(cell.shape == Shape::boxed &&
                                                layout::BoxHeader::kind(cell.words[0]) == BoxedKind::refc_binary);
        starts_.push_back({reinterpret_cast<std::uintptr_t>(cell.words.data()), cell.shape});
        if (!cell.slots.empty()) {
            slots_.push_back(cell.slots);
        }
    }

    // Find the shape of the object starting exactly at a term's address; filler means none.
    Shape shape_at(Word value) const {
        const auto address = static_cast<std::uintptr_t>(value & ~static_cast<Word>(abi::v1::primary_mask));
        const auto found = std::ranges::lower_bound(starts_, address, {}, &Start::address);
        return found != starts_.end() && found->address == address ? found->shape : Shape::filler;
    }

    // Pass 2: a slot holds an admitted immediate, a runtime atom or a pointer to an object start.
    bool slot_valid(Word value) const {
        switch (TermTag{value}.get_kind()) {
        case TermKind::list:
            return shape_at(value) == Shape::cons;
        case TermKind::boxed:
            return shape_at(value) == Shape::boxed;
        case TermKind::atom:
            return storage_.atoms_->lookup(value).has_value();
        default:
            return plain_immediate(value);
        }
    }

    // Check every slot of one object.
    bool slots_valid(std::span<const Word> slots) const {
        return std::ranges::all_of(slots, [&](Word value) { return slot_valid(value); });
    }

    // Every listed cell is a parsed off-heap binary, and every parsed one is listed.
    bool off_heap_valid() {
        for (const auto *cell = storage_.off_heap_; cell != nullptr; cell = cell->next_) {
            const auto value = reinterpret_cast<Word>(cell) | static_cast<Word>(TermKindPrimary::boxed);
            if (shape_at(value) != Shape::boxed ||
                layout::BoxHeader::kind(cell->header_.value_) != BoxedKind::refc_binary) {
                return false;
            }
            ++census_.off_heap_cells;
        }
        return census_.off_heap_cells == refc_cells_;
    }

    // Borrowed storage, the sorted object starts, traced slot spans and running counts.
    const HeapStorage &storage_;
    std::vector<Start> starts_;
    std::vector<std::span<const Word>> slots_;
    std::size_t refc_cells_ = 0;
    HeapCensus census_;
};
} // namespace
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
std::expected<HeapCensus, HeapError> ProcessHeap::verify() const noexcept {
    if (!storage_->alive()) {
        return std::unexpected(HeapError::expired_context);
    }
    try {
        return detail::Verifier(*storage_).run();
    } catch (const std::bad_alloc &) {
        return std::unexpected(HeapError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(HeapError::out_of_memory);
    }
}
} // namespace erlang_aot::runtime
