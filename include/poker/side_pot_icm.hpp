#pragma once

#include <cstddef>
#include <vector>

namespace poker {

struct SidePotLayerTournamentEv {
    double chip_ev{0.0};
    double icm_ev_win{0.0};
    double icm_ev_lose{0.0};
    double icm_marginal{0.0};
};

[[nodiscard]] std::vector<SidePotLayerTournamentEv> side_pot_layer_tournament_ev_delta(
    const std::vector<double>& table_stacks, const std::vector<double>& payouts,
    std::size_t hero_index, const std::vector<double>& committed_chips,
    const std::vector<std::vector<double>>& equity_player_by_layer);

}  // namespace poker
