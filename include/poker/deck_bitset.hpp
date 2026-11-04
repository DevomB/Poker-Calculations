#pragma once

#include "poker/bit_utils.hpp"
#include "poker/card.hpp"
#include "poker/card_string.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace poker {

/// Dead-card mask for deck indices 0..51 (`rank * 4 + suit`).
struct DeckBitset {
    std::uint64_t mask{0};

    void clear() { mask = 0; }

    void set(int deck_index) { mask |= (std::uint64_t{1} << deck_index); }

    [[nodiscard]] bool test(int deck_index) const {
        return (mask & (std::uint64_t{1} << deck_index)) != 0;
    }

    void mark_card(const Card& c) { set(deck_index_from_card(c)); }

    void mark_cards(const std::vector<Card>& cards) {
        for (const Card& c : cards) {
            mark_card(c);
        }
    }

    /// Marks `cards`; throws std::invalid_argument ("duplicate card in <what>") if one is already marked.
    void mark_cards_distinct(const std::vector<Card>& cards, const char* what) {
        for (const Card& c : cards) {
            const int idx = deck_index_from_card(c);
            if (test(idx)) {
                throw std::invalid_argument(std::string("duplicate card in ") + what);
            }
            set(idx);
        }
    }

    [[nodiscard]] int count() const { return popcount_u64(mask); }

    [[nodiscard]] std::vector<int> unused_indices() const {
        std::vector<int> out;
        out.reserve(52);
        for (int i = 0; i < 52; ++i) {
            if (!test(i)) {
                out.push_back(i);
            }
        }
        return out;
    }
};

}  // namespace poker
