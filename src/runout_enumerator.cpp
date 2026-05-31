#include "poker/runout_enumerator.hpp"

#include "poker/combo_enumerator.hpp"

namespace poker {

RunoutEnumerator make_runout_enumerator_from_dead_mask(std::uint64_t dead_mask, int board_size) {
    RunoutEnumerator e;
    e.deck_remaining.reserve(52);
    for (int i = 0; i < 52; ++i) {
        if ((dead_mask & (std::uint64_t{1} << i)) == 0) {
            e.deck_remaining.push_back(i);
        }
    }
    e.need_cards = 5 - board_size;
    if (e.need_cards < 0 || e.need_cards > 2) {
        e.need_cards = 0;
        e.deck_remaining.clear();
    }
    return e;
}

}  // namespace poker
