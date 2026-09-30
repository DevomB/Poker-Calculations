#include "binding_fgs.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"

#include "poker/fgs.hpp"

using poker_bind::read_f64_vector;
using poker_bind::write_f64_vector;

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
