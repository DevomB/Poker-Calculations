#include "poker/monte_carlo.hpp"

#include "poker/card.hpp"

#include <gtest/gtest.h>

#include <random>

TEST(MonteCarlo, AAPreflopEquityRough) {
    std::vector<poker::Card> aa = {poker::Card{12, 0}, poker::Card{12, 1}};
    std::vector<poker::Card> board;
    std::mt19937 rng{1234};
    const float eq = poker::simulate_hand_outcome(aa, board, 6000, rng, 1);
    EXPECT_GT(eq, 0.78F);
    EXPECT_LT(eq, 0.92F);
}

TEST(MonteCarlo, ParallelMatchesSingleThreadedRoughly) {
    std::vector<poker::Card> hole = {poker::Card{11, 0}, poker::Card{11, 1}};
    std::vector<poker::Card> board = {poker::Card{2, 0}, poker::Card{5, 1}, poker::Card{9, 2}};
    std::mt19937 rng{99};
    const float a = poker::simulate_hand_outcome(hole, board, 4000, rng, 1);
    const float b =
        poker::parallel_hand_simulation(hole, board, 4000, 99U, 1, 4);
    EXPECT_NEAR(a, b, 0.08F);
}

TEST(MonteCarlo, BatchMatchesSequential) {
    std::vector<poker::Card> hole = {poker::Card{12, 0}, poker::Card{11, 1}};
    std::vector<poker::Card> board = {poker::Card{2, 0}, poker::Card{5, 1}, poker::Card{9, 2}};
    std::vector<poker::SimSpot> spots(2);
    spots[0].hole = hole;
    spots[0].board = board;
    spots[0].num_simulations = 3000;
    spots[0].seed = 7;
    spots[0].villains = 1;
    spots[1] = spots[0];
    spots[1].seed = 8;

    std::vector<float> batch_out;
    poker::simulate_hand_outcome_batch(spots, batch_out);
    ASSERT_EQ(batch_out.size(), 2u);

    std::mt19937 rng0(7);
    const float s0 = poker::simulate_hand_outcome(hole, board, 3000, rng0, 1);
    std::mt19937 rng1(8);
    const float s1 = poker::simulate_hand_outcome(hole, board, 3000, rng1, 1);
    EXPECT_FLOAT_EQ(batch_out[0], s0);
    EXPECT_FLOAT_EQ(batch_out[1], s1);
}
