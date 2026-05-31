#include "poker/side_pot_icm.hpp"

#include "poker/icm.hpp"
#include "poker/side_pot.hpp"

#include <stdexcept>

namespace poker {

std::vector<SidePotLayerTournamentEv> side_pot_layer_tournament_ev_delta(
    const std::vector<double>& table_stacks, const std::vector<double>& payouts,
    const std::size_t hero_index, const std::vector<double>& committed_chips,
    const std::vector<std::vector<double>>& equity_player_by_layer) {
    if (hero_index >= table_stacks.size()) {
        throw std::invalid_argument("sidePotLayerTournamentEvDelta: invalid hero index");
    }
    const auto layers = side_pot_ladder_from_commitments(committed_chips);
  std::vector<double> layer_pots;
    for (const auto& layer : layers) {
        layer_pots.push_back(layer.pot_chips);
    }
    const auto base_icm = icm_expected_payouts(table_stacks, payouts);
    std::vector<SidePotLayerTournamentEv> out;
    out.reserve(layers.size());
    for (std::size_t li = 0; li < layers.size(); ++li) {
        SidePotLayerTournamentEv row;
        double layer_eq = 0.0;
        if (hero_index < equity_player_by_layer.size() &&
            li < equity_player_by_layer[hero_index].size()) {
            layer_eq = equity_player_by_layer[hero_index][li];
        }
        row.chip_ev = layer_eq * layers[li].pot_chips;
        const double pot = layers[li].pot_chips;
        auto win_stacks = table_stacks;
        auto lose_stacks = table_stacks;
        win_stacks[hero_index] += pot;
        lose_stacks[hero_index] = std::max(0.0, lose_stacks[hero_index] - pot);
        const auto icm_win = icm_expected_payouts(win_stacks, payouts);
        const auto icm_lose = icm_expected_payouts(lose_stacks, payouts);
        row.icm_ev_win = icm_win[hero_index];
        row.icm_ev_lose = icm_lose[hero_index];
        row.icm_marginal = row.icm_ev_win - row.icm_ev_lose;
        out.push_back(row);
    }
    (void)base_icm;
    return out;
}

}  // namespace poker
