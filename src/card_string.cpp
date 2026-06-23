#include "poker/card_string.hpp"

#include <array>
#include <cctype>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace poker {

namespace {

constexpr std::uint8_t kLutInvalid = 255;

constexpr unsigned char ascii_toupper(unsigned char c) noexcept {
    if (c >= 'a' && c <= 'z') {
        return static_cast<unsigned char>(c - 'a' + 'A');
    }
    return c;
}

constexpr std::array<std::uint8_t, 256> make_rank_lut() {
    std::array<std::uint8_t, 256> lut{};
    lut.fill(kLutInvalid);
    std::uint8_t rank = 0;
    for (const char ch : std::string_view{"23456789TJQKA"}) {
        const unsigned char lo = static_cast<unsigned char>(ch);
        const unsigned char up = ascii_toupper(lo);
        lut[lo] = rank;
        lut[up] = rank;
        ++rank;
    }
    return lut;
}

constexpr std::array<std::uint8_t, 256> make_suit_lut() {
    std::array<std::uint8_t, 256> lut{};
    lut.fill(kLutInvalid);
    std::uint8_t suit = 0;
    for (const char ch : std::string_view{"cdhs"}) {
        const unsigned char lo = static_cast<unsigned char>(ch);
        const unsigned char up = ascii_toupper(lo);
        lut[lo] = suit;
        lut[up] = suit;
        ++suit;
    }
    return lut;
}

constexpr std::array<std::uint8_t, 256> kRankLut = make_rank_lut();
constexpr std::array<std::uint8_t, 256> kSuitLut = make_suit_lut();

static_assert(kRankLut.size() == 256);
static_assert(kSuitLut.size() == 256);

void trim_ascii_range(const char*& p, std::size_t& n) {
    while (n > 0 && std::isspace(static_cast<unsigned char>(*p))) {
        ++p;
        --n;
    }
    while (n > 0 && std::isspace(static_cast<unsigned char>(p[n - 1]))) {
        --n;
    }
}

struct CompactScanState {
    std::array<bool, 52> seen{};
};

void scan_compact_card_list_impl(const std::string& raw,
                               const std::function<void(const Card&, std::size_t token_offset)>& on_card) {
    CompactScanState st{};
    std::size_t pos = 0;
    const std::size_t n = raw.size();
    while (pos < n) {
        while (pos < n && std::isspace(static_cast<unsigned char>(raw[pos]))) {
            ++pos;
        }
        if (pos >= n) {
            break;
        }
        std::size_t len = 2;
        if (pos + 2 < n && raw[pos] == '1' && raw[pos + 1] == '0') {
            len = 3;
        }
        if (pos + len > n) {
            throw std::invalid_argument("parseCompactCardList: truncated card token");
        }
        const char* piece = raw.data() + pos;
        Card c{};
        if (!parse_card_string_unchecked(piece, len, c)) {
            throw std::invalid_argument("parseCompactCardList: invalid card at offset " + std::to_string(pos));
        }
        const int didx = deck_index_from_card(c);
        if (st.seen[static_cast<std::size_t>(didx)]) {
            throw std::invalid_argument("parseCompactCardList: duplicate card");
        }
        st.seen[static_cast<std::size_t>(didx)] = true;
        on_card(c, pos);
        pos += len;
    }
}

}  // namespace

int deck_index_from_card(const Card& c) {
    return static_cast<int>(c.rank()) * 4 + static_cast<int>(c.suit());
}

Card card_from_deck_index(int idx) {
    if (idx < 0 || idx > 51) {
        throw std::invalid_argument("deck index must be 0..51");
    }
    const int rank = idx / 4;
    const int suit = idx % 4;
    return Card(static_cast<std::uint8_t>(rank), static_cast<std::uint8_t>(suit));
}

bool parse_packed_cards(const std::uint8_t* data, const std::size_t n, std::vector<Card>& out,
                        std::string* err) {
    out.clear();
    if (data == nullptr && n != 0) {
        if (err) {
            *err = "packed cards data must not be null";
        }
        return false;
    }
    const std::span<const std::uint8_t> bytes{data, n};
    out.reserve(n);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const int idx = static_cast<int>(bytes[i]);
        if (idx < 0 || idx > 51) {
            if (err) {
                *err = "invalid deck index at byte " + std::to_string(i) + " (must be 0..51)";
            }
            out.clear();
            return false;
        }
        out.push_back(card_from_deck_index(idx));
    }
    return true;
}

bool cards_have_duplicate(const std::vector<Card>& cards) {
    std::array<bool, 52> seen{};
    for (const Card& c : cards) {
        const int idx = deck_index_from_card(c);
        if (idx < 0 || idx > 51) {
            continue;
        }
        if (seen[static_cast<std::size_t>(idx)]) {
            return true;
        }
        seen[static_cast<std::size_t>(idx)] = true;
    }
    return false;
}

bool parse_card_string_unchecked(const char* p, std::size_t n, Card& out) {
    trim_ascii_range(p, n);
    if (n < 2 || n > 3) {
        return false;
    }
    int rank = -1;
    std::size_t suit_pos = 0;
    if (n == 3) {
        if (p[0] != '1' || p[1] != '0') {
            return false;
        }
        rank = 8;
        suit_pos = 2;
    } else {
        const std::uint8_t r = kRankLut[static_cast<unsigned char>(p[0])];
        if (r == kLutInvalid) {
            return false;
        }
        rank = static_cast<int>(r);
        suit_pos = 1;
    }
    const std::uint8_t s = kSuitLut[static_cast<unsigned char>(p[suit_pos])];
    if (s == kLutInvalid) {
        return false;
    }
    out = Card(static_cast<std::uint8_t>(rank), s);
    return true;
}

bool parse_card_string(const std::string& raw, Card& out) {
    const char* p = raw.data();
    std::size_t n = raw.size();
    return parse_card_string_unchecked(p, n, out);
}

bool packed_cards_have_duplicate(const std::uint8_t* data, const std::size_t n, std::string* err) {
    if (data == nullptr && n != 0) {
        if (err) {
            *err = "packed cards data must not be null";
        }
        return false;
    }
    std::array<bool, 52> seen{};
    const std::span<const std::uint8_t> bytes{data, n};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const int idx = static_cast<int>(bytes[i]);
        if (idx < 0 || idx > 51) {
            if (err) {
                *err = "invalid deck index at byte " + std::to_string(i) + " (must be 0..51)";
            }
            return false;
        }
        if (seen[static_cast<std::size_t>(idx)]) {
            return true;
        }
        seen[static_cast<std::size_t>(idx)] = true;
    }
    return false;
}

bool card_strings_have_duplicate(const std::vector<std::string>& cards) {
    std::array<bool, 52> seen{};
    std::size_t idx = 0;
    for (const std::string& card : cards) {
        Card c{};
        const char* p = card.data();
        std::size_t n = card.size();
        if (!parse_card_string_unchecked(p, n, c)) {
            throw std::invalid_argument("invalid card string at index " + std::to_string(idx));
        }
        const int didx = deck_index_from_card(c);
        if (seen[static_cast<std::size_t>(didx)]) {
            return true;
        }
        seen[static_cast<std::size_t>(didx)] = true;
        ++idx;
    }
    return false;
}

std::string canonical_card_string(const std::string& raw) {
    Card c{};
    if (!parse_card_string(raw, c)) {
        throw std::invalid_argument("canonicalCardString: invalid card string");
    }
    return c.to_string();
}

std::vector<std::string> parse_compact_card_list(const std::string& raw) {
    std::vector<std::string> out;
    scan_compact_card_list_impl(raw, [&](const Card& c, std::size_t) { out.push_back(c.to_string()); });
    return out;
}

std::vector<std::uint8_t> parse_compact_card_list_indices(const std::string& raw) {
    std::vector<std::uint8_t> out;
    scan_compact_card_list_impl(raw, [&](const Card& c, std::size_t) {
        out.push_back(static_cast<std::uint8_t>(deck_index_from_card(c)));
    });
    return out;
}

}  // namespace poker
