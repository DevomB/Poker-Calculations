#include "poker/card_string.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace poker {

namespace {

[[nodiscard]] std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
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
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const int idx = static_cast<int>(data[i]);
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
    for (std::size_t i = 0; i < cards.size(); ++i) {
        for (std::size_t j = i + 1; j < cards.size(); ++j) {
            if (cards[i] == cards[j]) {
                return true;
            }
        }
    }
    return false;
}

bool parse_card_string(const std::string& raw, Card& out) {
    const std::string s = trim_copy(raw);
    if (s.empty()) {
        return false;
    }
    int rank = -1;
    std::size_t i = 0;
    if (s.size() >= 3 && s[0] == '1' && s[1] == '0') {
        rank = 8;  // Ten
        i = 2;
    } else if (s.size() >= 2) {
        const char r = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        static constexpr const char* kRanks = "23456789TJQKA";
        for (int k = 0; k < 13; ++k) {
            if (kRanks[k] == r) {
                rank = k;
                break;
            }
        }
        i = 1;
    }
    if (rank < 0 || i >= s.size()) {
        return false;
    }
    const char su = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    static constexpr const char* kSuits = "cdhs";
    int suit = -1;
    for (int k = 0; k < 4; ++k) {
        if (kSuits[k] == su) {
            suit = k;
            break;
        }
    }
    if (suit < 0) {
        return false;
    }
    try {
        out = Card(static_cast<std::uint8_t>(rank), static_cast<std::uint8_t>(suit));
    } catch (...) {
        return false;
    }
    return true;
}

bool card_strings_have_duplicate(const std::vector<std::string>& cards) {
    std::vector<Card> parsed;
    parsed.reserve(cards.size());
    for (std::size_t idx = 0; idx < cards.size(); ++idx) {
        Card c{};
        if (!parse_card_string(cards[idx], c)) {
            throw std::invalid_argument("invalid card string at index " + std::to_string(idx));
        }
        parsed.push_back(c);
    }
    return cards_have_duplicate(parsed);
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
    std::vector<Card> seen;
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
        const std::string piece = raw.substr(pos, len);
        Card c{};
        if (!parse_card_string(piece, c)) {
            throw std::invalid_argument("parseCompactCardList: invalid card at offset " + std::to_string(pos));
        }
        for (const Card& o : seen) {
            if (o == c) {
                throw std::invalid_argument("parseCompactCardList: duplicate card");
            }
        }
        seen.push_back(c);
        out.push_back(c.to_string());
        pos += len;
    }
    return out;
}

}  // namespace poker
