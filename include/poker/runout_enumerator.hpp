#pragma once

#include "poker/cancel.hpp"
#include "poker/combo_enumerator.hpp"

#include <cstdint>
#include <vector>

namespace poker {

struct RunoutEnumerator {
    std::vector<int> deck_remaining;
    int need_cards{0};
};

[[nodiscard]] RunoutEnumerator make_runout_enumerator_from_dead_mask(std::uint64_t dead_mask,
                                                                     int board_size);

template <typename Fn>
void for_each_uniform_runout(const RunoutEnumerator& e, Fn&& fn, const CancelPredicate* cancel) {
    const int k = e.need_cards;
    if (k == 0) {
        fn(nullptr, 0);
        return;
    }
    for_each_combo_indices(e.deck_remaining, k, [&](const int* combo, int combo_k) {
        throw_if_cancelled(cancel);
        fn(combo, combo_k);
    });
}

}  // namespace poker
