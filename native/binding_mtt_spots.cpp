#include "binding_mtt_spots.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/mtt_spots.hpp"

#include <string>

using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

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
