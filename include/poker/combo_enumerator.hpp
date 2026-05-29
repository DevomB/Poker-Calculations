#pragma once

#include <cstddef>
#include <vector>

namespace poker {

/// In-place k-combination over `pool` indices; `fn` receives `const int* combo, int k`.
template <typename Fn>
void for_each_combo_indices(const std::vector<int>& pool, int k, Fn&& fn) {
    if (k <= 0 || static_cast<int>(pool.size()) < k) {
        return;
    }
    std::vector<int> idx(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i) {
        idx[static_cast<std::size_t>(i)] = i;
    }
    const int n = static_cast<int>(pool.size());
    for (;;) {
        int combo[16]{};
        for (int i = 0; i < k; ++i) {
            combo[i] = pool[static_cast<std::size_t>(idx[static_cast<std::size_t>(i)])];
        }
        fn(combo, k);
        int t = k - 1;
        while (t >= 0 && idx[static_cast<std::size_t>(t)] == n - k + t) {
            --t;
        }
        if (t < 0) {
            break;
        }
        ++idx[static_cast<std::size_t>(t)];
        for (int j = t + 1; j < k; ++j) {
            idx[static_cast<std::size_t>(j)] = idx[static_cast<std::size_t>(j - 1)] + 1;
        }
    }
}

}  // namespace poker
