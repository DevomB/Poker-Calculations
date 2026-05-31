#pragma once

#include <cstddef>
#include <vector>

namespace poker {

struct IcmFieldPressureResult {
    double index{0.0};
    std::vector<double> pairwise_bubble_factors;
    std::size_t argmax_villain{0};
};

[[nodiscard]] std::vector<double> icm_harville_stack_jacobian(const std::vector<double>& stacks,
                                                              const std::vector<double>& payouts);

[[nodiscard]] IcmFieldPressureResult icm_field_pressure_index(const std::vector<double>& stacks,
                                                              const std::vector<double>& payouts,
                                                              std::size_t hero_index,
                                                              double pot_chips);

struct IcmChopParetoPair {
    std::size_t i{0};
    std::size_t j{0};
    double max_transfer{0.0};
};

struct IcmChopNegotiationResult {
    std::vector<double> chip_chop;
    std::vector<double> icm;
    std::vector<double> surplus;
    double total_prize_pool{0.0};
    std::vector<IcmChopParetoPair> pareto_pairs;
};

[[nodiscard]] IcmChopNegotiationResult icm_chop_negotiation_analysis(const std::vector<double>& stacks,
                                                                     const std::vector<double>& payouts);

}  // namespace poker
