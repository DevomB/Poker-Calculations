#pragma once

#include <napi.h>

#include "poker/types.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct ExactHuParsed {
    std::vector<poker::Card> hero;
    std::vector<poker::Card> board;
};

struct StraightMadeParsed {
    std::vector<poker::Card> hero;
    std::vector<poker::Card> flop;
    std::vector<poker::Card> dead;
};

[[nodiscard]] bool parse_exact_hu_args(const Napi::CallbackInfo& info, ExactHuParsed& out, std::string* err);

[[nodiscard]] bool parse_straight_made_args(const Napi::CallbackInfo& info, StraightMadeParsed& out,
                                            std::string* err);

[[nodiscard]] std::size_t parse_benchmark_iterations(const Napi::CallbackInfo& info, std::string* err);

Napi::Value SimulateHandOutcome(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeAsync(const Napi::CallbackInfo& info);
Napi::Value ParallelHandSimulation(const Napi::CallbackInfo& info);
Napi::Value ParallelHandSimulationAsync(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeBatch(const Napi::CallbackInfo& info);
Napi::Value SimulateHandOutcomeBatchPacked(const Napi::CallbackInfo& info);
Napi::Value EvaluateHandStrengthFastBatch(const Napi::CallbackInfo& info);
Napi::Value ExactHuEquityVsRandomHandBatch(const Napi::CallbackInfo& info);
