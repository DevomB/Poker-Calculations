#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace poker {

struct ShapleyIcmResult {
    std::vector<double> values;
    std::string method;
    std::vector<double> se;
};

[[nodiscard]] ShapleyIcmResult icm_shapley_values(const std::vector<double>& stacks,
                                                  const std::vector<double>& payouts,
                                                  const char* method = "exact",
                                                  std::size_t permutations = 200000);

}  // namespace poker
