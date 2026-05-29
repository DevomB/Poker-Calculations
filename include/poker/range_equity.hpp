#pragma once

#include "poker/cancel.hpp"
#include "poker/card.hpp"
#include "poker/range.hpp"

#include <vector>

namespace poker {

[[nodiscard]] double exact_hu_equity_vs_known_hand(const std::vector<Card>& hero_hole_cards,
                                                 const std::vector<Card>& villain_hole_cards,
                                                 const std::vector<Card>& board_cards,
                                                 const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double exact_hu_equity_vs_range(const std::vector<Card>& hero_hole_cards,
                                              const std::vector<Card>& board_cards,
                                              const SparseRange& villain_range,
                                              const CancelPredicate* should_cancel = nullptr);

[[nodiscard]] double equity_delta_if_card_removed(const std::vector<Card>& hero_hole_cards,
                                                  const std::vector<Card>& board_cards,
                                                  int removed_deck_index,
                                                  const SparseRange& villain_range,
                                                  const CancelPredicate* should_cancel = nullptr);

}  // namespace poker
