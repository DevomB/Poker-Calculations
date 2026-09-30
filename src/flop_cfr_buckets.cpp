#include "poker/flop_cfr_buckets.hpp"

#include "poker/card_string.hpp"
#include "poker/deck_bitset.hpp"
#include "poker/suit_isomorphism.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace poker {

namespace {

constexpr int kComboCount = 1326;

[[nodiscard]] int combo_index_1326(int a, int b) {
    if (a > b) {
        std::swap(a, b);
    }
    if (a < 0 || b > 51 || a == b) {
        return -1;
    }
    return a * 51 - (a * (a - 1)) / 2 + (b - a - 1);
}

void decode_combo_1326(int idx, int& a, int& b) {
    if (idx < 0 || idx >= kComboCount) {
        a = 0;
        b = 1;
        return;
    }
    int rem = idx;
    for (a = 0; a < 52; ++a) {
        const int n = 51 - a;
        if (rem < n) {
            b = a + 1 + rem;
            return;
        }
        rem -= n;
    }
    a = 50;
    b = 51;
}

[[nodiscard]] double clamp01(double x) {
    if (!std::isfinite(x)) {
        return 0.0;
    }
    return std::min(1.0, std::max(0.0, x));
}

void require_finite_nonneg(double x, const char* name) {
    if (!std::isfinite(x) || x < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and ≥ 0");
    }
}

[[nodiscard]] int require_bucket_count(int bucket_count) {
    if (bucket_count < 1 || bucket_count > kComboCount) {
        throw std::invalid_argument("bucket count must be in [1, 1326]");
    }
    return bucket_count;
}

[[nodiscard]] std::vector<double> dense_from_sparse(const SparseRange& range) {
    std::vector<double> out(static_cast<std::size_t>(kComboCount), 0.0);
    for (const WeightedHoleCombo& c : range.combos) {
        const int idx = combo_index_1326(c.card_a, c.card_b);
        if (idx >= 0 && std::isfinite(c.weight) && c.weight > 0.0) {
            out[static_cast<std::size_t>(idx)] += c.weight;
        }
    }
    return out;
}

[[nodiscard]] std::vector<int> buckets_from_ehs2(const std::vector<double>& ehs2,
                                                 const std::vector<Card>& board, int bucket_count) {
    if (ehs2.size() != static_cast<std::size_t>(kComboCount)) {
        throw std::invalid_argument("EHS2 table must have length 1326");
    }
    DeckBitset dead;
    dead.mark_cards(board);
    std::vector<int> out(static_cast<std::size_t>(kComboCount), -1);
    for (int i = 0; i < kComboCount; ++i) {
        int a = 0;
        int b = 1;
        decode_combo_1326(i, a, b);
        if (dead.test(a) || dead.test(b)) {
            continue;
        }
        out[static_cast<std::size_t>(i)] = equity_bucket_from_ehs(ehs2[static_cast<std::size_t>(i)], bucket_count);
    }
    return out;
}

[[nodiscard]] long long milli_chips(double x, const char* name) {
    require_finite_nonneg(x, name);
    return std::llround(x * 1000.0);
}

}  // namespace

int flop_bucket_count_default() { return kDefaultFlopBucketCount; }

std::vector<int> ehs2_buckets_vs_range(const std::vector<Card>& board_cards, const SparseRange& villain_range,
                                       int bucket_count, const ComboEhsTableOptions& options) {
    (void)require_bucket_count(bucket_count);
    if (board_cards.size() != 3 && board_cards.size() != 4) {
        throw std::invalid_argument("ehs2BucketsVsRange requires a flop or turn (3 or 4 board cards)");
    }
    if (cards_have_duplicate(board_cards)) {
        throw std::invalid_argument("duplicate cards on board");
    }
    const std::vector<double> ehs2 = combo_ehs2_table_vs_range(board_cards, villain_range, options);
    return buckets_from_ehs2(ehs2, board_cards, bucket_count);
}

std::vector<double> bucket_mass_from_range(const std::vector<double>& range_1326,
                                           const std::vector<int>& combo_buckets, int bucket_count) {
    const int k = require_bucket_count(bucket_count);
    if (range_1326.size() != static_cast<std::size_t>(kComboCount) ||
        combo_buckets.size() != static_cast<std::size_t>(kComboCount)) {
        throw std::invalid_argument("range and buckets must have length 1326");
    }
    std::vector<double> mass(static_cast<std::size_t>(k), 0.0);
    double z = 0.0;
    for (int i = 0; i < kComboCount; ++i) {
        const int b = combo_buckets[static_cast<std::size_t>(i)];
        const double w = range_1326[static_cast<std::size_t>(i)];
        if (!std::isfinite(w) || w <= 0.0 || b < 0 || b >= k) {
            continue;
        }
        mass[static_cast<std::size_t>(b)] += w;
        z += w;
    }
    if (z <= 0.0) {
        throw std::invalid_argument("range has no mass in any live bucket");
    }
    for (double& x : mass) {
        x /= z;
    }
    return mass;
}

std::vector<double> bucket_mass_from_range(const SparseRange& range, const std::vector<int>& combo_buckets,
                                           int bucket_count) {
    return bucket_mass_from_range(dense_from_sparse(range), combo_buckets, bucket_count);
}

std::vector<double> flop_bucket_strategy_to_1326(const std::vector<double>& bucket_mix,
                                                 const std::vector<int>& combo_buckets) {
    if (bucket_mix.empty()) {
        throw std::invalid_argument("bucket mix must be non-empty");
    }
    if (combo_buckets.size() != static_cast<std::size_t>(kComboCount)) {
        throw std::invalid_argument("combo buckets must have length 1326");
    }
    const int k = static_cast<int>(bucket_mix.size());
    std::vector<double> out(static_cast<std::size_t>(kComboCount), 0.0);
    for (int i = 0; i < kComboCount; ++i) {
        const int b = combo_buckets[static_cast<std::size_t>(i)];
        if (b < 0 || b >= k) {
            continue;
        }
        out[static_cast<std::size_t>(i)] = clamp01(bucket_mix[static_cast<std::size_t>(b)]);
    }
    return out;
}

std::string canonical_flop_cfr_key(const std::vector<Card>& flop, double pot, double stack) {
    if (flop.size() != 3) {
        throw std::invalid_argument("canonicalFlopCfrKey requires a 3-card flop");
    }
    if (cards_have_duplicate(flop)) {
        throw std::invalid_argument("duplicate cards on flop");
    }
    const int idx = isomorphic_flop_index(flop);
    std::ostringstream oss;
    oss << "isoFlop=" << idx << "/" << count_canonical_flops() << ";pot=" << milli_chips(pot, "pot")
        << ";stack=" << milli_chips(stack, "stack");
    return oss.str();
}

}  // namespace poker
