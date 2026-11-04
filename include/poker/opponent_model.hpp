#pragma once

#include "poker/types.hpp"

#include <cstddef>

namespace poker {

/// Running action frequencies for one opponent. `aggression_factor` is a normalized policy score in
/// [0, 1] (`clamp(2 * raise_rate + 0.25)`, 0.5 with no samples), not the conventional
/// (bets + raises) / calls aggression factor. `decide_action` reads only this score.
class OpponentModel {
public:
    float aggression_factor{0.5F};
    float call_frequency{0.33F};
    float fold_frequency{0.33F};

    void update_model(Action opponent_action);

    [[nodiscard]] std::size_t samples() const { return total_; }

private:
    std::size_t raises_{0};
    std::size_t calls_{0};
    std::size_t folds_{0};
    std::size_t checks_{0};
    std::size_t total_{0};

    void recompute_rates();
};

}  // namespace poker
