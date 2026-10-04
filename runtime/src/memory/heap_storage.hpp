#pragma once
#include "heap_object.hpp"
#include "process_heap.hpp"
#include <cstdint>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
namespace layout {
struct RefcBinaryCell;
} // namespace layout

struct HeapArea {
    // One block that never moves: the heap or a fragment, bump-allocated in [0, top_) of capacity_ words.
    std::unique_ptr<Word[]> words_;
    std::size_t capacity_ = 0;
    std::size_t top_ = 0;

    // Borrow the allocated words as a walkable area.
    std::span<const Word> used() const noexcept;
    // Borrow allocated words from a word-aligned address below top; empty for any other address.
    std::span<Word> from(std::uintptr_t address) noexcept;
    // Report whether words more fit above top.
    bool fits(std::size_t words) const noexcept;
    // Advance top by words that fit and zero them, so unused reserved words parse as filler.
    std::span<Word> bump(std::size_t words) noexcept;
};

struct FragmentRange {
    // First word address of a fragment and its index in HeapStorage::fragments_, for admission lookups.
    std::uintptr_t begin_;
    std::size_t fragment_;
};

// The storage of exactly one process: its single heap block, its fragment chain and its off-heap list.
class HeapStorage final {
  public:
    // Bind validated budgets and a liveness token; the heap block is created by the first reservation.
    HeapStorage(HeapOptions options, std::weak_ptr<const ContextLifetime> lifetime, AtomStorage &atoms);
    // Release every off-heap reference while all backing bytes still exist.
    ~HeapStorage();
    HeapStorage(const HeapStorage &) = delete;
    HeapStorage &operator=(const HeapStorage &) = delete;
    // Check lifetime before admitting a surviving reservation or future heap Term.
    bool alive() const noexcept;
    // Capture exact accounting before an unpublished reservation changes a top.
    HeapMark mark() const noexcept;
    // Restore a reservation without touching any previously published words.
    void rollback(HeapMark mark) noexcept;
    // Bump-allocate in the heap, else the newest fragment, else a new fragment; nothing ever moves.
    std::expected<std::span<Word>, HeapError> reserve(std::size_t words);
    // Count a newly created off-heap buffer against the budget shared with heap backing.
    std::expected<void, HeapError> charge(std::size_t words) noexcept;
    // Borrow the used words from an address to the end of its area; empty unless the address is
    // word-aligned below the top of the heap or a fragment. Process pointers only name object starts.
    // The words are writable for the collector, which forwards objects in place.
    std::span<Word> owned(std::uintptr_t address) noexcept;

    // Retain bounded backing independently of future host pins.
    HeapOptions options_;
    std::weak_ptr<const ContextLifetime> lifetime_;
    // The process heap block; empty until the first reservation.
    HeapArea heap_;
    // Overflow blocks allocated while the heap may not move, oldest first; a collection merges them.
    std::vector<HeapArea> fragments_;
    // Fragment ranges sorted by address; rollback drops entries of removed fragments.
    std::vector<FragmentRange> ranges_;
    // Words below the tops of all areas, and words of all areas' capacity.
    std::size_t used_words_ = 0;
    std::size_t capacity_words_ = 0;
    // Words of off-heap buffers created by this process; capacity_words_ + off_heap_words_ stays within budget.
    std::size_t off_heap_words_ = 0;
    // Head of this process's off-heap binary cells, newest first; the only route to their C++ state.
    layout::RefcBinaryCell *off_heap_ = nullptr;
    // Set while a reservation is open; one reservation at a time.
    bool pending_ = false;
    // Completed collections; host Terms admitted before the latest one are stale.
    std::size_t collections_ = 0;
    // Borrow the runtime atom table only while the process lifetime token remains alive.
    AtomStorage *atoms_;

  private:
    // Choose the area for a validated request, creating the heap block or a fragment when needed.
    std::expected<HeapArea *, HeapError> area_for(std::size_t words);
    // Allocate a block of at least words and min_heap_words, capped by the remaining budget.
    std::expected<HeapArea, HeapError> block(std::size_t words) const;
    // Chain a new fragment that fits words and index its address range.
    std::expected<HeapArea *, HeapError> add_fragment(std::size_t words);
};

// Commit fully initialized objects and admit value; failure rolls the reservation back.
TermResult<Term> publish(const std::shared_ptr<HeapStorage> &storage, HeapReservation &reservation,
                         Word value) noexcept;
} // namespace erlang_aot::runtime::detail
