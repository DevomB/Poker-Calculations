#include "poker/side_pot.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace poker {

std::vector<Side_pot_layer> side_pot_ladder_from_commitments(
    const std::vector<double>& committed_chips) {
    if (committed_chips.empty()) {
        return {};
    }
    for (double c : committed_chips) {
        if (!std::isfinite(c) || c < 0.0) {
            throw std::invalid_argument("committed chips must be finite and non-negative");
        }
    }
    std::set<double> levels;
    for (double c : committed_chips) {
        if (c > 0.0) {
            levels.insert(c);
        }
    }
    if (levels.empty()) {
        return {};
    }
    std::vector<Side_pot_layer> layers;
    double prev = 0.0;
    for (double u : levels) {
        const double delta = u - prev;
        if (delta <= 0.0) {
            prev = u;
            continue;
        }
        Side_pot_layer layer{};
        layer.player_cap_contribution.resize(committed_chips.size(), 0.0);
        double pot = 0.0;
        for (std::size_t i = 0; i < committed_chips.size(); ++i) {
            if (committed_chips[i] >= u - 1e-12) {
                layer.player_cap_contribution[i] = delta;
                pot += delta;
            }
        }
        layer.pot_chips = pot;
        layers.push_back(std::move(layer));
        prev = u;
    }
    return layers;
}

std::vector<double> layered_pot_chip_ev_from_equities(
    const std::vector<double>& layer_pot_chips,
    const std::vector<std::vector<double>>& equity_player_by_layer) {
    if (layer_pot_chips.empty()) {
        return {};
    }
    if (equity_player_by_layer.empty()) {
        throw std::invalid_argument("equity matrix must be non-empty");
    }
    const std::size_t n_players = equity_player_by_layer.size();
    const std::size_t n_layers = layer_pot_chips.size();
    if (equity_player_by_layer[0].size() != n_layers) {
        throw std::invalid_argument("equity columns must match number of pot layers");
    }
    for (double p : layer_pot_chips) {
        if (!std::isfinite(p) || p < 0.0) {
            throw std::invalid_argument("layer pot chips must be finite and non-negative");
        }
    }
    for (std::size_t j = 0; j < n_layers; ++j) {
        double col = 0.0;
        for (std::size_t i = 0; i < n_players; ++i) {
            const double e = equity_player_by_layer[i][j];
            if (!std::isfinite(e) || e < 0.0 || e > 1.0) {
                throw std::invalid_argument("equities must be in [0,1]");
            }
            col += e;
        }
        if (std::abs(col - 1.0) > 1e-6) {
            throw std::invalid_argument("each layer's equities must sum to 1");
        }
    }
    std::vector<double> ev(n_players, 0.0);
    for (std::size_t i = 0; i < n_players; ++i) {
        for (std::size_t j = 0; j < n_layers; ++j) {
            ev[i] += layer_pot_chips[j] * equity_player_by_layer[i][j];
        }
    }
    return ev;
}

}  // namespace poker
