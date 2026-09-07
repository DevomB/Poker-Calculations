#include "binding_pko.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/pko.hpp"

using poker_bind::read_f64_matrix;
using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

bool parse_outcomes(const Napi::Value& v, double& p_win, double& p_tie, double& p_lose, std::string* err) {
    if (v.IsNumber()) {
        p_win = v.As<Napi::Number>().DoubleValue();
        p_tie = 0.0;
        p_lose = 1.0 - p_win;
        return true;
    }
    std::vector<double> parts;
    if (!read_f64_vector(v, "equity", parts, err)) {
        return false;
    }
    if (parts.size() == 1) {
        p_win = parts[0];
        p_tie = 0.0;
        p_lose = 1.0 - p_win;
        return true;
    }
    if (parts.size() == 2) {
        // equity + tie: equity includes half the ties.
        p_tie = parts[1];
        p_win = parts[0] - 0.5 * p_tie;
        p_lose = 1.0 - p_win - p_tie;
        return true;
    }
    if (parts.size() == 3) {
        p_win = parts[0];
        p_tie = parts[1];
        p_lose = parts[2];
        return true;
    }
    if (err) {
        *err = "equity: number, [equity, tie], or [win, tie, lose]";
    }
    return false;
}

bool flatten_knockouts(const Napi::Value& v, std::size_t n, std::vector<double>& out, std::string* err) {
    if (v.IsArray()) {
        const Napi::Array a = v.As<Napi::Array>();
        if (a.Length() > 0 && a.Get(0u).IsArray()) {
            std::vector<std::vector<double>> mat;
            if (!read_f64_matrix(v, "knockouts", mat, err)) {
                return false;
            }
            if (mat.size() != n) {
                if (err) {
                    *err = "knockouts: matrix row count must match players";
                }
                return false;
            }
            out.clear();
            out.reserve(n * n);
            for (const auto& row : mat) {
                if (row.size() != n) {
                    if (err) {
                        *err = "knockouts: matrix must be n×n";
                    }
                    return false;
                }
                out.insert(out.end(), row.begin(), row.end());
            }
            return true;
        }
    }
    return read_f64_vector(v, "knockouts", out, err);
}

Napi::Object write_spot(Napi::Env env, const poker::PkoSpotEvResult& r, const char* take_key) {
    Napi::Object out = Napi::Object::New(env);
    out.Set(take_key, Napi::Number::New(env, r.take_ev));
    out.Set("foldEv", Napi::Number::New(env, r.fold_ev));
    out.Set("delta", Napi::Number::New(env, r.delta));
    return out;
}

}  // namespace

Napi::Value PkoKnockoutProbabilityMatrix(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "pkoKnockoutProbabilityMatrix(stacks)");
    std::string err;
    std::vector<double> stacks;
    if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::pko_knockout_probability_matrix(stacks);
        Napi::Object out = Napi::Object::New(env);
        out.Set("n", Napi::Number::New(env, static_cast<double>(r.n)));
        out.Set("matrix", write_f64_vector(env, r.flat, poker_bind::F64ReturnFormat::Float64));
        return out;
    });
}

Napi::Value PkoExpectedBountyCollection(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "pkoExpectedBountyCollection(stacks, bountyValues[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto fmt = poker_bind::parse_return_format(info, 2);
    POKER_TRY(env, { return write_f64_vector(env, poker::pko_expected_bounty_collection(stacks, bounties), fmt); });
}

Napi::Value PkoIcmbuPayouts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3, "pkoIcmbuPayouts(stacks, payouts, bountyValues[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto fmt = poker_bind::parse_return_format(info, 3);
    POKER_TRY(env, {
        const auto r = poker::pko_icmbu_payouts(stacks, payouts, bounties);
        Napi::Object out = Napi::Object::New(env);
        out.Set("icm", write_f64_vector(env, r.icm, fmt));
        out.Set("bounty", write_f64_vector(env, r.bounty, fmt));
        out.Set("icmbu", write_f64_vector(env, r.icmbu, fmt));
        return out;
    });
}

Napi::Value PkoBountyRiskPremium(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3, "pkoBountyRiskPremium(stacks, payouts, bountyValues)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::pko_bounty_risk_premium(stacks, payouts, bounties);
        Napi::Object out = Napi::Object::New(env);
        out.Set("freezeoutIcm", write_f64_vector(env, r.freezeout_icm, poker_bind::F64ReturnFormat::Array));
        out.Set("icmbu", write_f64_vector(env, r.icmbu, poker_bind::F64ReturnFormat::Array));
        out.Set("icmbuMinusFreezeout",
                write_f64_vector(env, r.icmbu_minus_freezeout, poker_bind::F64ReturnFormat::Array));
        out.Set("chipShareBountyEv",
                write_f64_vector(env, r.chip_share_bounty_ev, poker_bind::F64ReturnFormat::Array));
        out.Set("bountyRiskPremium",
                write_f64_vector(env, r.bounty_risk_premium, poker_bind::F64ReturnFormat::Array));
        return out;
    });
}

Napi::Value PkoCallEvVsShove(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 7,
                  "pkoCallEvVsShove(stacks, payouts, bountyValues, heroIndex, villainIndex, pot, "
                  "heroEquityIfCall)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    if (!info[3].IsNumber() || !info[4].IsNumber() || !info[5].IsNumber()) {
        POKER_FAIL_TYPE(env, "pkoCallEvVsShove: heroIndex, villainIndex, pot must be numbers");
    }
    const std::size_t hero = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
    const std::size_t vil = static_cast<std::size_t>(info[4].As<Napi::Number>().Uint32Value());
    const double pot = info[5].As<Napi::Number>().DoubleValue();
    double p_win = 0, p_tie = 0, p_lose = 0;
    if (!parse_outcomes(info[6], p_win, p_tie, p_lose, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r = poker::pko_call_ev_vs_shove(stacks, payouts, bounties, hero, vil, pot, p_win, p_tie,
                                                   p_lose);
        return write_spot(env, r, "callEv");
    });
}

Napi::Value PkoJamEvVsFold(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 8,
                  "pkoJamEvVsFold(stacks, payouts, bountyValues, heroIndex, villainIndex, pot, "
                  "foldEquity, equityWhenCalled)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    if (!info[3].IsNumber() || !info[4].IsNumber() || !info[5].IsNumber() || !info[6].IsNumber()) {
        POKER_FAIL_TYPE(env, "pkoJamEvVsFold: heroIndex, villainIndex, pot, foldEquity must be numbers");
    }
    const std::size_t hero = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
    const std::size_t vil = static_cast<std::size_t>(info[4].As<Napi::Number>().Uint32Value());
    const double pot = info[5].As<Napi::Number>().DoubleValue();
    const double fe = info[6].As<Napi::Number>().DoubleValue();
    double p_win = 0, p_tie = 0, p_lose = 0;
    if (!parse_outcomes(info[7], p_win, p_tie, p_lose, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    POKER_TRY(env, {
        const auto r =
            poker::pko_jam_ev_vs_fold(stacks, payouts, bounties, hero, vil, pot, fe, p_win, p_tie, p_lose);
        return write_spot(env, r, "jamEv");
    });
}

Napi::Value MysteryBountyExpectedValue(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "mysteryBountyExpectedValue(values[, weights[, k]])");
    std::string err;
    std::vector<double> values;
    if (!read_f64_vector(info[0], "values", values, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> weights;
    int k = 0;
    if (info.Length() >= 2 && !info[1].IsUndefined() && !info[1].IsNull()) {
        if (info[1].IsNumber() && info.Length() == 2) {
            k = info[1].As<Napi::Number>().Int32Value();
        } else if (!read_f64_vector(info[1], "weights", weights, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    if (info.Length() >= 3 && info[2].IsNumber()) {
        k = info[2].As<Napi::Number>().Int32Value();
    }
    POKER_TRY(env, {
        const auto r = poker::mystery_bounty_expected_value(values, weights, k);
        Napi::Object out = Napi::Object::New(env);
        out.Set("oneDraw", Napi::Number::New(env, r.one_draw));
        out.Set("allRemaining", Napi::Number::New(env, r.all_remaining));
        out.Set("sampleK", Napi::Number::New(env, r.sample_k));
        out.Set("k", Napi::Number::New(env, static_cast<double>(r.k)));
        return out;
    });
}

Napi::Value ProgressiveKoPostedBounty(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "progressiveKoPostedBounty(baseBounties, knockouts, carryFraction[, returnFormat])");
    std::string err;
    std::vector<double> base;
    if (!read_f64_vector(info[0], "baseBounties", base, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    std::vector<double> knockouts;
    if (!flatten_knockouts(info[1], base.size(), knockouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    if (!info[2].IsNumber()) {
        POKER_FAIL_TYPE(env, "progressiveKoPostedBounty: carryFraction must be a number");
    }
    const double carry = info[2].As<Napi::Number>().DoubleValue();
    const auto fmt = poker_bind::parse_return_format(info, 3);
    POKER_TRY(env, {
        return write_f64_vector(env, poker::progressive_ko_posted_bounty(base, knockouts, carry), fmt);
    });
}

Napi::Value PkoCoveringHuntEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 6,
                  "pkoCoveringHuntEv(stacks, payouts, bountyValues, hunterIndex, preyIndex, pot[, "
                  "equity])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    if (!info[3].IsNumber() || !info[4].IsNumber() || !info[5].IsNumber()) {
        POKER_FAIL_TYPE(env, "pkoCoveringHuntEv: hunterIndex, preyIndex, pot must be numbers");
    }
    const std::size_t hunter = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
    const std::size_t prey = static_cast<std::size_t>(info[4].As<Napi::Number>().Uint32Value());
    const double pot = info[5].As<Napi::Number>().DoubleValue();
    double equity = 0.0;
    bool provided = false;
    if (info.Length() >= 7 && info[6].IsNumber()) {
        equity = info[6].As<Napi::Number>().DoubleValue();
        provided = true;
    }
    POKER_TRY(env, {
        const auto r =
            poker::pko_covering_hunt_ev(stacks, payouts, bounties, hunter, prey, pot, equity, provided);
        Napi::Object out = Napi::Object::New(env);
        out.Set("huntEv", Napi::Number::New(env, r.hunt_ev));
        out.Set("checkDownEv", Napi::Number::New(env, r.check_down_ev));
        out.Set("delta", Napi::Number::New(env, r.delta));
        out.Set("equityUsed", Napi::Number::New(env, r.equity_used));
        return out;
    });
}

Napi::Value PkoWinnerTakeRemainingBounties(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "pkoWinnerTakeRemainingBounties(stacks, payouts, remainingBountyPool[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) ||
        !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    if (!info[2].IsNumber()) {
        POKER_FAIL_TYPE(env, "pkoWinnerTakeRemainingBounties: remainingBountyPool must be a number");
    }
    const double pool = info[2].As<Napi::Number>().DoubleValue();
    const auto fmt = poker_bind::parse_return_format(info, 3);
    POKER_TRY(env, {
        const auto r = poker::pko_winner_take_remaining_bounties(stacks, payouts, pool);
        Napi::Object out = Napi::Object::New(env);
        out.Set("adjustedPayouts", write_f64_vector(env, r.adjusted_payouts, fmt));
        out.Set("ev", write_f64_vector(env, r.ev, fmt));
        out.Set("winProbabilities", write_f64_vector(env, r.win_probabilities, fmt));
        out.Set("bountyToWinnerEv", write_f64_vector(env, r.bounty_to_winner_ev, fmt));
        return out;
    });
}
