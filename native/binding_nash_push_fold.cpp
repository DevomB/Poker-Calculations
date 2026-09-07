#include "binding_nash_push_fold.hpp"

#include "binding_common.hpp"
#include "binding_numeric.hpp"
#include "poker/nash_push_fold.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

using poker_bind::read_f64_vector;

namespace {

Napi::Float64Array to_f64(Napi::Env env, const std::array<double, poker::kNashHandCount>& a) {
    Napi::ArrayBuffer buf = Napi::ArrayBuffer::New(env, poker::kNashHandCount * sizeof(double));
    std::memcpy(buf.Data(), a.data(), poker::kNashHandCount * sizeof(double));
    return Napi::Float64Array::New(env, poker::kNashHandCount, buf, 0);
}

double opt_num(const Napi::Object& o, const char* key, double def) {
    if (o.Has(key) && o.Get(key).IsNumber()) {
        return o.Get(key).As<Napi::Number>().DoubleValue();
    }
    return def;
}

int opt_int(const Napi::Object& o, const char* key, int def) {
    if (o.Has(key) && o.Get(key).IsNumber()) {
        return o.Get(key).As<Napi::Number>().Int32Value();
    }
    return def;
}

void apply_common_options(const Napi::Object& o, poker::NashPushFoldSpec& spec, std::string* err) {
    spec.small_blind = opt_num(o, "smallBlind", spec.small_blind);
    spec.big_blind = opt_num(o, "bigBlind", spec.big_blind);
    spec.ante = opt_num(o, "ante", spec.ante);
    spec.max_iterations = opt_int(o, "maxIterations", spec.max_iterations);
    spec.tolerance = opt_num(o, "tolerance", spec.tolerance);
    spec.equity_iterations = opt_int(o, "equityIterations", spec.equity_iterations);
    if (o.Has("equitySeed") && o.Get("equitySeed").IsNumber()) {
        spec.equity_seed = o.Get("equitySeed").As<Napi::Number>().Uint32Value();
    }
    const double stack_bb = opt_num(o, "stackBb", -1.0);
    if (stack_bb > 0.0) {
        spec.hero_stack = stack_bb * spec.big_blind;
        spec.villain_stack = stack_bb * spec.big_blind;
    }
    if (o.Has("heroStack") && o.Get("heroStack").IsNumber()) {
        spec.hero_stack = o.Get("heroStack").As<Napi::Number>().DoubleValue();
    }
    if (o.Has("villainStack") && o.Get("villainStack").IsNumber()) {
        spec.villain_stack = o.Get("villainStack").As<Napi::Number>().DoubleValue();
    }
    spec.hero_posted = spec.small_blind;
    spec.villain_posted = spec.big_blind;
    if (o.Has("otherStacks") && !o.Get("otherStacks").IsUndefined() && !o.Get("otherStacks").IsNull()) {
        if (!read_f64_vector(o.Get("otherStacks"), "otherStacks", spec.other_stacks, err)) {
            return;
        }
    }
    if (o.Has("payouts") && !o.Get("payouts").IsUndefined() && !o.Get("payouts").IsNull()) {
        if (!read_f64_vector(o.Get("payouts"), "payouts", spec.payouts, err)) {
            return;
        }
    }
}

bool parse_hu_spec(const Napi::CallbackInfo& info, poker::NashPushFoldSpec& spec, std::string* err) {
    spec = poker::NashPushFoldSpec{};
    if (info.Length() < 1) {
        if (err) {
            *err = "expected stackBb or options object";
        }
        return false;
    }
    if (info[0].IsNumber()) {
        const double stack_bb = info[0].As<Napi::Number>().DoubleValue();
        spec.small_blind = info.Length() >= 2 && info[1].IsNumber() ? info[1].As<Napi::Number>().DoubleValue() : 0.5;
        spec.big_blind = info.Length() >= 3 && info[2].IsNumber() ? info[2].As<Napi::Number>().DoubleValue() : 1.0;
        spec.ante = info.Length() >= 4 && info[3].IsNumber() ? info[3].As<Napi::Number>().DoubleValue() : 0.0;
        spec.hero_stack = stack_bb * spec.big_blind;
        spec.villain_stack = stack_bb * spec.big_blind;
        spec.hero_posted = spec.small_blind;
        spec.villain_posted = spec.big_blind;
        if (info.Length() >= 5 && info[4].IsArray()) {
            if (!read_f64_vector(info[4], "otherStacks", spec.other_stacks, err)) {
                return false;
            }
        }
        if (info.Length() >= 6 && (info[5].IsArray() || info[5].IsTypedArray())) {
            if (!read_f64_vector(info[5], "payouts", spec.payouts, err)) {
                return false;
            }
        }
        return true;
    }
    if (!info[0].IsObject()) {
        if (err) {
            *err = "expected stackBb number or options object";
        }
        return false;
    }
    apply_common_options(info[0].As<Napi::Object>(), spec, err);
    return err == nullptr || err->empty();
}

Napi::Object solve_to_js(Napi::Env env, const poker::NashJamCallResult& r) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("jam", to_f64(env, r.jam));
    o.Set("call", to_f64(env, r.call));
    o.Set("heroEv", Napi::Number::New(env, r.hero_ev));
    o.Set("villainEv", Napi::Number::New(env, r.villain_ev));
    o.Set("iterations", Napi::Number::New(env, r.iterations));
    return o;
}

int parse_hand_arg(const Napi::Value& v) {
    if (v.IsNumber()) {
        return v.As<Napi::Number>().Int32Value();
    }
    if (v.IsString()) {
        return poker::nash_hand169_from_notation(v.As<Napi::String>().Utf8Value());
    }
    throw std::invalid_argument("hand must be notation like \"AKo\" or a 0..168 index");
}

}  // namespace

Napi::Value NashHeadsUpJamCallSolve(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    if (!parse_hu_spec(info, spec, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "nashHeadsUpJamCallSolve(stackBb, sb?, bb?, ante?)" : err);
    }
    POKER_TRY(env, { return solve_to_js(env, poker::nash_heads_up_jam_call_solve(spec)); });
}

Napi::Value NashHeadsUpJamRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    if (!parse_hu_spec(info, spec, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "nashHeadsUpJamRange(stackBb, sb?, bb?, ante?)" : err);
    }
    POKER_TRY(env, { return to_f64(env, poker::nash_heads_up_jam_call_solve(spec).jam); });
}

Napi::Value NashHeadsUpCallRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    if (!parse_hu_spec(info, spec, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "nashHeadsUpCallRange(stackBb, sb?, bb?, ante?)" : err);
    }
    POKER_TRY(env, { return to_f64(env, poker::nash_heads_up_jam_call_solve(spec).call); });
}

Napi::Value NashBlindVsBlindSolve(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    if (!parse_hu_spec(info, spec, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "nashBlindVsBlindSolve(stackBb, sb?, bb?, ante?)" : err);
    }
    spec.hero_posted = spec.small_blind;
    spec.villain_posted = spec.big_blind;
    POKER_TRY(env, { return solve_to_js(env, poker::nash_heads_up_jam_call_solve(spec)); });
}

Napi::Value NashIcmHeadsUpJamCallSolve(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    if (!parse_hu_spec(info, spec, &err)) {
        POKER_FAIL_TYPE(env, err.empty()
                                ? "nashIcmHeadsUpJamCallSolve({ stackBb, otherStacks, payouts, ... })"
                                : err);
    }
    spec.use_icm = true;
    if (spec.payouts.empty()) {
        POKER_FAIL_TYPE(env, "nashIcmHeadsUpJamCallSolve requires payouts[]");
    }
    POKER_TRY(env, { return solve_to_js(env, poker::nash_heads_up_jam_call_solve(spec)); });
}

Napi::Value NashFirstInJamRange(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "nashFirstInJamRange({ stackBb, nOpponents, stacks, ... })");
    std::string err;
    poker::NashPushFoldSpec spec;
    std::vector<double> stacks;
    int n_opp = 0;
    if (info[0].IsObject()) {
        const Napi::Object o = info[0].As<Napi::Object>();
        apply_common_options(o, spec, &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
        n_opp = opt_int(o, "nOpponents", 0);
        if (o.Has("stacks")) {
            if (!read_f64_vector(o.Get("stacks"), "stacks", stacks, &err)) {
                POKER_FAIL_TYPE(env, err);
            }
        }
    } else if (info[0].IsNumber() && info.Length() >= 3) {
        n_opp = info[1].As<Napi::Number>().Int32Value();
        if (!read_f64_vector(info[2], "stacks", stacks, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        spec.small_blind = info.Length() >= 4 && info[3].IsNumber() ? info[3].As<Napi::Number>().DoubleValue() : 0.5;
        spec.big_blind = info.Length() >= 5 && info[4].IsNumber() ? info[4].As<Napi::Number>().DoubleValue() : 1.0;
        spec.ante = info.Length() >= 6 && info[5].IsNumber() ? info[5].As<Napi::Number>().DoubleValue() : 0.0;
        spec.hero_stack = info[0].As<Napi::Number>().DoubleValue() * spec.big_blind;
        spec.villain_stack = spec.hero_stack;
    } else {
        POKER_FAIL_TYPE(env, "nashFirstInJamRange({ stackBb, nOpponents, stacks })");
    }
    if (n_opp < 1) {
        n_opp = static_cast<int>(stacks.size());
    }
    if (n_opp < 1 || stacks.size() != static_cast<std::size_t>(n_opp)) {
        POKER_FAIL_TYPE(env, "nOpponents must match stacks[] length");
    }
    spec.hero_posted = 0.0;
    spec.villain_posted = 0.0;
    POKER_TRY(env, { return to_f64(env, poker::nash_multiway_shove_call(spec, stacks).jam); });
}

Napi::Value NashJamFoldChart169(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    double max_bb = poker::kNashDefaultMaxStackBb;
    if (info.Length() >= 1 && info[0].IsNumber()) {
        spec.big_blind = info[0].As<Napi::Number>().DoubleValue();
        spec.ante = info.Length() >= 2 && info[1].IsNumber() ? info[1].As<Napi::Number>().DoubleValue() : 0.0;
        if (info.Length() >= 3 && info[2].IsNumber()) {
            max_bb = info[2].As<Napi::Number>().DoubleValue();
        }
        spec.small_blind = spec.big_blind * 0.5;
    } else if (info.Length() >= 1 && info[0].IsObject()) {
        apply_common_options(info[0].As<Napi::Object>(), spec, &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
        max_bb = opt_num(info[0].As<Napi::Object>(), "stackBb", max_bb);
        max_bb = opt_num(info[0].As<Napi::Object>(), "maxStackBb", max_bb);
    } else {
        POKER_FAIL_TYPE(env, "nashJamFoldChart169(bigBlind, ante?, stackBb?)");
    }
    POKER_TRY(env, { return to_f64(env, poker::nash_jam_threshold_stack_bb(spec, max_bb)); });
}

Napi::Value NashCallChart169(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    std::string err;
    poker::NashPushFoldSpec spec;
    double max_bb = poker::kNashDefaultMaxStackBb;
    if (info.Length() >= 1 && info[0].IsNumber()) {
        spec.big_blind = info[0].As<Napi::Number>().DoubleValue();
        spec.ante = info.Length() >= 2 && info[1].IsNumber() ? info[1].As<Napi::Number>().DoubleValue() : 0.0;
        if (info.Length() >= 3 && info[2].IsNumber()) {
            max_bb = info[2].As<Napi::Number>().DoubleValue();
        }
        spec.small_blind = spec.big_blind * 0.5;
    } else if (info.Length() >= 1 && info[0].IsObject()) {
        apply_common_options(info[0].As<Napi::Object>(), spec, &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
        max_bb = opt_num(info[0].As<Napi::Object>(), "stackBb", max_bb);
        max_bb = opt_num(info[0].As<Napi::Object>(), "maxStackBb", max_bb);
    } else {
        POKER_FAIL_TYPE(env, "nashCallChart169(bigBlind, ante?, stackBb?)");
    }
    POKER_TRY(env, { return to_f64(env, poker::nash_call_threshold_stack_bb(spec, max_bb)); });
}

Napi::Value NashIndifferenceStackBb(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1, "nashIndifferenceStackBb(hand, stackBb|options?)");
    std::string err;
    poker::NashPushFoldSpec spec;
    double max_bb = poker::kNashDefaultMaxStackBb;
    if (info.Length() >= 2 && info[1].IsObject()) {
        apply_common_options(info[1].As<Napi::Object>(), spec, &err);
        if (!err.empty()) {
            POKER_FAIL_TYPE(env, err);
        }
        max_bb = opt_num(info[1].As<Napi::Object>(), "maxStackBb", max_bb);
        max_bb = opt_num(info[1].As<Napi::Object>(), "stackBb", max_bb);
    } else if (info.Length() >= 2 && info[1].IsNumber()) {
        spec.small_blind = info.Length() >= 3 && info[2].IsNumber() ? info[2].As<Napi::Number>().DoubleValue() : 0.5;
        spec.big_blind = info.Length() >= 4 && info[3].IsNumber() ? info[3].As<Napi::Number>().DoubleValue() : 1.0;
        spec.ante = info.Length() >= 5 && info[4].IsNumber() ? info[4].As<Napi::Number>().DoubleValue() : 0.0;
        max_bb = info[1].As<Napi::Number>().DoubleValue();
    }
    POKER_TRY(env, {
        const int hand = parse_hand_arg(info[0]);
        return Napi::Number::New(env, poker::nash_indifference_stack_bb(hand, spec, max_bb));
    });
}

Napi::Value NashMultiwayShoveCall(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    POKER_REQUIRE(env, info.Length() >= 1 && info[0].IsObject(),
                  "nashMultiwayShoveCall({ shoverStack|stackBb, callerStacks, ... })");
    std::string err;
    poker::NashPushFoldSpec spec;
    const Napi::Object o = info[0].As<Napi::Object>();
    apply_common_options(o, spec, &err);
    if (!err.empty()) {
        POKER_FAIL_TYPE(env, err);
    }
    if (o.Has("shoverStack") && o.Get("shoverStack").IsNumber()) {
        spec.hero_stack = o.Get("shoverStack").As<Napi::Number>().DoubleValue();
    }
    spec.hero_posted = 0.0;
    spec.villain_posted = 0.0;
    spec.use_icm = !spec.payouts.empty();
    std::vector<double> callers;
    if (!o.Has("callerStacks") || !read_f64_vector(o.Get("callerStacks"), "callerStacks", callers, &err)) {
        POKER_FAIL_TYPE(env, err.empty() ? "callerStacks[] required" : err);
    }
    POKER_TRY(env, {
        const auto r = poker::nash_multiway_shove_call(spec, callers);
        Napi::Object out = Napi::Object::New(env);
        out.Set("jam", to_f64(env, r.jam));
        Napi::Array calls = Napi::Array::New(env, r.calls.size());
        for (std::size_t i = 0; i < r.calls.size(); ++i) {
            calls.Set(static_cast<std::uint32_t>(i), to_f64(env, r.calls[i]));
        }
        out.Set("calls", calls);
        out.Set("iterations", Napi::Number::New(env, r.iterations));
        return out;
    });
}
