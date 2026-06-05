#include "binding_init.hpp"

#include "poker/types.hpp"

#include <array>

namespace poker_bind {

namespace {

constexpr const char* kHandRankNames[] = {"highCard",      "onePair",       "twoPair",    "threeOfAKind",
                                          "straight",      "flush",         "fullHouse",  "fourOfAKind",
                                          "straightFlush", "royalFlush"};

}  // namespace

std::array<Napi::Reference<Napi::String>, 10> g_hand_rank_strings{};

void init_binding(Napi::Env env) {
    for (int i = 0; i < 10; ++i) {
        g_hand_rank_strings[static_cast<std::size_t>(i)] =
            Napi::Persistent(Napi::String::New(env, kHandRankNames[i]));
    }
}

Napi::String hand_rank_string_interned(Napi::Env env, poker::HandRank r) {
    const int idx = static_cast<int>(r);
    if (idx >= 0 && idx < 10) {
        return g_hand_rank_strings[static_cast<std::size_t>(idx)].Value();
    }
    return Napi::String::New(env, "unknown");
}

}  // namespace poker_bind
