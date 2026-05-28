#include "async_workers.hpp"

#include "poker/strategy.hpp"

#include <utility>

namespace poker_async {

namespace {

Napi::Value MakeAbortError(Napi::Env env) {
    const Napi::Value dom_ctor = env.Global().Get("DOMException");
    if (dom_ctor.IsFunction()) {
        return dom_ctor.As<Napi::Function>().New(
            {Napi::String::New(env, "The operation was aborted"), Napi::String::New(env, "AbortError")});
    }
    Napi::Error err = Napi::Error::New(env, "Aborted");
    err.Set("code", Napi::String::New(env, "ABORT_ERR"));
    return err.Value();
}

class CancellableWorker : public Napi::AsyncWorker {
 public:
    explicit CancellableWorker(Napi::Promise::Deferred deferred)
        : Napi::AsyncWorker(deferred.Env()), deferred_(std::move(deferred)) {}

    ~CancellableWorker() override { CleanupSignal(); }

    [[nodiscard]] bool PrepareSignal(Napi::Value signal) {
        const Napi::Env env = Env();
        if (signal.IsUndefined() || signal.IsNull() || !signal.IsObject()) {
            return true;
        }
        const Napi::Object sig = signal.As<Napi::Object>();
        if (!sig.Has("aborted")) {
            return true;
        }
        if (sig.Get("aborted").ToBoolean()) {
            deferred_.Reject(MakeAbortError(env));
            return false;
        }
        signal_ref_ = Napi::ObjectReference::New(sig, 1);
        Napi::Function listener =
            Napi::Function::New(env, OnAbortCallback, "onAbort", static_cast<void*>(this));
        listener_ref_ = Napi::FunctionReference::New(listener, 1);
        sig.Get("addEventListener")
            .As<Napi::Function>()
            .Call(sig, {Napi::String::New(env, "abort"), listener});
        return true;
    }

 protected:
    [[nodiscard]] poker::CancelPredicate MakeCancelCheck() {
        return [this]() {
            if (IsCancelled()) {
                cancelled_flag_.store(true, std::memory_order_relaxed);
            }
            return cancelled_flag_.load(std::memory_order_relaxed);
        };
    }

    template <typename Fn>
    bool RunWithCancel(Fn&& fn) {
        if (IsCancelled()) {
            SetError("Aborted");
            return true;
        }
        poker::CancelPredicate check = MakeCancelCheck();
        try {
            fn(&check);
        } catch (const poker::operation_cancelled&) {
            SetError("Aborted");
            return true;
        }
        if (check() || IsCancelled()) {
            SetError("Aborted");
            return true;
        }
        return false;
    }

    void FinishSuccess() {
        CleanupSignal();
    }

    void FinishError(const Napi::Error& e) {
        CleanupSignal();
        if (e.Message() == "Aborted") {
            deferred_.Reject(MakeAbortError(Env()));
            return;
        }
        deferred_.Reject(e.Value());
    }

    Napi::Promise::Deferred deferred_;

 private:
    static void OnAbortCallback(const Napi::CallbackInfo& info) {
        auto* self = static_cast<CancellableWorker*>(info.Data());
        if (self != nullptr) {
            self->cancelled_flag_.store(true, std::memory_order_relaxed);
            self->Cancel();
        }
    }

    void CleanupSignal() {
        if (signal_ref_.IsEmpty()) {
            return;
        }
        Napi::Env env = Env();
        if (env != nullptr && listener_ref_) {
            Napi::Object signal = signal_ref_.Value();
            if (signal.Has("removeEventListener")) {
                signal.Get("removeEventListener")
                    .As<Napi::Function>()
                    .Call(signal, {Napi::String::New(env, "abort"), listener_ref_.Value()});
            }
        }
        listener_ref_.Reset();
        signal_ref_.Reset();
    }

    std::atomic<bool> cancelled_flag_{false};
    Napi::ObjectReference signal_ref_;
    Napi::FunctionReference listener_ref_;
};

class FloatResultWorker : public CancellableWorker {
 public:
    FloatResultWorker(Napi::Promise::Deferred deferred, FloatWorkFn run)
        : CancellableWorker(std::move(deferred)), run_(std::move(run)) {}

    void Execute() override {
        if (RunWithCancel([&](const poker::CancelPredicate* cancel) { result_ = run_(cancel); })) {
            return;
        }
    }

    void OnOK() override {
        FinishSuccess();
        deferred_.Resolve(Napi::Number::New(Env(), result_));
    }

    void OnError(const Napi::Error& e) override { FinishError(e); }

 private:
    FloatWorkFn run_;
    double result_{0.0};
};

class DecideActionWorker : public CancellableWorker {
 public:
    DecideActionWorker(Napi::Promise::Deferred deferred, poker::PokerGameState state,
                       std::vector<poker::Card> hero_hole, poker::BotConfig cfg,
                       std::optional<poker::OpponentModel> opponent, int hero_seat)
        : CancellableWorker(std::move(deferred)),
          state_(std::move(state)),
          hero_hole_(std::move(hero_hole)),
          cfg_(cfg),
          opponent_(opponent),
          hero_seat_(hero_seat) {}

    void Execute() override {
        if (RunWithCancel([&](const poker::CancelPredicate* cancel) {
                const poker::OpponentModel* opp_ptr = opponent_ ? &(*opponent_) : nullptr;
                decision_ =
                    poker::decide_action(state_, hero_hole_, cfg_, opp_ptr, hero_seat_, cancel);
            })) {
            return;
        }
    }

    void OnOK() override {
        FinishSuccess();
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

    void OnError(const Napi::Error& e) override { FinishError(e); }

 private:
    poker::PokerGameState state_;
    std::vector<poker::Card> hero_hole_;
    poker::BotConfig cfg_;
    std::optional<poker::OpponentModel> opponent_;
    int hero_seat_{-1};
    poker::Decision decision_{};
};

class BenchmarkThroughputWorker : public CancellableWorker {
 public:
    BenchmarkThroughputWorker(Napi::Promise::Deferred deferred, std::size_t iterations)
        : CancellableWorker(std::move(deferred)), iterations_(iterations) {}

    void Execute() override {
        if (RunWithCancel(
                [&](const poker::CancelPredicate* cancel) {
                    bench_ = poker::benchmark_evaluator_throughput(iterations_, cancel);
                })) {
            return;
        }
    }

    void OnOK() override {
        FinishSuccess();
        Napi::Object out = Napi::Object::New(Env());
        out.Set("legacyEvalsPerSecond", Napi::Number::New(Env(), bench_.legacy_evals_per_second));
        out.Set("fastEvalsPerSecond", Napi::Number::New(Env(), bench_.fast_evals_per_second));
        out.Set("implementation", Napi::String::New(Env(), bench_.implementation));
        deferred_.Resolve(out);
    }

    void OnError(const Napi::Error& e) override { FinishError(e); }

 private:
    std::size_t iterations_{200000};
    poker::EvaluatorBenchmarkResult bench_{};
};

}  // namespace

Napi::Promise enqueue_float_work(Napi::Env env, FloatWorkFn run, Napi::Value signal) {
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new FloatResultWorker(deferred, std::move(run));
    if (!worker->PrepareSignal(signal)) {
        delete worker;
        return deferred.Promise();
    }
    worker->Queue();
    return deferred.Promise();
}

Napi::Promise enqueue_decide_action(Napi::Env env, poker::PokerGameState state,
                                    std::vector<poker::Card> hero_hole, poker::BotConfig cfg,
                                    std::optional<poker::OpponentModel> opponent, int hero_seat,
                                    Napi::Value signal) {
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new DecideActionWorker(deferred, std::move(state), std::move(hero_hole), cfg,
                                          opponent, hero_seat);
    if (!worker->PrepareSignal(signal)) {
        delete worker;
        return deferred.Promise();
    }
    worker->Queue();
    return deferred.Promise();
}

Napi::Promise enqueue_benchmark(Napi::Env env, std::size_t iterations, Napi::Value signal) {
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new BenchmarkThroughputWorker(deferred, iterations);
    if (!worker->PrepareSignal(signal)) {
        delete worker;
        return deferred.Promise();
    }
    worker->Queue();
    return deferred.Promise();
}

}  // namespace poker_async
