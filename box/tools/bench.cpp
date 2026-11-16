// The sound test bench: for each engine, how much CPU it takes (a 4-note chord) and how much aliasing a high note makes.
// Build: g++ -std=c++17 -O2 -I. -Icore box/tools/bench.cpp -o /tmp/bench   Run: /tmp/bench [engine...]
#include "box/BoxEngines.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>
using namespace box;
static void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const auto w = std::polar(1.0, -2 * M_PI / len);
        for (size_t i = 0; i < n; i += len) { std::complex<double> u = 1; for (size_t k = 0; k < len / 2; ++k) { auto x = a[i + k], y = a[i + k + len / 2] * u; a[i + k] = x + y; a[i + k + len / 2] = x - y; u *= w; } } }
}
// aliasing: the same note rendered at 48 kHz and at 192 kHz (the reference, where almost nothing folds back); the
// energy the 48 kHz one has below 20 kHz where the reference has (6 dB) less, against the whole, in dB: lower = cleaner
// what would differ between two renders anyway (random drift, noise): off, so only aliasing is left
static void quiet(oct::EngineBase& E) {
    for (int i = 0; i < E.np; ++i) {
        std::string n = std::string(E.D[i].id) + " " + E.D[i].name; for (auto& c : n) c = (char) std::tolower((unsigned char) c);
        if (n.find("drift") != std::string::npos || n.find("vintage") != std::string::npos || n.rfind("rev", 0) == 0 || n.find("reverb") != std::string::npos || (n.find("noise") != std::string::npos && E.D[i].type == 'f')) E.p[(size_t) i] = E.D[i].mn;
    }
}
static std::vector<double> spectrum(const char* id, int note, double fs, size_t N) {
    std::unique_ptr<oct::EngineBase> E(makeBoxEngine(id)); quiet(*E); E->prepare(fs); E->noteOn(note, .9);
    const size_t skip = (size_t) (fs * .25) / 256 * 256; std::vector<float> L(skip + N + 256), R(L.size());
    for (size_t i = 0; i + 256 <= L.size(); i += 256) E->process(&L[i], &R[i], 256);
    std::vector<std::complex<double>> a(N); for (size_t i = 0; i < N; ++i) a[i] = L[skip + i] * (0.5 - 0.5 * std::cos(2 * M_PI * i / N));
    fft(a); std::vector<double> P(N / 2); for (size_t k = 0; k < N / 2; ++k) P[k] = std::norm(a[k]) / ((double) N * N); return P;
}
static double alias(const char* id, int note) {
    const size_t N = 32768; const auto A = spectrum(id, note, 48000, N), B = spectrum(id, note, 192000, N * 4);   // the same bin width
    const size_t top = (size_t) (20000.0 * N / 48000); double tot = 0, bad = 0;
    for (size_t k = 8; k < top; k += 8) {   // bands of 8 bins (12 Hz)
        double a = 0, b = 0; for (size_t j = 0; j < 8; ++j) { a += A[k + j]; b += B[k + j]; } tot += a; if (a > 4 * b) bad += a - b;
    }
    return 10 * std::log10(std::max(bad, 1e-30) / std::max(tot, 1e-30));
}
int main(int argc, char** argv) {
    const double fs = 48000;
    std::printf("%-8s %10s %12s %12s %12s\n", "engine", "CPU 4 voix", "alias C5", "alias C6", "alias C7");
    for (int e = 0; e < NENG; ++e) {
        const std::string id = ENGS[e].id; if (id == "dr8" || id == "sp64" || id == "dfam") continue;
        if (argc > 1) { bool want = false; for (int q = 1; q < argc; ++q) want = want || id == argv[q]; if (!want) continue; }
        std::unique_ptr<oct::EngineBase> E(makeBoxEngine(id.c_str())); E->prepare(fs);
        for (int n : { 48, 55, 60, 64 }) E->noteOn(n, .8);
        float L[256], R[256]; const int blocks = (int) (8 * fs / 256);
        const auto t0 = std::chrono::steady_clock::now(); for (int b = 0; b < blocks; ++b) E->process(L, R, 256);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        E->allOff();
        std::printf("%-8s %9.1f%% %11.1f dB %11.1f dB %11.1f dB\n", id.c_str(), 100 * sec / 8, alias(id.c_str(), 72), alias(id.c_str(), 84), alias(id.c_str(), 96));
    }
}
