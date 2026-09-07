#include "binding_fgs.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/fgs.hpp"

using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

namespace {

Napi::Object decision_object(Napi::Env env, const poker::IcmDecisionEv& r, const char* take_key) {
    Napi::Object out = Napi::Object::New(env);
    out.Set("foldEv", Napi::Number::New(env, r.fold_ev));
    out.Set(take_key, Napi::Number::New(env, r.take_ev));
    out.Set("delta", Napi::Number::New(env, r.delta));
    return out;
}

bool read_optional_unit_object_flag(const Napi::Value& v, const char* key, bool fallback) {
    if (!v.IsObject()) {
        return fallback;
    }
    const Napi::Object o = v.As<Napi::Object>();
    if (!o.Has(key)) {
        return fallback;
    }
    return o.Get(key).ToBoolean();
}

}  // namespace

Napi::Value FutureGameSimulationPayouts(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "futureGameSimulationPayouts(stacks, payouts, orbits, smallBlind, bigBlind[, ante, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const int orbits = info[2].As<Napi::Number>().Int32Value();
    const double sb = info[3].As<Napi::Number>().DoubleValue();
    const double bb = info[4].As<Napi::Number>().DoubleValue();
    double ante = 0.0;
    std::size_t fmt_i = 5;
    if (info.Length() >= 6 && info[5].IsNumber()) {
        ante = info[5].As<Napi::Number>().DoubleValue();
        fmt_i = 6;
    }
    const auto fmt = poker_bind::parse_return_format(info, fmt_i);
    POKER_TRY(env, {
        return write_f64_vector(env, poker::future_game_simulation_payouts(stacks, payouts, orbits, sb, bb, ante),
                                fmt);
    });
}

Napi::Value FutureGrowthShare(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4,
                  "futureGrowthShare(stacks, orbits, smallBlind, bigBlind[, ante])");
    std::string err;
    std::vector<double> stacks;
    if (!read_f64_vector(info[0], "stacks", stacks, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const int orbits = info[1].As<Napi::Number>().Int32Value();
    const double sb = info[2].As<Napi::Number>().DoubleValue();
    const double bb = info[3].As<Napi::Number>().DoubleValue();
    const double ante = info.Length() >= 5 && info[4].IsNumber() ? info[4].As<Napi::Number>().DoubleValue() : 0.0;
    POKER_TRY(env, {
        const auto r = poker::future_growth_share(stacks, orbits, sb, bb, ante);
        Napi::Object out = Napi::Object::New(env);
        out.Set("netGrowth", write_f64_vector(env, r.net_growth, poker_bind::F64ReturnFormat::Array));
        out.Set("growthShare", write_f64_vector(env, r.growth_share, poker_bind::F64ReturnFormat::Array));
        out.Set("survivorCount", Napi::Number::New(env, static_cast<double>(r.survivor_count)));
        return out;
    });
}

Napi::Value IcmPayoutsAfterBlindPost(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "icmPayoutsAfterBlindPost(stacks, payouts, heroIndex, heroPost, posts[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> posts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[4], "posts", posts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const double hero_post = info[3].As<Napi::Number>().DoubleValue();
    const auto fmt = poker_bind::parse_return_format(info, 5);
    POKER_TRY(env, {
        return write_f64_vector(env, poker::icm_payouts_after_blind_post(stacks, payouts, hero, hero_post, posts),
                                fmt);
    });
}

Napi::Value IcmJamVsFoldEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 7,
                  "icmJamVsFoldEv(stacks, payouts, heroIndex, villainIndex, pot, foldEquity, equityWhenCalled)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const std::size_t villain = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
    const double pot = info[4].As<Napi::Number>().DoubleValue();
    const double fe = info[5].As<Napi::Number>().DoubleValue();
    const double eq = info[6].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        return decision_object(env, poker::icm_jam_vs_fold_ev(stacks, payouts, hero, villain, pot, fe, eq), "jamEv");
    });
}

Napi::Value IcmCallVsFoldEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 7,
                  "icmCallVsFoldEv(stacks, payouts, heroIndex, villainIndex, pot, callAmount, heroEquity)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const std::size_t villain = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
    const double pot = info[4].As<Napi::Number>().DoubleValue();
    const double call = info[5].As<Napi::Number>().DoubleValue();
    const double eq = info[6].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        return decision_object(env, poker::icm_call_vs_fold_ev(stacks, payouts, hero, villain, pot, call, eq),
                               "callEv");
    });
}

Napi::Value IcmCallingBubbleFactor(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "icmCallingBubbleFactor(stacks, payouts, heroIndex, villainIndex, chipsAtRisk)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const std::size_t villain = static_cast<std::size_t>(info[3].As<Napi::Number>().Uint32Value());
    const double risk = info[4].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, { return Napi::Number::New(env, poker::icm_calling_bubble_factor(stacks, payouts, hero, villain, risk)); });
}

Napi::Value FgsPayoutsBlindSchedule(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 6,
                  "fgsPayoutsBlindSchedule(stacks, payouts, smallBlinds, bigBlinds, antes, orbitsAtLevel[, returnFormat])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    std::vector<double> sb;
    std::vector<double> bb;
    std::vector<double> ante;
    std::vector<double> orbits;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err) ||
        !read_f64_vector(info[2], "smallBlinds", sb, &err) || !read_f64_vector(info[3], "bigBlinds", bb, &err) ||
        !read_f64_vector(info[4], "antes", ante, &err) || !read_f64_vector(info[5], "orbitsAtLevel", orbits, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const auto fmt = poker_bind::parse_return_format(info, 6);
    POKER_TRY(env, {
        return write_f64_vector(env, poker::fgs_payouts_blind_schedule(stacks, payouts, sb, bb, ante, orbits), fmt);
    });
}

Napi::Value IcmStallingEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 5,
                  "icmStallingEv(stacks, payouts, heroIndex, smallBlind, bigBlind[, ante, options])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const double sb = info[3].As<Napi::Number>().DoubleValue();
    const double bb = info[4].As<Napi::Number>().DoubleValue();
    double ante = 0.0;
    bool model_collision = true;
    if (info.Length() >= 6 && info[5].IsNumber()) {
        ante = info[5].As<Napi::Number>().DoubleValue();
        if (info.Length() >= 7) {
            model_collision = read_optional_unit_object_flag(info[6], "modelCollision", true);
        }
    } else if (info.Length() >= 6 && info[5].IsObject()) {
        model_collision = read_optional_unit_object_flag(info[5], "modelCollision", true);
    }
    POKER_TRY(env, {
        const auto r = poker::icm_stalling_ev(stacks, payouts, hero, sb, bb, ante, model_collision);
        Napi::Object out = Napi::Object::New(env);
        out.Set("nowEv", Napi::Number::New(env, r.now_ev));
        out.Set("stallEv", Napi::Number::New(env, r.stall_ev));
        out.Set("stallingPremium", Napi::Number::New(env, r.stalling_premium));
        out.Set("collisionEv", Napi::Number::New(env, r.collision_ev));
        out.Set("collisionModeled", Napi::Boolean::New(env, r.collision_modeled));
        return out;
    });
}

Napi::Value IcmPayJumpSurvivalEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 3, "icmPayJumpSurvivalEv(stacks, payouts, heroIndex[, options])");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    std::string bust = "vanish";
    if (info.Length() >= 4 && info[3].IsObject()) {
        const Napi::Object o = info[3].As<Napi::Object>();
        if (o.Has("bustChips") && o.Get("bustChips").IsString()) {
            bust = o.Get("bustChips").As<Napi::String>().Utf8Value();
        }
    }
    POKER_TRY(env, {
        const auto r = poker::icm_pay_jump_survival_ev(stacks, payouts, hero, bust);
        Napi::Object out = Napi::Object::New(env);
        out.Set("nowEv", Napi::Number::New(env, r.now_ev));
        out.Set("afterBustEv", Napi::Number::New(env, r.after_bust_ev));
        out.Set("ladderDelta", Napi::Number::New(env, r.ladder_delta));
        out.Set("bustedIndex", Napi::Number::New(env, static_cast<double>(r.busted_index)));
        return out;
    });
}

Napi::Value IcmDeadPotDollarEv(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 4, "icmDeadPotDollarEv(stacks, payouts, heroIndex, deadChips)");
    std::string err;
    std::vector<double> stacks;
    std::vector<double> payouts;
    if (!read_f64_vector(info[0], "stacks", stacks, &err) || !read_f64_vector(info[1], "payouts", payouts, &err)) {
        POKER_FAIL_TYPE(env, err);
    }
    const std::size_t hero = static_cast<std::size_t>(info[2].As<Napi::Number>().Uint32Value());
    const double dead = info[3].As<Napi::Number>().DoubleValue();
    POKER_TRY(env, {
        const auto r = poker::icm_dead_pot_dollar_ev(stacks, payouts, hero, dead);
        Napi::Object out = Napi::Object::New(env);
        out.Set("nowEv", Napi::Number::New(env, r.now_ev));
        out.Set("winEv", Napi::Number::New(env, r.win_ev));
        out.Set("delta", Napi::Number::New(env, r.delta));
        return out;
    });
}
