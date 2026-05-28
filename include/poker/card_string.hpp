#pragma once

#include "poker/card.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace poker {

/// Deck id 0..51: rank * 4 + suit (rank 0=2 .. 12=A, suit 0=c .. 3=s).
[[nodiscard]] int deck_index_from_card(const Card& c);

/// Inverse of deck_index_from_card; throws std::invalid_argument if idx not in 0..51.
[[nodiscard]] Card card_from_deck_index(int idx);

/**
 * Parse packed card bytes (each 0..51). On failure sets *err and returns false.
 * Does not check for duplicate cards.
 */
[[nodiscard]] bool parse_packed_cards(const std::uint8_t* data, std::size_t n, std::vector<Card>& out,
                                      std::string* err);

/// True if any two cards in the list are equal (single-pass deck-index bitmap).
[[nodiscard]] bool cards_have_duplicate(const std::vector<Card>& cards);

/**
 * Parse token at [p, p+n) after ASCII trim; no allocation. Invalid rank/suit LUT entries (255) fail.
 */
[[nodiscard]] bool parse_card_string_unchecked(const char* p, std::size_t n, Card& out);

/// Trim ASCII whitespace; parses ranks `23456789TJQKA` and `10`, suits `cdhs` (case-insensitive rank/suit).
[[nodiscard]] bool parse_card_string(const std::string& raw, Card& out);

/// True if any two packed bytes are equal or invalid; sets *err on invalid byte index.
[[nodiscard]] bool packed_cards_have_duplicate(const std::uint8_t* data, std::size_t n, std::string* err);

/// True if any two entries parse to the same card. Throws if any string is not a valid card.
[[nodiscard]] bool card_strings_have_duplicate(const std::vector<std::string>& cards);

/// Deterministic two-character rank+suit string (`Th`, `Ac`, …) after parse.
[[nodiscard]] std::string canonical_card_string(const std::string& raw);

/**
 * Parse a run of cards from concatenated or whitespace-separated text (`AhKh`, `Ah Kh`, `10hKd`).
 * Throws `std::invalid_argument` on invalid token or duplicate cards.
 */
[[nodiscard]] std::vector<std::string> parse_compact_card_list(const std::string& raw);

/// Same scan as parse_compact_card_list; returns deck ids 0..51 per card.
[[nodiscard]] std::vector<std::uint8_t> parse_compact_card_list_indices(const std::string& raw);

}  // namespace poker
