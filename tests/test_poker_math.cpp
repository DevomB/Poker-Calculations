#include "poker/poker_math.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

TEST(PokerMath, PotOddsRatio) {
    EXPECT_DOUBLE_EQ(poker::pot_odds_ratio(100, 50), 50.0 / 150.0);
    EXPECT_DOUBLE_EQ(poker::pot_odds_ratio(100, 0), 0.0);
}

TEST(PokerMath, ExpectedValueCall) {
    const double ev = poker::expected_value_call(0.5, 100, 50);
    EXPECT_GT(ev, 0.0);
}

TEST(PokerMath, BreakevenMatchesPotOddsRatio) {
    EXPECT_DOUBLE_EQ(poker::breakeven_call_equity(100, 50), poker::pot_odds_ratio(100, 50));
}

TEST(PokerMath, Spr) {
    EXPECT_DOUBLE_EQ(poker::spr(90, 270), 3.0);
    EXPECT_DOUBLE_EQ(poker::spr(0, 0), 0.0);
    EXPECT_TRUE(std::isinf(poker::spr(0, 100)));
}

TEST(PokerMath, EffectiveStack) {
    EXPECT_DOUBLE_EQ(poker::effective_stack({}), 0.0);
    EXPECT_DOUBLE_EQ(poker::effective_stack({100, 200}), 100.0);
}

TEST(PokerMath, MinimumDefenseAndAlpha) {
    const double mdf = poker::minimum_defense_frequency(100, 50);
    const double alpha = poker::alpha_frequency(100, 50);
    EXPECT_DOUBLE_EQ(mdf + alpha, 1.0);
    EXPECT_DOUBLE_EQ(mdf, 100.0 / 150.0);
}

TEST(PokerMath, SprAfterCall) {
    // pot 100, call 50, stack 200 -> stack after 150, new pot 200 -> 0.75
    EXPECT_DOUBLE_EQ(poker::spr_after_call(100, 50, 200), 150.0 / 200.0);
}

TEST(PokerMath, SprAfterCallRejectsOvercall) {
    EXPECT_THROW(poker::spr_after_call(100, 50, 40), std::invalid_argument);
}

TEST(PokerMath, CommitmentRatio) {
    EXPECT_DOUBLE_EQ(poker::commitment_ratio(25, 100), 0.25);
    EXPECT_DOUBLE_EQ(poker::commitment_ratio(10, 0), 0.0);
}

TEST(PokerMath, BetAsPotFraction) {
    EXPECT_DOUBLE_EQ(poker::bet_as_pot_fraction(100, 50), 0.5);
    EXPECT_TRUE(std::isinf(poker::bet_as_pot_fraction(0, 50)));
}

TEST(PokerMath, BreakevenFoldPureBluff) {
    EXPECT_DOUBLE_EQ(poker::breakeven_fold_equity_pure_bluff(100, 50), 50.0 / 150.0);
}

TEST(PokerMath, BreakevenFoldSemiBluff) {
    // Good enough when called -> no FE needed
    EXPECT_DOUBLE_EQ(poker::breakeven_fold_equity_semi_bluff(100, 50, 0.5, 300), 0.0);
}

TEST(PokerMath, BluffToValueAndInverse) {
    const double b = poker::bluff_to_value_ratio(100, 50);
    EXPECT_GT(b, 0.0);
    EXPECT_DOUBLE_EQ(poker::value_to_bluff_ratio(100, 50), 1.0 / b);
}

TEST(PokerMath, ValueToBluffInfWhenNoBluffRatio) {
    EXPECT_TRUE(std::isinf(poker::value_to_bluff_ratio(100, 0)));
}

TEST(PokerMath, FormatPotOdds) {
    EXPECT_EQ(poker::format_pot_odds(100, 50, 2), "2:1");
}

TEST(PokerMath, ImpliedBreakevenInfinityAtZeroEquity) {
    EXPECT_TRUE(std::isinf(poker::implied_breakeven_future_win(100, 50, 0.0)));
}

TEST(PokerMath, HypergeometricOneCard) {
    EXPECT_DOUBLE_EQ(poker::hypergeometric_one_card_hit_probability(9, 47), 9.0 / 47.0);
}

TEST(PokerMath, KellyBinary) {
    const double f = poker::kelly_criterion_binary(0.6, 1.0);
    EXPECT_NEAR(f, 0.2, 1e-12);
}

TEST(PokerMath, ChubukovBreakevenStack) {
    const double s = poker::chubukov_symmetric_jam_breakeven_stack(150.0, 0.4);
    EXPECT_NEAR(s, 150.0 * 0.4 / (1.0 - 0.8), 1e-9);
}

TEST(PokerMath, ChubukovSymmetricJamEv) {
    EXPECT_NEAR(poker::chubukov_symmetric_jam_ev(100.0, 50.0, 0.4), 0.4 * 250.0 - 100.0, 1e-9);
}

TEST(PokerMath, ChubukovMaxJamStackChipsBinarySearch) {
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.4, 150.0, 1'000'000), 300);
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.4, 150.0, 100), 100);
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.51, 150.0, 100), 100);
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.0, 150.0, 100), 0);
}

TEST(PokerMath, MultiwaySymmetricBreakeven) {
    const double e = poker::multiway_symmetric_breakeven_call_equity(100, 50, 2);
    EXPECT_DOUBLE_EQ(e, 50.0 / (100 + 50 * 3));
}

TEST(PokerMath, RakeBreakevenCall) {
    const double e = poker::breakeven_call_equity_with_rake(100, 50, 0.05, 1e9);
    const double final_pot = 200.0;
    const double rake = 0.05 * final_pot;
    EXPECT_DOUBLE_EQ(e, 50.0 / (200.0 - rake));
}

TEST(PokerMath, UnionTwoCategoriesMatchesDisjoint) {
    const double u = 47.0;
    const double p1 = poker::flop_to_river_at_least_one_hit_union_two_categories(u, 5.0, 4.0, 0.0);
    const double p2 = poker::flop_to_river_at_least_one_hit_probability(9.0, u);
    EXPECT_DOUBLE_EQ(p1, p2);
}

TEST(PokerMath, PureBluffWithRake) {
    const double fe = poker::breakeven_fold_equity_pure_bluff_with_rake(100, 50, 0.0, 1e9);
    EXPECT_DOUBLE_EQ(fe, poker::breakeven_fold_equity_pure_bluff(100, 50));
}

TEST(PokerMath, TwoStreetPureBluffEvZeroAtSameFe) {
    const double fe = poker::two_street_pure_bluff_same_fold_equity(100, 50, 50);
    ASSERT_FALSE(std::isnan(fe));
    const double ev = poker::two_street_pure_bluff_ev(100, 50, 50, fe, fe);
    EXPECT_NEAR(ev, 0.0, 1e-9);
}

TEST(PokerMath, SecondStreetBreakevenMatchesSameFe) {
    const double fe = poker::two_street_pure_bluff_same_fold_equity(100, 50, 50);
    ASSERT_FALSE(std::isnan(fe));
    const double fe2 = poker::breakeven_fold_equity_second_street_pure_bluff(100, 50, 50, fe);
    EXPECT_NEAR(fe2, fe, 1e-9);
}

TEST(PokerMath, RunnerRunnerStraightGutshot) {
    const double p = poker::runner_runner_straight_draw_hit_probability(
        poker::Runner_runner_straight_draw_kind::GutshotFourOut, 0, 47.0);
    EXPECT_DOUBLE_EQ(p, poker::flop_to_river_at_least_one_hit_probability(4.0, 47.0));
}

TEST(PokerMath, MultiwayShareReducesBreakeven) {
    const double wta = poker::multiway_symmetric_breakeven_call_equity_with_share(
        100, 50, 1, poker::Multiway_symmetric_pot_share_model::WinnerTakesAll, 1.0);
    const double half = poker::multiway_symmetric_breakeven_call_equity_with_share(
        100, 50, 1, poker::Multiway_symmetric_pot_share_model::FixedHeroShareWhenWins, 0.5);
    EXPECT_DOUBLE_EQ(wta, poker::multiway_symmetric_breakeven_call_equity(100, 50, 1));
    EXPECT_GT(half, wta);
}

TEST(PokerMath, UnionFourCategoriesMatchesDisjointSum) {
    const double u = 47.0;
    const double p4 = poker::flop_to_river_at_least_one_hit_union_four_categories(
        u, 2.0, 2.0, 2.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    const double psum = poker::flop_to_river_at_least_one_hit_disjoint_outs_sum(u, {2.0, 2.0, 2.0, 2.0});
    EXPECT_DOUBLE_EQ(p4, psum);
}

TEST(PokerMath, FirstStreetBreakevenPairsWithEv) {
    const double fe2 = 0.35;
    const double fe1 = poker::breakeven_fold_equity_first_street_pure_bluff(100, 50, 50, fe2);
    const double ev = poker::two_street_pure_bluff_ev(100, 50, 50, fe1, fe2);
    EXPECT_NEAR(ev, 0.0, 1e-9);
}

TEST(PokerMath, HarringtonEffectiveActiveAntesMatchesSum) {
    const double m1 = poker::harrington_m_effective_active_antes(400, 1, 2, {1.0, 2.0, 1.0});
    const double m2 = poker::harrington_m(400, 1, 2, 4.0);
    EXPECT_DOUBLE_EQ(m1, m2);
}

TEST(PokerMath, ChubukovJamEvAndIntMaxStack) {
    EXPECT_NEAR(poker::chubukov_symmetric_jam_ev(50.0, 100.0, 0.4), 0.4 * 200.0 - 50.0, 1e-9);
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.4, 100.0, 500), 200);
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.4, 100.0, 150), 150);
    EXPECT_EQ(poker::chubukov_max_symmetric_jam_stack_chips_binary_search(0.4, 100.0, 0), 0);
}
