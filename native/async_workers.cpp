#include "async_workers.hpp"

#include "poker/strategy.hpp"

namespace poker_async {

namespace {

class FloatResultWorker : public Napi::AsyncWorker {
 public:
    FloatResultWorker(Napi::Promise::Deferred deferred, std::function<double()> run)
        : Napi::AsyncWorker(deferred.Env()), deferred_(deferred), run_(std::move(run)) {}

    void Execute() override { result_ = run_(); }

    void OnOK() override { deferred_.Resolve(Napi::Number::New(Env(), result_)); }

    void OnError(const Napi::Error& e) override { deferred_.Reject(e.Value()); }

 private:
    Napi::Promise::Deferred deferred_;
    std::function<double()> run_;
    double result_{0.0};
};

class DecideActionWorker : public Napi::AsyncWorker {
 public:
    DecideActionWorker(Napi::Promise::Deferred deferred, poker::PokerGameState state,
                       std::vector<poker::Card> hero_hole, poker::BotConfig cfg,
                       std::optional<poker::OpponentModel> opponent, int hero_seat)
        : Napi::AsyncWorker(deferred.Env()),
          deferred_(deferred),
          state_(std::move(state)),
          hero_hole_(std::move(hero_hole)),
          cfg_(cfg),
          opponent_(opponent),
          hero_seat_(hero_seat) {}

    void Execute() override {
        const poker::OpponentModel* opp_ptr = opponent_ ? &(*opponent_) : nullptr;
        decision_ = poker::decide_action(state_, hero_hole_, cfg_, opp_ptr, hero_seat_);
    }

    void OnOK() override {
        Napi::Object out = Napi::Object::New(Env());
        switch (decision_.action) {
            case poker::Action::Fold:
                out.Set("action", Napi::String::New(Env(), "fold"));
                break;
            case poker::Action::Call:
                out.Set("action", Napi::String::New(Env(), "call"));
                break;
            case poker::Action::Raise:
                out.Set("action", Napi::String::New(Env(), "raise"));
                break;
            case poker::Action::Check:
                out.Set("action", Napi::String::New(Env(), "check"));
                break;
            default:
                out.Set("action", Napi::String::New(Env(), "fold"));
                break;
        }
        out.Set("raiseBy", Napi::Number::New(Env(), decision_.raise_by));
        deferred_.Resolve(out);
    }

    void OnError(const Napi::Error& e) override { deferred_.Reject(e.Value()); }

 private:
    Napi::Promise::Deferred deferred_;
    poker::PokerGameState state_;
    std::vector<poker::Card> hero_hole_;
    poker::BotConfig cfg_;
    std::optional<poker::OpponentModel> opponent_;
    int hero_seat_{-1};
    poker::Decision decision_{};
};

class BenchmarkThroughputWorker : public Napi::AsyncWorker {
 public:
    explicit BenchmarkThroughputWorker(Napi::Promise::Deferred deferred, std::size_t iterations)
        : Napi::AsyncWorker(deferred.Env()), deferred_(deferred), iterations_(iterations) {}

    void Execute() override { bench_ = poker::benchmark_evaluator_throughput(iterations_); }

    void OnOK() override {
        Napi::Object out = Napi::Object::New(Env());
        out.Set("legacyEvalsPerSecond", Napi::Number::New(Env(), bench_.legacy_evals_per_second));
        out.Set("fastEvalsPerSecond", Napi::Number::New(Env(), bench_.fast_evals_per_second));
        out.Set("implementation", Napi::String::New(Env(), bench_.implementation));
        deferred_.Resolve(out);
    }

    void OnError(const Napi::Error& e) override { deferred_.Reject(e.Value()); }

 private:
    Napi::Promise::Deferred deferred_;
    std::size_t iterations_{200000};
    poker::EvaluatorBenchmarkResult bench_{};
};

}  // namespace

Napi::Promise enqueue_float_work(Napi::Env env, std::function<double()> run) {
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new FloatResultWorker(deferred, std::move(run));
    worker->Queue();
    return deferred.Promise();
}

Napi::Promise enqueue_decide_action(Napi::Env env, poker::PokerGameState state, std::vector<poker::Card> hero_hole,
                                    poker::BotConfig cfg, std::optional<poker::OpponentModel> opponent, int hero_seat) {
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker =
        new DecideActionWorker(deferred, std::move(state), std::move(hero_hole), cfg, opponent, hero_seat);
    worker->Queue();
    return deferred.Promise();
}

Napi::Promise enqueue_benchmark(Napi::Env env, std::size_t iterations) {
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new BenchmarkThroughputWorker(deferred, iterations);
    worker->Queue();
    return deferred.Promise();
}

}  // namespace poker_async
