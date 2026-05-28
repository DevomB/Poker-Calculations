#include "binding_batch.hpp"

#include "async_workers.hpp"
#include "binding_cards.hpp"
#include "binding_common.hpp"

#include "poker/exact_equity.hpp"
#include "poker/fast_evaluator.hpp"
#include "poker/monte_carlo.hpp"

#include <random>
#include <stdexcept>

namespace {

using poker_bind::parse_cards_from_js;

struct SimulateHandParsed {
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    int num_sim{0};
    std::uint32_t seed{0};
    int villains{1};
};

[[nodiscard]] bool parse_simulate_hand_args(const Napi::CallbackInfo& info, SimulateHandParsed& out,
                                            std::string* err) {
    if (info.Length() < 4 || !info[2].IsNumber() || !info[3].IsNumber()) {
        if (err) {
            *err = "simulateHandOutcome(holeCards: CardInput, board: CardInput, numSimulations, seed, villains?)";
        }
        return false;
    }
    const Napi::Env env = info.Env();
    out.hole = parse_cards_from_js(env, info[0], err);
    if (err && !err->empty()) {
        return false;
    }
    out.board = parse_cards_from_js(env, info[1], err);
    if (err && !err->empty()) {
        return false;
    }
    out.num_sim = info[2].As<Napi::Number>().Int32Value();
    out.seed = static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
    out.villains = 1;
    const int opts_idx = poker_bind::trailing_async_options_index(info);
    int villains_idx = -1;
    if (opts_idx >= 0) {
        if (opts_idx > 4 && info[opts_idx - 1].IsNumber()) {
            villains_idx = opts_idx - 1;
        }
    } else if (info.Length() > 4 && info[4].IsNumber()) {
        villains_idx = 4;
    }
    if (villains_idx >= 0) {
        out.villains = info[villains_idx].As<Napi::Number>().Int32Value();
    }
    return true;
}

struct ParallelSimParsed {
    std::vector<poker::Card> hole;
    std::vector<poker::Card> board;
    int num_sim{0};
    std::uint32_t base_seed{0};
    int villains{1};
    std::size_t num_threads{1};
};

[[nodiscard]] bool parse_parallel_sim_args(const Napi::CallbackInfo& info, ParallelSimParsed& out, std::string* err) {
    const int opts_idx = poker_bind::trailing_async_options_index(info);
    const int min_len = opts_idx >= 0 ? 7 : 6;
    if (info.Length() < min_len) {
        if (err) {
            *err = "parallelHandSimulation(hole: CardInput, board: CardInput, numSimulations, baseSeed, "
                   "villains, numThreads, options?)";
        }
        return false;
    }
    const Napi::Env env = info.Env();
    out.hole = parse_cards_from_js(env, info[0], err);
    if (err && !err->empty()) {
        return false;
    }
    out.board = parse_cards_from_js(env, info[1], err);
    if (err && !err->empty()) {
        return false;
    }
    out.num_sim = info[2].As<Napi::Number>().Int32Value();
    out.base_seed = static_cast<std::uint32_t>(info[3].As<Napi::Number>().Uint32Value());
    out.villains = info[4].As<Napi::Number>().Int32Value();
    const int threads_idx = opts_idx >= 0 ? 5 : 5;
    if (opts_idx >= 0 && opts_idx != 6) {
        if (err) {
            *err = "parallelHandSimulation(..., villains, numThreads, options?)";
        }
        return false;
    }
    if (!info[threads_idx].IsNumber()) {
        if (err) {
            *err = "parallelHandSimulation: numThreads must be a number";
        }
        return false;
    }
    out.num_threads = static_cast<std::size_t>(info[threads_idx].As<Napi::Number>().Uint32Value());
    return true;
}

[[nodiscard]] bool parse_sim_batch_specs(const Napi::Env& env, const Napi::Array& specs,
                                          std::vector<poker::SimSpot>& spots, std::string* err) {
    spots.clear();
    const uint32_t n = specs.Length();
    spots.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Napi::Value sv = specs[i];
        if (!sv.IsObject()) {
            if (err) {
                *err = "specs[" + std::to_string(i) + "]: must be an object";
            }
            return false;
        }
        const Napi::Object o = sv.As<Napi::Object>();
        poker::SimSpot spot{};
        if (!o.Has("holeCards") || !o.Has("board") || !o.Has("numSimulations") || !o.Has("seed")) {
            if (err) {
                *err = "specs[" + std::to_string(i) + "]: need holeCards, board, numSimulations, seed";
            }
            return false;
        }
        std::string cerr;
        spot.hole = parse_cards_from_js(env, o.Get("holeCards"), &cerr);
        if (!cerr.empty()) {
            if (err) {
                *err = "specs[" + std::to_string(i) + "].holeCards: " + cerr;
            }
            return false;
        }
        spot.board = parse_cards_from_js(env, o.Get("board"), &cerr);
        if (!cerr.empty()) {
            if (err) {
                *err = "specs[" + std::to_string(i) + "].board: " + cerr;
            }
            return false;
        }
        if (!o.Get("numSimulations").IsNumber() || !o.Get("seed").IsNumber()) {
            if (err) {
                *err = "specs[" + std::to_string(i) + "]: numSimulations and seed must be numbers";
            }
            return false;
        }
        spot.num_simulations = o.Get("numSimulations").As<Napi::Number>().Int32Value();
        spot.seed = static_cast<std::uint32_t>(o.Get("seed").As<Napi::Number>().Uint32Value());
        spot.villains = 1;
        if (o.Has("villains") && o.Get("villains").IsNumber()) {
            spot.villains = o.Get("villains").As<Napi::Number>().Int32Value();
        }
        spots.push_back(std::move(spot));
    }
    return true;
}

}  // namespace

bool parse_exact_hu_args(const Napi::CallbackInfo& info, ExactHuParsed& out, std::string* err) {
    if (poker_bind::effective_arg_length(info) < 2) {
        if (err) {
            *err = "exactHuEquityVsRandomHand(heroHoleCards: CardInput, boardCards: CardInput)";
        }
        return false;
    }
    const Napi::Env env = info.Env();
    out.hero = poker_bind::parse_cards_from_js(env, info[0], err);
    if (err && !err->empty()) {
        return false;
    }
    out.board = poker_bind::parse_cards_from_js(env, info[1], err);
    return err == nullptr || err->empty();
}

bool parse_straight_made_args(const Napi::CallbackInfo& info, StraightMadeParsed& out, std::string* err) {
    if (poker_bind::effective_arg_length(info) < 3 || !poker_bind::is_card_input(info[0]) ||
        !poker_bind::is_card_input(info[1]) || !poker_bind::is_card_input(info[2])) {
        if (err) {
            *err = "straightMadeFlopToRiverExactProbability(heroHoleCards: CardInput, flopThree: CardInput, "
                   "knownDead: CardInput)";
        }
        return false;
    }
    const Napi::Env env = info.Env();
    out.hero = poker_bind::parse_cards_from_js(env, info[0], err);
    if (err && !err->empty()) {
        return false;
    }
    out.flop = poker_bind::parse_cards_from_js(env, info[1], err);
    if (err && !err->empty()) {
        return false;
    }
    out.dead = poker_bind::parse_cards_from_js(env, info[2], err);
    return err == nullptr || err->empty();
}

std::size_t parse_benchmark_iterations(const Napi::CallbackInfo& info, std::string* err) {
    std::size_t iterations = 200000;
    const int opts_idx = poker_bind::trailing_async_options_index(info);
    if (info.Length() >= 1 && info[0].IsNumber()) {
        const double n = info[0].As<Napi::Number>().DoubleValue();
        if (n < 1.0) {
            if (err) {
                *err = "benchmarkEvaluatorThroughput(iterations): iterations must be >= 1";
            }
            return 0;
        }
        iterations = static_cast<std::size_t>(n);
    } else if (opts_idx == 0) {
        iterations = 200000;
    }
    return iterations;
}

Napi::Value SimulateHandOutcome(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            SimulateHandParsed args{};
        std::string err;
        if (!parse_simulate_hand_args(info, args, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        std::mt19937 rng(args.seed);
        const float eq = poker::simulate_hand_outcome(args.hole, args.board, args.num_sim, rng, args.villains);
        return Napi::Number::New(env, static_cast<double>(eq));

}

Napi::Value SimulateHandOutcomeAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            SimulateHandParsed args{};
        std::string err;
        if (!parse_simulate_hand_args(info, args, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        const auto hole = std::move(args.hole);
        const auto board = std::move(args.board);
        const int num_sim = args.num_sim;
        const std::uint32_t seed = args.seed;
        const int villains = args.villains;
        const Napi::Value signal = poker_bind::parse_async_signal(info);
        return poker_async::enqueue_float_work(
            env,
            [hole, board, num_sim, seed, villains](const poker::CancelPredicate* cancel) {
                std::mt19937 rng(seed);
                return static_cast<double>(
                    poker::simulate_hand_outcome(hole, board, num_sim, rng, villains, cancel));
            },
            signal);

}

Napi::Value ParallelHandSimulation(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            ParallelSimParsed args{};
        std::string err;
        if (!parse_parallel_sim_args(info, args, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        const float eq = poker::parallel_hand_simulation(args.hole, args.board, args.num_sim, args.base_seed,
                                                         args.villains, args.num_threads);
        return Napi::Number::New(env, static_cast<double>(eq));

}

Napi::Value ParallelHandSimulationAsync(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            ParallelSimParsed args{};
        std::string err;
        if (!parse_parallel_sim_args(info, args, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        const auto hole = std::move(args.hole);
        const auto board = std::move(args.board);
        const int num_sim = args.num_sim;
        const std::uint32_t base_seed = args.base_seed;
        const int villains = args.villains;
        const std::size_t num_threads = args.num_threads;
        const Napi::Value signal = poker_bind::parse_async_signal(info);
        return poker_async::enqueue_float_work(
            env,
            [hole, board, num_sim, base_seed, villains, num_threads](const poker::CancelPredicate* cancel) {
                return static_cast<double>(poker::parallel_hand_simulation(
                    hole, board, num_sim, base_seed, villains, num_threads, cancel));
            },
            signal);

}

Napi::Value SimulateHandOutcomeBatch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            if (info.Length() < 1 || !info[0].IsArray()) {
            POKER_FAIL_TYPE(env, "simulateHandOutcomeBatch(specs[], out?)");
        }
        std::vector<poker::SimSpot> spots;
        std::string err;
        if (!parse_sim_batch_specs(env, info[0].As<Napi::Array>(), spots, &err)) {
            POKER_FAIL_TYPE(env, err);
        }
        std::vector<float> equities;
        poker::simulate_hand_outcome_batch(spots, equities);
        const std::size_t n = equities.size();
        std::vector<double> out_d(n);
        for (std::size_t i = 0; i < n; ++i) {
            out_d[i] = static_cast<double>(equities[i]);
        }
        Napi::Value out_arg = info.Length() >= 2 ? info[1] : env.Undefined();
        if (!out_arg.IsUndefined()) {
            if (!poker_bind::write_f64_into_out(env, out_arg, out_d.data(), n, &err)) {
                POKER_FAIL_TYPE(env, err);
            }
        }
        return poker_bind::write_f64_vector(env, out_d, poker_bind::F64ReturnFormat::Float64);

}

Napi::Value SimulateHandOutcomeBatchPacked(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            if (info.Length() < 3) {
            POKER_FAIL_TYPE(env, 
                "simulateHandOutcomeBatchPacked(holes: Uint8Array, boards: Uint8Array, meta: Uint32Array, out?)");
        }
        const std::uint8_t* holes = nullptr;
        const std::uint8_t* boards = nullptr;
        std::size_t holes_len = 0;
        std::size_t boards_len = 0;
        std::string perr;
        if (!poker_bind::packed_card_bytes(info[0], &holes, &holes_len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "holes: Uint8Array" : perr);
        }
        if (!poker_bind::packed_card_bytes(info[1], &boards, &boards_len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "boards: Uint8Array" : perr);
        }
        if (!info[2].IsTypedArray() || info[2].As<Napi::TypedArray>().TypedArrayType() != napi_uint32_array) {
            POKER_FAIL_TYPE(env, "meta: Uint32Array (numSim, seed, villains per spot)");
        }
        const Napi::TypedArray meta_ta = info[2].As<Napi::TypedArray>();
        if (holes_len % 2 != 0 || boards_len % 5 != 0) {
            POKER_FAIL_TYPE(env, "holes length must be 2*n, boards length 5*n");
        }
        const std::size_t n = holes_len / 2;
        if (boards_len / 5 != n || meta_ta.ElementLength() != n * 3) {
            POKER_FAIL_TYPE(env, "holes, boards (5 each), and meta (3 uint32 per spot) length mismatch");
        }
        Napi::ArrayBuffer meta_ab = meta_ta.ArrayBuffer();
        const auto* m = reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::uint8_t*>(meta_ab.Data()) + meta_ta.ByteOffset());
        std::vector<poker::SimSpot> spots;
        spots.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            poker::SimSpot s{};
            std::string cerr;
            if (!poker::parse_packed_cards(holes + i * 2, 2, s.hole, &cerr)) {
                POKER_FAIL_TYPE(env, "spot " + std::to_string(i) + " holes: " + cerr);
            }
            if (!poker::parse_packed_cards(boards + i * 5, 5, s.board, &cerr)) {
                POKER_FAIL_TYPE(env, "spot " + std::to_string(i) + " board: " + cerr);
            }
            s.num_simulations = static_cast<int>(m[i * 3 + 0]);
            s.seed = m[i * 3 + 1];
            s.villains = static_cast<int>(m[i * 3 + 2]);
            spots.push_back(std::move(s));
        }
        std::vector<float> equities;
        poker::simulate_hand_outcome_batch(spots, equities);
        std::vector<double> out_d(n);
        for (std::size_t i = 0; i < n; ++i) {
            out_d[i] = static_cast<double>(equities[i]);
        }
        std::string werr;
        if (info.Length() >= 4 && !info[3].IsUndefined()) {
            if (!poker_bind::write_f64_into_out(env, info[3], out_d.data(), n, &werr)) {
                POKER_FAIL_TYPE(env, werr);
            }
        }
        return poker_bind::write_f64_vector(env, out_d, poker_bind::F64ReturnFormat::Float64);

}

Napi::Value EvaluateHandStrengthFastBatch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            if (info.Length() < 2) {
            POKER_FAIL_TYPE(env, 
                "evaluateHandStrengthFastBatch(holes: Uint8Array, boards: Uint8Array, boardCards?, out?)");
        }
        int board_cards = 5;
        std::size_t out_idx = 2;
        if (info.Length() >= 3 && info[2].IsNumber()) {
            board_cards = info[2].As<Napi::Number>().Int32Value();
            out_idx = 3;
        }
        if (board_cards < 0 || board_cards > 5) {
            POKER_FAIL_TYPE(env, "boardCards must be 0..5");
        }
        const std::uint8_t* holes = nullptr;
        const std::uint8_t* boards = nullptr;
        std::size_t holes_len = 0;
        std::size_t boards_len = 0;
        std::string perr;
        if (!poker_bind::packed_card_bytes(info[0], &holes, &holes_len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "holes: Uint8Array" : perr);
        }
        if (!poker_bind::packed_card_bytes(info[1], &boards, &boards_len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "boards: Uint8Array" : perr);
        }
        if (holes_len % 2 != 0) {
            POKER_FAIL_TYPE(env, "holes length must be 2*n");
        }
        const std::size_t n = holes_len / 2;
        if (boards_len != n * static_cast<std::size_t>(board_cards)) {
            POKER_FAIL_TYPE(env, "boards length must be n * boardCards");
        }
        std::vector<double> out_d(n);
        for (std::size_t i = 0; i < n; ++i) {
            std::vector<poker::Card> hole;
            std::vector<poker::Card> board;
            std::string cerr;
            if (!poker::parse_packed_cards(holes + i * 2, 2, hole, &cerr)) {
                POKER_FAIL_TYPE(env, "spot " + std::to_string(i) + ": " + cerr);
            }
            if (!poker::parse_packed_cards(boards + i * static_cast<std::size_t>(board_cards),
                                           static_cast<std::size_t>(board_cards), board, &cerr)) {
                POKER_FAIL_TYPE(env, "spot " + std::to_string(i) + " board: " + cerr);
            }
            out_d[i] = static_cast<double>(poker::evaluate_hand_strength_fast(hole, board));
        }
        std::string werr;
        if (info.Length() > static_cast<int>(out_idx) && !info[out_idx].IsUndefined()) {
            if (!poker_bind::write_f64_into_out(env, info[out_idx], out_d.data(), n, &werr)) {
                POKER_FAIL_TYPE(env, werr);
            }
        }
        return poker_bind::write_f64_vector(env, out_d, poker_bind::F64ReturnFormat::Float64);

}

Napi::Value ExactHuEquityVsRandomHandBatch(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
            if (info.Length() < 3) {
            POKER_FAIL_TYPE(env, 
                "exactHuEquityVsRandomHandBatch(holes: Uint8Array, boards: Uint8Array, boardCards, out?)");
        }
        const int board_cards = info[2].As<Napi::Number>().Int32Value();
        if (board_cards < 3 || board_cards > 5) {
            POKER_FAIL_TYPE(env, "boardCards must be 3..5");
        }
        const std::uint8_t* holes = nullptr;
        const std::uint8_t* boards = nullptr;
        std::size_t holes_len = 0;
        std::size_t boards_len = 0;
        std::string perr;
        if (!poker_bind::packed_card_bytes(info[0], &holes, &holes_len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "holes: Uint8Array" : perr);
        }
        if (!poker_bind::packed_card_bytes(info[1], &boards, &boards_len, &perr)) {
            POKER_FAIL_TYPE(env, perr.empty() ? "boards: Uint8Array" : perr);
        }
        const std::size_t n = holes_len / 2;
        if (holes_len % 2 != 0 || boards_len != n * static_cast<std::size_t>(board_cards)) {
            POKER_FAIL_TYPE(env, "holes and boards length mismatch");
        }
        std::vector<double> out_d(n);
        for (std::size_t i = 0; i < n; ++i) {
            std::vector<poker::Card> hole;
            std::vector<poker::Card> board;
            std::string cerr;
            if (!poker::parse_packed_cards(holes + i * 2, 2, hole, &cerr)) {
                POKER_FAIL_TYPE(env, "spot " + std::to_string(i) + ": " + cerr);
            }
            if (!poker::parse_packed_cards(boards + i * static_cast<std::size_t>(board_cards),
                                           static_cast<std::size_t>(board_cards), board, &cerr)) {
                POKER_FAIL_TYPE(env, "spot " + std::to_string(i) + " board: " + cerr);
            }
            out_d[i] = poker::exact_hu_equity_vs_random_hand(hole, board);
        }
        std::string werr;
        if (info.Length() >= 4 && !info[3].IsUndefined()) {
            if (!poker_bind::write_f64_into_out(env, info[3], out_d.data(), n, &werr)) {
                POKER_FAIL_TYPE(env, werr);
            }
        }
        return poker_bind::write_f64_vector(env, out_d, poker_bind::F64ReturnFormat::Float64);

}
