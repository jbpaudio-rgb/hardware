// The parts rendered in parallel. The audio thread and up to 3 helper threads take the parts one by one (whoever is
// free takes the next), so a block is done as soon as the slowest part is. Nothing allocates or locks while it runs.
//
// Adaptive, so it costs nothing when it isn't needed: while the box is light, the audio thread renders every part on
// its own and the helpers sleep (no CPU, no wake-ups). When rendering takes more than ~40% of the block's time, the
// helpers are woken for the next blocks (on the ~50 ms average, or at once past 80%); once it falls back under ~20%
// for a second, they go back to sleep.
// A helper that is late never holds the block up: the audio thread takes any part not yet taken itself.
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#if !defined(__wasi__)
#include <thread>
#endif
#include <vector>
#if defined(__linux__)
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#if defined(__x86_64__) || defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace box {

#if defined(__wasi__)
// the simulator has no threads: the parts one after the other
class PartPool {
public:
    using Job = void (*)(void* ctx, int job);
    void start(int, int = 0) {}
    void stop() {}
    int helpers() const { return 0; }
    bool parallel() const { return false; }
    bool always = false; double lastLoad = 0;
    void run(int n, Job job, void* ctx, double) { for (int j = 0; j < n; ++j) job(ctx, j); }
};
#else
class PartPool {
public:
    using Job = void (*)(void* ctx, int job);
    ~PartPool() { stop(); }

    // helpers: 0 renders everything on the calling thread. prio: their real-time priority (SCHED_FIFO), 0 for none.
    void start(int helpers, int prio = 0) {
        stop(); quit.store(false);
        for (int h = 0; h < helpers && h < 3; ++h) th.emplace_back([this, prio] { loop(prio); });
    }
    void stop() {
        if (th.empty()) return;
        quit.store(true); gen.fetch_add(1, std::memory_order_release); wake();
        for (auto& t : th) t.join(); th.clear();
    }
    int helpers() const { return (int) th.size(); }
    bool always = false;   // tests: parallel on every block
    double lastLoad = 0;   // the last block's render time / its duration (as if on one thread)
    bool parallel() const { return par; }

    // runs job(ctx, 0..n-1); returns once all are done. budget: the block's duration in seconds (for the adaptive switch)
    void run(int n, Job job, void* ctx, double budget) {
        const auto t0 = std::chrono::steady_clock::now();
        if (!par || th.empty()) for (int j = 0; j < n; ++j) job(ctx, j);
        else {
            fn.store(job, std::memory_order_relaxed); arg.store(ctx, std::memory_order_relaxed); nj.store(n, std::memory_order_relaxed); done.store(0, std::memory_order_relaxed);
            const uint32_t g = gen.load(std::memory_order_relaxed) + 1;
            ticket.store((uint64_t) g << 32, std::memory_order_relaxed);
            gen.store(g, std::memory_order_release);
            if (sleeping.load(std::memory_order_acquire)) wake();
            work(g);
            while (done.load(std::memory_order_acquire) < n) relax();
        }
        // the switch, on how long this block's parts took (in parallel: their sum, ~what one thread would have taken)
        const double used = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double load = (par && !th.empty() ? sum.exchange(0) : used) / budget;
        lastLoad = load;
        avg += (load - avg) * std::min(1.0, budget / .05);   // ~50 ms average: one late block (the OS, a cold cache) is not a trend
        if (always) par = true;
        else if (!par && (avg > .35 || load > .8)) { par = true; calm = 0; }
        else if (par && avg < .2) { calm += budget; if (calm > 1) par = false; }
        else calm = 0;
    }

private:
    std::vector<std::thread> th;
    std::atomic<uint32_t> gen { 0 };      // a new round of jobs
    std::atomic<uint64_t> ticket { 0 };   // the round (high 32 bits) and the next job to take (low)
    std::atomic<int> done { 0 }, sleeping { 0 };
    std::atomic<bool> quit { false };
    std::atomic<double> sum { 0 };        // the time the jobs took, all threads together
    std::atomic<Job> fn { nullptr }; std::atomic<void*> arg { nullptr }; std::atomic<int> nj { 0 };   // atomic: a late helper may still read them
    bool par = false; double calm = 0, avg = 0;

    static void relax() {
#if defined(__x86_64__) || defined(__SSE2__)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield");
#endif
    }
    void wake() {
#if defined(__linux__)
        syscall(SYS_futex, (uint32_t*) &gen, FUTEX_WAKE_PRIVATE, 1 << 30, nullptr, nullptr, 0);
#endif
    }
    void sleepWhile(uint32_t seen) {
#if defined(__linux__)
        syscall(SYS_futex, (uint32_t*) &gen, FUTEX_WAIT_PRIVATE, seen, nullptr, nullptr, 0);   // returns at once if gen moved
#else
        std::this_thread::sleep_for(std::chrono::microseconds(200));
#endif
    }
    // take jobs of round g until none is left
    void work(uint32_t g) {
        for (;;) {
            uint64_t t = ticket.load(std::memory_order_acquire);
            if ((uint32_t) (t >> 32) != g || (int) (uint32_t) t >= nj.load(std::memory_order_relaxed)) return;
            if (!ticket.compare_exchange_weak(t, t + 1, std::memory_order_acq_rel)) continue;
            const auto t0 = std::chrono::steady_clock::now();
            fn.load(std::memory_order_relaxed)(arg.load(std::memory_order_relaxed), (int) (uint32_t) t);
            const double d = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double s = sum.load(std::memory_order_relaxed); while (!sum.compare_exchange_weak(s, s + d, std::memory_order_relaxed)) {}
            done.fetch_add(1, std::memory_order_release);
        }
    }
    void loop(int prio) {
#if defined(__linux__)
        if (prio > 0) { sched_param sp {}; sp.sched_priority = prio; pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp); }   // may be refused: fine
#else
        (void) prio;
#endif
#if defined(__aarch64__)
        { uint64_t c; asm volatile("mrs %0, fpcr" : "=r"(c)); c |= 1ull << 24; asm volatile("msr fpcr, %0" : : "r"(c)); }   // no denormals here either
#elif defined(__x86_64__) || defined(__SSE2__)
        _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
        uint32_t seen = gen.load(std::memory_order_acquire);
        for (;;) {
            // spin a very little (a few us) before sleeping: the wake-up costs less than burning a core between blocks
            int spin = 0;
            while (gen.load(std::memory_order_acquire) == seen && spin < 256) { relax(); ++spin; }
            if (gen.load(std::memory_order_acquire) == seen) { sleeping.fetch_add(1); sleepWhile(seen); sleeping.fetch_sub(1); continue; }
            seen = gen.load(std::memory_order_acquire);
            if (quit.load()) return;
            work(seen);
        }
    }
};

#endif
} // namespace box
