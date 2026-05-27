#pragma once

#include "poker/game_state.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace poker {

inline constexpr char kStateCodecMagic[4] = {'P', 'K', 'S', 'T'};
inline constexpr std::uint8_t kStateCodecVersion = 1;
inline constexpr std::size_t kStateCodecMaxPlayers = 10;

[[nodiscard]] std::vector<std::uint8_t> encode_poker_state(const PokerGameState& state, std::string* err = nullptr);

[[nodiscard]] bool decode_poker_state(const std::uint8_t* data, std::size_t len, PokerGameState& out,
                                      std::string* err = nullptr);

}  // namespace poker
