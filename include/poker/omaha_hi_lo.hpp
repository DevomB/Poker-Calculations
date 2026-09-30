#pragma once

#include "poker/card.hpp"
#include "poker/hand_evaluator.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace poker {

/// Sentinel: no qualifying 8-or-better low. Valid lows use a smaller packed key.
inline constexpr std::uint32_t kOmahaNoLow = 0xFFFFFFFFu;

/// Best 8-or-better low under Omaha 2-hole + 3-board. Ace is 1; ranks are high-to-low.
struct OmahaLoHand {
    bool qualifies{false};
    std::array<std::uint8_t, 5> ranks{};
    std::uint32_t key{kOmahaNoLow};
};

struct OmahaHiLoHands {
    HandEvaluation hi{};
    OmahaLoHand lo{};
};

/// HU PLO-8: high-hand equity, low-hand equity, P(scoop), P(quarter), pot share.
struct OmahaHiLoEquity {
    double hi_equity{0.0};
    double lo_equity{0.0};
    double scoop_equity{0.0};
    double quarter_rate{0.0};
    double pot_share{0.0};
};

struct OmahaHiLoNuttedness {
    bool hi_nuts{false};
    bool lo_nuts{false};
    bool scoop_nuts{false};
};

/// Best qualifying low, or `qualifies=false` / `kOmahaNoLow`. Hole 4, board 3–5.
[[nodiscard]] OmahaLoHand evaluate_omaha_lo_hand(const std::vector<Card>& hole,
                                                 const std::vector<Card>& board);

[[nodiscard]] bool omaha_lo_qualifies(const std::vector<Card>& hole, const std::vector<Card>& board);

/// High via `evaluate_omaha_best_hand`; low via 8-or-better (straights/flushes ignored).
[[nodiscard]] OmahaHiLoHands evaluate_omaha_hi_lo(const std::vector<Card>& hole,
                                                  const std::vector<Card>& board);

/// Exact HU vs a known 4-card hand. Board 0–5. No low → high takes the full pot.
[[nodiscard]] OmahaHiLoEquity exact_hu_omaha_hi_lo_equity(const std::vector<Card>& hero,
                                                          const std::vector<Card>& villain,
                                                          const std::vector<Card>& board);

/// `villain` empty → uniform random 4-card opponent. Board 0–5.
[[nodiscard]] OmahaHiLoEquity simulate_omaha_hi_lo_equity(const std::vector<Card>& hero,
                                                          const std::vector<Card>& villain,
                                                          const std::vector<Card>& board, int trials,
                                                          std::mt19937& rng);

[[nodiscard]] double omaha_scoop_probability_mc(const std::vector<Card>& hero,
                                                const std::vector<Card>& villain,
                                                const std::vector<Card>& board, int trials,
                                                std::mt19937& rng);

/// P(exactly one side ties and the other is won/lost). No-low runouts are not quarters.
[[nodiscard]] double omaha_quarter_probability_mc(const std::vector<Card>& hero,
                                                  const std::vector<Card>& villain,
                                                  const std::vector<Card>& board, int trials,
                                                  std::mt19937& rng);

[[nodiscard]] bool omaha_lo_nuts_on_board(const std::vector<Card>& hero, const std::vector<Card>& board,
                                          const std::vector<Card>& extra_dead = {});

[[nodiscard]] OmahaHiLoNuttedness omaha_hi_lo_nuttedness(const std::vector<Card>& hero,
                                                         const std::vector<Card>& board,
                                                         const std::vector<Card>& extra_dead = {});

/// 3-way chip EV with hi/lo halves and quartering. Length 3. Board 0–5.
[[nodiscard]] std::vector<double> omaha_hi_lo_multiway_mc(
    const std::vector<std::vector<Card>>& holes, const std::vector<Card>& board, int trials,
    std::mt19937& rng);

}  // namespace poker
