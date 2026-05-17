#pragma once

#include "poker/card.hpp"

#include <vector>

namespace poker {

/**
 * P22: exact HU equity vs a uniformly random villain hand (two cards), board length in [3, 5].
 * Enumerates all villain combos and all completions of the board to five cards.
 */
[[nodiscard]] double exact_hu_equity_vs_random_hand(const std::vector<Card>& hero_hole_cards,
                                                    const std::vector<Card>& board_cards);

}  // namespace poker
