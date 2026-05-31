#include "binding_cooperative_icm.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/icm.hpp"
#include "poker/icm_sensitivity.hpp"
#include "poker/shapley_icm.hpp"
#include "poker/side_pot_icm.hpp"
#include "poker/tournament_duel.hpp"

using poker_bind::read_f64_matrix;
using poker_bind::read_f64_vector;
using poker_bind::write_f64_matrix_flat;
using poker_bind::write_f64_vector;

Napi::Value IcmShapleyValues(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "icmShapleyValues(stacks, payouts[, options])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const char* method = "exact";
    std::size_t perms = 200000;
    if (info.Length() >= 3 && info[2].IsObject()) {
        const Napi::Object o = info[2].As<Napi::Object>();
        if (o.Has("method") && o.Get("method").IsString()) {
            method = o.Get("method").As<Napi::String>().Utf8Value().c_str();
        }
        if (o.Has("permutations") && o.Get("permutations").IsNumber()) {
            perms = static_cast<std::size_t>(o.Get("permutations").As<Napi::Number>().Uint32Value());
        }
    }
    POKER_TRY(env, {
        const auto r = poker::icm_shapley_values(stacks, payouts, method, perms);
        Napi::Object out = Napi::Object::New(env);
        out.Set("values", write_f64_vector(env, r.values, poker_bind::F64ReturnFormat::Array));
        out.Set("method", Napi::String::New(env, r.method));
        if (!r.se.empty()) {
            out.Set("se", write_f64_vector(env, r.se, poker_bind::F64ReturnFormat::Array));
        }
        return out;
    });
}

Napi::Value IcmHarvilleStackJacobian(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "icmHarvilleStackJacobian(stacks, payouts[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto fmt = poker_bind::parse_return_format(info, 2);
    POKER_TRY(env, {
        const auto j = poker::icm_harville_stack_jacobian(stacks, payouts);
        const std::size_t n = stacks.size();
        std::vector<std::vector<double>> mat(n, std::vector<double>(n));
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t k = 0; k < n; ++k) {
                mat[i][k] = j[i * n + k];
            }
        }
        return write_f64_matrix_flat(env, mat, fmt);
    });
}

Napi::Value IcmHarvilleSkillAdjustedPayouts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "icmHarvilleSkillAdjustedPayouts(stacks, payouts, skillWeights, blend[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> skill;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "skillWeights", skill, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double blend = info[3].As<Napi::Number>().DoubleValue();
    const auto fmt = poker_bind::parse_return_format(info, 4);
    POKER_TRY(env, {
        const auto ev = poker::icm_harville_skill_adjusted_payouts(stacks, payouts, skill, blend);
        return write_f64_vector(env, ev, fmt);
    });
}

Napi::Value IcmFieldPressureIndex(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "icmFieldPressureIndex(stacks, payouts, heroIndex, potChips)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const double pot = info[3].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r = poker::icm_field_pressure_index(stacks, payouts, hero, pot);
        Napi::Object out = Napi::Object::New(env);
        out.Set("index", Napi::Number::New(env, r.index));
        out.Set("pairwiseBubbleFactors",
                write_f64_vector(env, r.pairwise_bubble_factors, poker_bind::F64ReturnFormat::Array));
        out.Set("argmaxVillain", Napi::Number::New(env, static_cast<double>(r.argmax_villain)));
        return out;
    });
}

Napi::Value IcmChopNegotiationAnalysis(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "icmChopNegotiationAnalysis(stacks, payouts)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::icm_chop_negotiation_analysis(stacks, payouts);
        Napi::Object out = Napi::Object::New(env);
        out.Set("chipChop", write_f64_vector(env, r.chip_chop, poker_bind::F64ReturnFormat::Array));
        out.Set("icm", write_f64_vector(env, r.icm, poker_bind::F64ReturnFormat::Array));
        out.Set("surplus", write_f64_vector(env, r.surplus, poker_bind::F64ReturnFormat::Array));
        out.Set("totalPrizePool", Napi::Number::New(env, r.total_prize_pool));
        Napi::Array pairs = Napi::Array::New(env, r.pareto_pairs.size());
        for (std::size_t i = 0; i < r.pareto_pairs.size(); ++i) {
            Napi::Object p = Napi::Object::New(env);
            p.Set("i", Napi::Number::New(env, static_cast<double>(r.pareto_pairs[i].i)));
            p.Set("j", Napi::Number::New(env, static_cast<double>(r.pareto_pairs[i].j)));
            p.Set("maxTransfer", Napi::Number::New(env, r.pareto_pairs[i].max_transfer));
            pairs.Set(static_cast<uint32_t>(i), p);
        }
        out.Set("paretoPairs", pairs);
        return out;
    });
}

Napi::Value TournamentDuelAbsorptionProbabilities(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "tournamentDuelAbsorptionProbabilities(heroStack, villainStack, winProbabilityPerHand, chipsPerAllIn[, winnerPrize])");
    const double hero = info[0].As<Napi::Number>().DoubleValue();
    const double vil = info[1].As<Napi::Number>().DoubleValue();
    const double p = info[2].As<Napi::Number>().DoubleValue();
    const double chips = info[3].As<Napi::Number>().DoubleValue();
    const double prize = info.Length() >= 5 ? info[4].As<Napi::Number>().DoubleValue() : 0.0;
    POKER_TRY(env, {
        const auto r = poker::tournament_duel_absorption_probabilities(hero, vil, p, chips, prize);
        Napi::Object out = Napi::Object::New(env);
        out.Set("heroWinProbability", Napi::Number::New(env, r.hero_win_probability));
        out.Set("expectedHands", Napi::Number::New(env, r.expected_hands));
        out.Set("heroPrizeEv", Napi::Number::New(env, r.hero_prize_ev));
        return out;
    });
}

Napi::Value SidePotLayerTournamentEvDelta(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "sidePotLayerTournamentEvDelta(tableStacks, payouts, heroIndex, committedChips, equityPlayerByLayer)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> committed;
    std::vector<std::vector<double>> equities;
    if (!read_f64_vector(info[0], "tableStacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[3], "committedChips", committed, &err) ||
        !read_f64_matrix(info[4], "equityPlayerByLayer", equities, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    POKER_TRY(env, {
        const auto rows =
            poker::side_pot_layer_tournament_ev_delta(stacks, payouts, hero, committed, equities);
        Napi::Array arr = Napi::Array::New(env, rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            Napi::Object row = Napi::Object::New(env);
            row.Set("chipEv", Napi::Number::New(env, rows[i].chip_ev));
            row.Set("icmEvWin", Napi::Number::New(env, rows[i].icm_ev_win));
            row.Set("icmEvLose", Napi::Number::New(env, rows[i].icm_ev_lose));
            row.Set("icmMarginal", Napi::Number::New(env, rows[i].icm_marginal));
            arr.Set(static_cast<uint32_t>(i), row);
        }
        return arr;
    });
}
