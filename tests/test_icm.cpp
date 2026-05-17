#include "poker/icm.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

TEST(Icm, WinProbTwoPlayerEqual) {
    const std::vector<double> stacks = {500.0, 500.0};
    const auto w = poker::icm_win_probabilities_harville(stacks);
    ASSERT_EQ(w.size(), 2U);
    EXPECT_NEAR(w[0], 0.5, 1e-9);
    EXPECT_NEAR(w[1], 0.5, 1e-9);
}

TEST(Icm, ExpectedPayoutsSum) {
    const std::vector<double> stacks = {400.0, 300.0, 300.0};
    const std::vector<double> payouts = {100.0, 50.0, 20.0};
    const auto ev = poker::icm_expected_payouts(stacks, payouts);
    ASSERT_EQ(ev.size(), 3U);
    double sum = 0.0;
    for (double x : ev) {
        sum += x;
    }
    EXPECT_NEAR(sum, 170.0, 1e-6);
}

TEST(Icm, BubbleFactorPositive) {
    const std::vector<double> stacks = {1200.0, 800.0};
    const std::vector<double> payouts = {500.0, 200.0};
    const double bf = poker::icm_pairwise_bubble_factor(stacks, payouts, 0, 1, 100.0);
    EXPECT_TRUE(std::isfinite(bf));
    EXPECT_GT(bf, 0.0);
}
