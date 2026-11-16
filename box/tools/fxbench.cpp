// The FX test bench: for each effect and model, alone in slot 1 at full mix:
//   CPU    one stereo rack running it, % of real time
//   alias  a 2.5 kHz sine through it at 48 kHz against the same at 192 kHz (the reference): the energy that only the
//          48 kHz one has below 20 kHz, against the whole, in dB (lower = cleaner; what a distortion folds back)
//   hf     a 10 kHz sine through it: its level out (dB, against the same effect on a 1 kHz sine: the top it loses)
//   grain  the 10 kHz sine: the energy more than 400 Hz away from 10 kHz, against the whole, dB (interpolation noise
//          of a modulated delay; for a distortion its harmonics count too)
// Build: g++ -std=c++17 -O2 -I. -Icore box/tools/fxbench.cpp -o /tmp/fxbench   Run: /tmp/fxbench [effect number...]
#include "core/FxEngine.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
static void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const auto w = std::polar(1.0, -2 * M_PI / len);
        for (size_t i = 0; i < n; i += len) { std::complex<double> u = 1; for (size_t k = 0; k < len / 2; ++k) { auto x = a[i + k], y = a[i + k + len / 2] * u; a[i + k] = x + y; a[i + k + len / 2] = x - y; u *= w; } } }
}
static int pid(const std::string& id) { for (int i = 0; i < fxp::NP; ++i) if (id == fxp::PDEFS[i].id) return i; return -1; }
static void setup(oct::FxRack& R, double fs, int eff, int model) {
    R.prepare(fs); R.p[pid("insrc")] = 2;   // the rack hears the input (not its demo loop), as in the box
    for (int s = 0; s < 8; ++s) { const int i = pid("slot" + std::to_string(s)); if (i >= 0) R.p[i] = s == 0 ? (float) eff : 0.f; }
    const std::string q = "q0f" + std::to_string(eff - 1);
    const int m = pid(q + "model"); if (m >= 0) R.p[m] = (float) model;
    const int x = pid(q + "mix"); if (x >= 0) R.p[x] = fxp::PDEFS[x].mx;
}
static std::vector<double> spectrum(int eff, int model, double f, double fs, size_t N, double* cpu = nullptr) {
    oct::FxRack R; setup(R, fs, eff, model);
    const size_t skip = (size_t) (fs * .5) / 256 * 256; std::vector<float> in(skip + N + 256), L(in.size()), Rr(in.size());
    for (size_t i = 0; i < in.size(); ++i) in[i] = (float) (.5 * std::sin(2 * M_PI * f * i / fs));
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i + 256 <= in.size(); i += 256) R.process(&in[i], &in[i], &L[i], &Rr[i], 256);
    if (cpu) *cpu = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / (in.size() / fs) * 100;
    std::vector<std::complex<double>> a(N); for (size_t i = 0; i < N; ++i) a[i] = L[skip + i] * (0.5 - 0.5 * std::cos(2 * M_PI * i / N));
    fft(a); std::vector<double> P(N / 2); for (size_t k = 0; k < N / 2; ++k) P[k] = std::norm(a[k]) / ((double) N * N); return P;
}
static double db(double x) { return 10 * std::log10(std::max(x, 1e-30)); }
int main(int argc, char** argv) {
    const int sl = pid("slot0"); const char* names = fxp::PDEFS[sl].choices;
    std::vector<std::string> eff; { std::string s = names; size_t a = 0; for (;;) { size_t b = s.find('|', a); eff.push_back(s.substr(a, b == std::string::npos ? b : b - a)); if (b == std::string::npos) break; a = b + 1; } }
    std::printf("%-12s %-20s %6s %9s %7s %8s\n", "effect", "model", "CPU", "alias", "hf", "grain");
    for (int e = 1; e < (int) eff.size(); ++e) {
        if (argc > 1) { bool want = false; for (int q = 1; q < argc; ++q) want = want || std::atoi(argv[q]) == e; if (!want) continue; }
        const int mi = pid("q0f" + std::to_string(e - 1) + "model");
        std::vector<std::string> models { "-" };
        if (mi >= 0) { models.clear(); std::string s = fxp::PDEFS[mi].choices; size_t a = 0; for (;;) { size_t b = s.find('|', a); models.push_back(s.substr(a, b == std::string::npos ? b : b - a)); if (b == std::string::npos) break; a = b + 1; } }
        for (int m = 0; m < (int) models.size(); ++m) {
            const size_t N = 32768; double cpu = 0;
            const auto A = spectrum(e, m, 2500, 48000, N, &cpu), B = spectrum(e, m, 2500, 192000, N * 4);
            const size_t top = (size_t) (20000.0 * N / 48000); double tot = 0, bad = 0;
            for (size_t k = 8; k < top; k += 8) { double a = 0, b = 0; for (size_t j = 0; j < 8; ++j) { a += A[k + j]; b += B[k + j]; } tot += a; if (a > 4 * b) bad += a - b; }
            const auto H = spectrum(e, m, 10000, 48000, N), Lo = spectrum(e, m, 1000, 48000, N);
            double hin = 0, hout = 0, lo = 0; const size_t c = (size_t) (10000.0 * N / 48000), w = (size_t) (400.0 * N / 48000), c1 = (size_t) (1000.0 * N / 48000);
            for (size_t k = 1; k < N / 2; ++k) { if (k + w >= c && k <= c + w) hin += H[k]; else hout += H[k]; if (k + w >= c1 && k <= c1 + w) lo += Lo[k]; }
            std::printf("%-12s %-20s %5.1f%% %6.1f dB %4.1f dB %5.1f dB\n", eff[(size_t) e].c_str(), models[(size_t) m].c_str(), cpu, db(bad) - db(tot), db(hin) - db(lo), db(hout) - db(hin + hout));
        }
    }
}
