#include "binding_init.hpp"

#include "poker/types.hpp"

namespace poker_bind {

namespace {

constexpr const char* kHandRankNames[] = {"highCard",      "onePair",       "twoPair",    "threeOfAKind",
                                          "straight",      "flush",         "fullHouse",  "fourOfAKind",
                                          "straightFlush", "royalFlush"};

}  // namespace

void init_binding(Napi::Env env) {
    Napi::Array names = Napi::Array::New(env, 10);
    for (uint32_t i = 0; i < 10; ++i) {
        names.Set(i, Napi::String::New(env, kHandRankNames[i]));
    }
    // N-API 8 permits object references, not string references. Each environment owns its cache.
    env.SetInstanceData(new Napi::ObjectReference(Napi::Persistent(names.As<Napi::Object>())));
}

Napi::String hand_rank_string_interned(Napi::Env env, poker::HandRank r) {
    const int idx = static_cast<int>(r);
    if (idx >= 0 && idx < 10) {
        if (const auto* names = env.GetInstanceData<Napi::ObjectReference>()) {
            return names->Get(static_cast<uint32_t>(idx)).As<Napi::String>();
        }
        return Napi::String::New(env, kHandRankNames[idx]);
    }
    return Napi::String::New(env, "unknown");
}

}  // namespace poker_bind
