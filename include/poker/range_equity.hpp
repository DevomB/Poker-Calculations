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

/// `extra_dead_mask` bits (deck ids) are excluded from the runout as well as from the range.
[[nodiscard]] double exact_hu_equity_vs_range(const std::vector<Card>& hero_hole_cards,
                                              const std::vector<Card>& board_cards,
                                              const SparseRange& villain_range,
                                              const CancelPredicate* should_cancel = nullptr,
                                              std::uint64_t extra_dead_mask = 0);

[[nodiscard]] double equity_delta_if_card_removed(const std::vector<Card>& hero_hole_cards,
                                                  const std::vector<Card>& board_cards,
                                                  int removed_deck_index,
                                                  const SparseRange& villain_range,
                                                  const CancelPredicate* should_cancel = nullptr);

}  // namespace poker
