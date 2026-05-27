#include "poker/state_codec.hpp"

#include <cstring>
#include <optional>

namespace poker {

namespace {

void append_u8(std::vector<std::uint8_t>& b, std::uint8_t v) { b.push_back(v); }

void append_i32(std::vector<std::uint8_t>& b, std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    b.push_back(static_cast<std::uint8_t>(u & 0xFF));
    b.push_back(static_cast<std::uint8_t>((u >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((u >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((u >> 24) & 0xFF));
}

bool read_u8(const std::uint8_t*& p, const std::uint8_t* end, std::uint8_t& out) {
    if (p >= end) {
        return false;
    }
    out = *p++;
    return true;
}

bool read_i32(const std::uint8_t*& p, const std::uint8_t* end, std::int32_t& out) {
    if (p + 4 > end) {
        return false;
    }
    std::uint32_t u = 0;
    u |= static_cast<std::uint32_t>(*p++);
    u |= static_cast<std::uint32_t>(*p++) << 8;
    u |= static_cast<std::uint32_t>(*p++) << 16;
    u |= static_cast<std::uint32_t>(*p++) << 24;
    out = static_cast<std::int32_t>(u);
    return true;
}

[[nodiscard]] std::uint8_t phase_to_u8(GamePhase ph) {
    return static_cast<std::uint8_t>(ph);
}

[[nodiscard]] std::optional<GamePhase> phase_from_u8(std::uint8_t v) {
    if (v <= static_cast<std::uint8_t>(GamePhase::HandComplete)) {
        return static_cast<GamePhase>(v);
    }
    return std::nullopt;
}

[[nodiscard]] std::uint8_t card_to_byte(const Card& c) {
    return static_cast<std::uint8_t>(static_cast<int>(c.rank()) * 4 + static_cast<int>(c.suit()));
}

}  // namespace

std::vector<std::uint8_t> encode_poker_state(const PokerGameState& state, std::string* err) {
    std::vector<std::uint8_t> b;
    b.reserve(128);
    for (char c : kStateCodecMagic) {
        append_u8(b, static_cast<std::uint8_t>(c));
    }
    append_u8(b, kStateCodecVersion);
    const std::size_t pn = state.players.size();
    if (pn > kStateCodecMaxPlayers) {
        if (err) {
            *err = "encodePokerState: too many players";
        }
        return {};
    }
    append_u8(b, static_cast<std::uint8_t>(pn));
    append_u8(b, phase_to_u8(state.phase));
    append_u8(b, 0);  // flags reserved

    append_i32(b, state.pot);
    append_i32(b, state.current_bet);
    append_i32(b, state.button_seat);
    append_i32(b, state.small_blind);
    append_i32(b, state.big_blind);
    append_i32(b, state.acting_index);
    append_i32(b, state.last_raise_increment);
    append_i32(b, state.street_opening_index);

    append_u8(b, static_cast<std::uint8_t>(state.community_cards.size()));
    for (const Card& c : state.community_cards) {
        append_u8(b, card_to_byte(c));
    }

    for (const Player& pl : state.players) {
        if (pl.hole_cards.size() != 2) {
            if (err) {
                *err = "encodePokerState: each player needs exactly 2 hole cards";
            }
            return {};
        }
        append_u8(b, card_to_byte(pl.hole_cards[0]));
        append_u8(b, card_to_byte(pl.hole_cards[1]));
        append_i32(b, pl.stack);
        append_i32(b, pl.committed_this_street);
        append_i32(b, pl.total_committed_hand);
        append_u8(b, pl.folded ? 1 : 0);
        append_u8(b, static_cast<std::uint8_t>(static_cast<int>(pl.seat) & 0xFF));
        const std::size_t name_len = pl.name.size() > 255 ? 255 : pl.name.size();
        append_u8(b, static_cast<std::uint8_t>(name_len));
        for (std::size_t i = 0; i < name_len; ++i) {
            append_u8(b, static_cast<std::uint8_t>(pl.name[i]));
        }
    }

    for (std::size_t i = 0; i < pn; ++i) {
        const bool acted = i < state.acted_this_street.size() && state.acted_this_street[i];
        append_u8(b, acted ? 1 : 0);
    }
    return b;
}

bool decode_poker_state(const std::uint8_t* data, std::size_t len, PokerGameState& out, std::string* err) {
    out = {};
    const std::uint8_t* p = data;
    const std::uint8_t* end = data + len;
    if (len < 8) {
        if (err) {
            *err = "decodePokerState: buffer too short";
        }
        return false;
    }
    if (std::memcmp(data, kStateCodecMagic, 4) != 0) {
        if (err) {
            *err = "decodePokerState: invalid magic";
        }
        return false;
    }
    p += 4;
    std::uint8_t ver = 0;
    if (!read_u8(p, end, ver) || ver != kStateCodecVersion) {
        if (err) {
            *err = "decodePokerState: unsupported version";
        }
        return false;
    }
    std::uint8_t pn = 0;
    std::uint8_t phase_u8 = 0;
    std::uint8_t flags = 0;
    if (!read_u8(p, end, pn) || !read_u8(p, end, phase_u8) || !read_u8(p, end, flags)) {
        if (err) {
            *err = "decodePokerState: truncated header";
        }
        return false;
    }
    (void)flags;
    const auto ph = phase_from_u8(phase_u8);
    if (!ph) {
        if (err) {
            *err = "decodePokerState: invalid phase";
        }
        return false;
    }
    out.phase = *ph;
    if (!read_i32(p, end, out.pot) || !read_i32(p, end, out.current_bet) || !read_i32(p, end, out.button_seat) ||
        !read_i32(p, end, out.small_blind) || !read_i32(p, end, out.big_blind) || !read_i32(p, end, out.acting_index) ||
        !read_i32(p, end, out.last_raise_increment) || !read_i32(p, end, out.street_opening_index)) {
        if (err) {
            *err = "decodePokerState: truncated table fields";
        }
        return false;
    }
    std::uint8_t board_n = 0;
    if (!read_u8(p, end, board_n)) {
        if (err) {
            *err = "decodePokerState: truncated board count";
        }
        return false;
    }
    out.community_cards.reserve(board_n);
    for (std::uint8_t i = 0; i < board_n; ++i) {
        std::uint8_t id = 0;
        if (!read_u8(p, end, id) || id > 51) {
            if (err) {
                *err = "decodePokerState: invalid board card";
            }
            return false;
        }
        out.community_cards.emplace_back(static_cast<std::uint8_t>(id / 4), static_cast<std::uint8_t>(id % 4));
    }
    out.players.resize(pn);
    for (std::uint8_t pi = 0; pi < pn; ++pi) {
        Player pl{};
        std::uint8_t h0 = 0;
        std::uint8_t h1 = 0;
        if (!read_u8(p, end, h0) || !read_u8(p, end, h1) || h0 > 51 || h1 > 51) {
            if (err) {
                *err = "decodePokerState: invalid hole cards";
            }
            return false;
        }
        pl.hole_cards.emplace_back(static_cast<std::uint8_t>(h0 / 4), static_cast<std::uint8_t>(h0 % 4));
        pl.hole_cards.emplace_back(static_cast<std::uint8_t>(h1 / 4), static_cast<std::uint8_t>(h1 % 4));
        if (!read_i32(p, end, pl.stack) || !read_i32(p, end, pl.committed_this_street) ||
            !read_i32(p, end, pl.total_committed_hand)) {
            if (err) {
                *err = "decodePokerState: truncated player stacks";
            }
            return false;
        }
        std::uint8_t folded = 0;
        std::uint8_t seat = 0;
        if (!read_u8(p, end, folded) || !read_u8(p, end, seat)) {
            if (err) {
                *err = "decodePokerState: truncated player flags";
            }
            return false;
        }
        pl.folded = folded != 0;
        pl.seat = static_cast<int>(seat);
        std::uint8_t name_len = 0;
        if (!read_u8(p, end, name_len)) {
            if (err) {
                *err = "decodePokerState: truncated player name";
            }
            return false;
        }
        pl.name.reserve(name_len);
        for (std::uint8_t ni = 0; ni < name_len; ++ni) {
            std::uint8_t ch = 0;
            if (!read_u8(p, end, ch)) {
                if (err) {
                    *err = "decodePokerState: truncated player name bytes";
                }
                return false;
            }
            pl.name.push_back(static_cast<char>(ch));
        }
        out.players[static_cast<std::size_t>(pi)] = std::move(pl);
    }
    out.acted_this_street.resize(pn);
    for (std::uint8_t i = 0; i < pn; ++i) {
        std::uint8_t acted = 0;
        if (!read_u8(p, end, acted)) {
            if (err) {
                *err = "decodePokerState: truncated actedThisStreet";
            }
            return false;
        }
        out.acted_this_street[static_cast<std::size_t>(i)] = acted != 0;
    }
    return true;
}

}  // namespace poker
