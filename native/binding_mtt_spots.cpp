#include "binding_mtt_spots.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/mtt_spots.hpp"

#include <array>
#include <cstring>
#include <string>

using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

Napi::Float64Array to_f64(Napi::Env env, const std::array<double, poker::kNashHandCount>& a) {
    Napi::ArrayBuffer buf = Napi::ArrayBuffer::New(env, poker::kNashHandCount * sizeof(double));
    std::memcpy(buf.Data(), a.data(), poker::kNashHandCount * sizeof(double));
    return Napi::Float64Array::New(env, poker::kNashHandCount, buf, 0);
}

Napi::Object spot_object(Napi::Env env, const poker::SpotChipEv& r, const char* take_key) {
    Napi::Object out = Napi::Object::New(env);
    out.Set(take_key, Napi::Number::New(env, r.take_ev));
    out.Set("foldEv", Napi::Number::New(env, r.fold_ev));
    out.Set("delta", Napi::Number::New(env, r.delta));
    return out;
}

}  // namespace

Napi::Value SpinGoPayouts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "spinGoPayouts(multiplier, buyin[, winnerTakeAll, returnFormat])");
    const double multiplier = info[0].As<Napi::Number>().DoubleValue();
    const double buyin = info[1].As<Napi::Number>().DoubleValue();
    bool wta = false;
    std::size_t fmt_i = 2;
    if (info.Length() >= 3 && info[2].IsBoolean()) {
        wta = info[2].As<Napi::Boolean>().Value();
        fmt_i = 3;
    } else if (info.Length() >= 3 && info[2].IsNumber()) {
        wta = info[2].As<Napi::Number>().Int32Value() != 0;
        fmt_i = 3;
    }
    const auto fmt = poker_bind::parse_return_format(info, fmt_i);
    POKER_TRY(env, { return write_f64_vector(env, poker::spin_go_payouts(multiplier, buyin, wta), fmt); });
}

Napi::Value SpinGoIcmEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 2, "spinGoIcmEv(stacks, payouts[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto fmt = poker_bind::parse_return_format(info, 2);
    POKER_TRY(env, { return write_f64_vector(env, poker::spin_go_icm_ev(stacks, payouts), fmt); });
}

Napi::Value SpinGoNashJamCall(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "spinGoNashJamCall(btnStack, sbStack, bbStack, payouts[, smallBlind, bigBlind, ante])");
    std::string err;
    std::vector<double> payouts;
    if (!read_f64_vector(info[3], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const double btn = info[0].As<Napi::Number>().DoubleValue();
    const double sb_stack = info[1].As<Napi::Number>().DoubleValue();
    const double bb_stack = info[2].As<Napi::Number>().DoubleValue();
    const double sb = info.Length() >= 5 && info[4].IsNumber() ? info[4].As<Napi::Number>().DoubleValue() : 0.5;
    const double bb = info.Length() >= 6 && info[5].IsNumber() ? info[5].As<Napi::Number>().DoubleValue() : 1.0;
    const double ante = info.Length() >= 7 && info[6].IsNumber() ? info[6].As<Napi::Number>().DoubleValue() : 0.0;
    POKER_TRY(env, {
        const auto r = poker::spin_go_nash_jam_call(btn, sb_stack, bb_stack, payouts, sb, bb, ante);
        Napi::Object out = Napi::Object::New(env);
        out.Set("jam", to_f64(env, r.jam));
        out.Set("sbCall", to_f64(env, r.sb_call));
        out.Set("bbCall", to_f64(env, r.bb_call));
        out.Set("iterations", Napi::Number::New(env, r.iterations));
        return out;
    });
}

Napi::Value PkoFgsPayouts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 6,
                  "pkoFgsPayouts(stacks, payouts, bountyValues, orbits, smallBlind, bigBlind[, ante, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> bounties;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "bountyValues", bounties, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const int orbits = info[3].As<Napi::Number>().Int32Value();
    const double sb = info[4].As<Napi::Number>().DoubleValue();
    const double bb = info[5].As<Napi::Number>().DoubleValue();
    double ante = 0.0;
    std::size_t fmt_i = 6;
    if (info.Length() >= 7 && info[6].IsNumber()) {
        ante = info[6].As<Napi::Number>().DoubleValue();
        fmt_i = 7;
    }
    const auto fmt = poker_bind::parse_return_format(info, fmt_i);
    POKER_TRY(env, {
        const auto r = poker::pko_fgs_payouts(stacks, payouts, bounties, orbits, sb, bb, ante);
        Napi::Object out = Napi::Object::New(env);
        out.Set("icm", write_f64_vector(env, r.icm, fmt));
        out.Set("bounty", write_f64_vector(env, r.bounty, fmt));
        out.Set("icmbu", write_f64_vector(env, r.icmbu, fmt));
        return out;
    });
}

Napi::Value LateRegOverlayEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(
        env, info.Length() >= 5,
        "lateRegOverlayEv(fieldRemaining, prizePool, lateRegFee, startingStack, averageStack)");
    const int field = info[0].As<Napi::Number>().Int32Value();
    const double pool = info[1].As<Napi::Number>().DoubleValue();
    const double fee = info[2].As<Napi::Number>().DoubleValue();
    const double start = info[3].As<Napi::Number>().DoubleValue();
    const double avg = info[4].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r = poker::late_reg_overlay_ev(field, pool, fee, start, avg);
        Napi::Object out = Napi::Object::New(env);
        out.Set("overlayRatio", Napi::Number::New(env, r.overlay_ratio));
        out.Set("registerEv", Napi::Number::New(env, r.register_ev));
        out.Set("icmShare", Napi::Number::New(env, r.icm_share));
        return out;
    });
}

Napi::Value WinnerTakeAllSatelliteEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "winnerTakeAllSatelliteEv(stacks, heroIndex, ticketCount, ticketValue)");
    std::string err;
    std::vector<double> stacks;
    if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[1].As<Napi::Number>().Uint32Value());
    const int tickets = info[2].As<Napi::Number>().Int32Value();
    const double value = info[3].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r = poker::winner_take_all_satellite_ev(stacks, hero, tickets, value);
        Napi::Object out = Napi::Object::New(env);
        out.Set("advanceProb", Napi::Number::New(env, r.advance_prob));
        out.Set("ticketEv", Napi::Number::New(env, r.ticket_ev));
        out.Set("chipEvIfDouble", Napi::Number::New(env, r.chip_ev_if_double));
        out.Set("dollarEvIfDouble", Napi::Number::New(env, r.dollar_ev_if_double));
        return out;
    });
}

Napi::Value SqueezeEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 9,
                  "squeezeEv(pot, heroPut, openerCall, callerCall, foldEquityOpener, "
                  "foldEquityCaller, equityVsOpener, equityVsCaller, equityVsBoth)");
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double hero_put = info[1].As<Napi::Number>().DoubleValue();
    const double opener_call = info[2].As<Napi::Number>().DoubleValue();
    const double caller_call = info[3].As<Napi::Number>().DoubleValue();
    const double fe_o = info[4].As<Napi::Number>().DoubleValue();
    const double fe_c = info[5].As<Napi::Number>().DoubleValue();
    const double eq_o = info[6].As<Napi::Number>().DoubleValue();
    const double eq_c = info[7].As<Napi::Number>().DoubleValue();
    const double eq_b = info[8].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        return spot_object(env, poker::squeeze_ev(pot, hero_put, opener_call, caller_call, fe_o, fe_c, eq_o, eq_c, eq_b),
                           "squeezeEv");
    });
}

Napi::Value FourBetJamEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5, "fourBetJamEv(deadPot, jam, call, foldEquity, equityWhenCalled)");
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double jam = info[1].As<Napi::Number>().DoubleValue();
    const double call = info[2].As<Napi::Number>().DoubleValue();
    const double fe = info[3].As<Napi::Number>().DoubleValue();
    const double eq = info[4].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, { return spot_object(env, poker::four_bet_jam_ev(pot, jam, call, fe, eq), "jamEv"); });
}

Napi::Value IsoRaiseVsLimpersEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "isoRaiseVsLimpersEv(pot, isoSize, limpCall, nLimpers, pFold[, equities])");
    std::string err;
    std::vector<double> equities;
    if (info.Length() >= 6 && !info[5].IsUndefined() && !info[5].IsNull()) {
        if (!read_f64_vector(info[5], "equities", equities, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
    }
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double iso = info[1].As<Napi::Number>().DoubleValue();
    const double limp = info[2].As<Napi::Number>().DoubleValue();
    const int n = info[3].As<Napi::Number>().Int32Value();
    const double p_fold = info[4].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r = poker::iso_raise_vs_limpers_ev(pot, iso, limp, n, p_fold, equities);
        Napi::Object out = Napi::Object::New(env);
        out.Set("isoEv", Napi::Number::New(env, r.iso_ev));
        out.Set("checkEv", Napi::Number::New(env, r.check_ev));
        out.Set("foldEv", Napi::Number::New(env, r.fold_ev));
        return out;
    });
}

Napi::Value ThreeBetPotCommitEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3,
                  "threeBetPotCommitEv(potAfterThreeBet, effectiveRemaining, equity[, realization])");
    const double pot = info[0].As<Napi::Number>().DoubleValue();
    const double remain = info[1].As<Napi::Number>().DoubleValue();
    const double eq = info[2].As<Napi::Number>().DoubleValue();
    const double real = info.Length() >= 4 && info[3].IsNumber() ? info[3].As<Napi::Number>().DoubleValue() : 1.0;
    POKER_TRY(env, {
        const auto r = poker::three_bet_pot_commit_ev(pot, remain, eq, real);
        Napi::Object out = Napi::Object::New(env);
        out.Set("spr", Napi::Number::New(env, r.spr));
        out.Set("stackOff", Napi::Boolean::New(env, r.stack_off));
        out.Set("continueEv", Napi::Number::New(env, r.continue_ev));
        out.Set("foldEv", Napi::Number::New(env, r.fold_ev));
        return out;
    });
}
