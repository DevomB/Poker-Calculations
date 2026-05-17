#include "poker/side_pot.hpp"

#include <gtest/gtest.h>
#include <vector>

TEST(SidePot, ThreeWayLadder) {
    const std::vector<double> committed = {50.0, 100.0, 150.0};
    const auto layers = poker::side_pot_ladder_from_commitments(committed);
    ASSERT_EQ(layers.size(), 3U);
    double total = 0.0;
    for (const auto& L : layers) {
        total += L.pot_chips;
    }
    EXPECT_DOUBLE_EQ(total, 300.0);
    EXPECT_DOUBLE_EQ(layers[0].pot_chips, 150.0);
    EXPECT_DOUBLE_EQ(layers[1].pot_chips, 100.0);
    EXPECT_DOUBLE_EQ(layers[2].pot_chips, 50.0);
}

TEST(SidePot, LayeredEv) {
    const std::vector<double> pots = {100.0, 50.0};
    const std::vector<std::vector<double>> eq = {
        {0.6, 0.5},
        {0.4, 0.5},
    };
    const auto ev = poker::layered_pot_chip_ev_from_equities(pots, eq);
    ASSERT_EQ(ev.size(), 2U);
    EXPECT_DOUBLE_EQ(ev[0], 100.0 * 0.6 + 50.0 * 0.5);
    EXPECT_DOUBLE_EQ(ev[1], 100.0 * 0.4 + 50.0 * 0.5);
}
