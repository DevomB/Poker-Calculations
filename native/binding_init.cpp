#include "binding_init.hpp"

#include "poker/types.hpp"

namespace poker_bind {

namespace {

constexpr const char* kHandRankNames[] = {"highCard",      "onePair",       "twoPair",    "threeOfAKind",
                                          "straight",      "flush",         "fullHouse",  "fourOfAKind",
                                          "straightFlush", "royalFlush"};

}  // namespace

void init_binding(Napi::Env env) {
    (void)env;
}

Napi::String hand_rank_string_interned(Napi::Env env, poker::HandRank r) {
    const int idx = static_cast<int>(r);
    if (idx >= 0 && idx < 10) {
        return Napi::String::New(env, kHandRankNames[idx]);
    }
    return Napi::String::New(env, "unknown");
}

}  // namespace poker_bind
