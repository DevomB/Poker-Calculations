#include "poker/runout_enumerator.hpp"

#include "poker/combo_enumerator.hpp"

#include <stdexcept>
#include <string>

namespace poker {

RunoutEnumerator make_runout_enumerator_from_dead_mask(std::uint64_t dead_mask, int board_size) {
    RunoutEnumerator e;
    e.deck_remaining.reserve(52);
    for (int i = 0; i < 52; ++i) {
        if ((dead_mask & (std::uint64_t{1} << i)) == 0) {
            e.deck_remaining.push_back(i);
        }
    }
    // Turns and rivers only: a board of 3 to 5 cards needs 0 to 2 more. Anything else used to be
    // treated as complete, which made up results from an unfinished board.
    if (board_size < 3 || board_size > 5) {
        throw std::invalid_argument("runout enumeration needs a board of 3 to 5 cards (got " +
                                    std::to_string(board_size) + ")");
    }
    e.need_cards = 5 - board_size;
    return e;
}

}  // namespace poker
