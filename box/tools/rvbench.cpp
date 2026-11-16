// The reverb bench: each REVERB and SHIMMER model's impulse response, alone in slot 1 at full mix:
//   RT60   the decay time (Schroeder, from -5 to -25 dB, extrapolated), seconds
//   dens   echo density in the 60-160 ms tail (Abel & Huang's normalized echo density: ~1 = as dense as noise; low
//          values are audible as separate echoes, a "flutter")
//   ring   the tail's (100-700 ms, averaged spectrum) strongest resonance above its 1/3-octave median, dB,
//          200 Hz-8 kHz: metallic ringing ('classic' as an argument: the reverbs as they were, to compare)
//   corr   left/right correlation of the tail (1 = mono, ~0 = wide)
// Build: g++ -std=c++17 -O2 -I. -Icore box/tools/rvbench.cpp -o /tmp/rvbench
#include "core/FxEngine.h"
#include <algorithm>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>
static void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const auto w = std::polar(1.0, -2 * M_PI / len);
        for (size_t i = 0; i < n; i += len) { std::complex<double> u = 1; for (size_t k = 0; k < len / 2; ++k) { auto x = a[i + k], y = a[i + k + len / 2] * u; a[i + k] = x + y; a[i + k + len / 2] = x - y; u *= w; } } }
}
static int pid(const std::string& id) { for (int i = 0; i < fxp::NP; ++i) if (id == fxp::PDEFS[i].id) return i; return -1; }
static std::vector<std::string> split(const char* c) { std::vector<std::string> v; std::string s = c; size_t a = 0; for (;;) { size_t b = s.find('|', a); v.push_back(s.substr(a, b == std::string::npos ? b : b - a)); if (b == std::string::npos) break; a = b + 1; } return v; }
int main(int argc, char** argv) {
    oct::classicSound() = argc > 1;   // "classic": the reverbs as they were
    const double fs = 48000; const int N = (int) (fs * 4);
    std::printf("%-8s %-16s %6s %6s %7s %6s\n", "effect", "model", "RT60", "dens", "ring", "corr");
    for (int eff : { 7, 16 }) {
        const int mi = pid("q0f" + std::to_string(eff - 1) + "model"); const auto models = split(fxp::PDEFS[mi].choices);
        for (int m = 0; m < (int) models.size(); ++m) {
            oct::FxRack R; R.prepare(fs); R.p[pid("insrc")] = 2;
            for (int s = 0; s < 8; ++s) R.p[pid("slot" + std::to_string(s))] = s == 0 ? (float) eff : 0.f;
            R.p[mi] = (float) m; const int x = pid("q0f" + std::to_string(eff - 1) + "mix"); if (x >= 0) R.p[x] = fxp::PDEFS[x].mx;
            std::vector<float> in((size_t) N, 0.f), L((size_t) N), Rr((size_t) N); in[4800] = .5f;   // the impulse at 100 ms (the rack settled)
            for (int i = 0; i + 256 <= N; i += 256) R.process(&in[(size_t) i], &in[(size_t) i], &L[(size_t) i], &Rr[(size_t) i], 256);
            std::vector<double> h; for (int i = 4800; i < N; ++i) h.push_back(.5 * (L[(size_t) i] + Rr[(size_t) i]));
            // RT60: Schroeder backward integration
            std::vector<double> E(h.size()); double acc = 0; for (size_t i = h.size(); i-- > 0;) { acc += h[i] * h[i]; E[i] = acc; }
            double t5 = -1, t25 = -1; for (size_t i = 0; i < E.size(); ++i) { const double d = 10 * std::log10(E[i] / E[0] + 1e-30); if (t5 < 0 && d < -5) t5 = i / fs; if (t25 < 0 && d < -25) { t25 = i / fs; break; } }
            const double rt = t25 > 0 ? (t25 - t5) * 3 : 99;
            // echo density, 20 ms windows from 60 to 160 ms
            double ned = 0; int nw = 0;
            for (int w0 = (int) (.06 * fs); w0 + (int) (.02 * fs) <= (int) (.16 * fs); w0 += (int) (.01 * fs)) {
                const int W = (int) (.02 * fs); double s2 = 0; for (int i = w0; i < w0 + W; ++i) s2 += h[(size_t) i] * h[(size_t) i]; const double sd = std::sqrt(s2 / W);
                int c = 0; for (int i = w0; i < w0 + W; ++i) c += std::fabs(h[(size_t) i]) > sd; ned += c / (double) W / .3173; ++nw;
            }
            ned /= nw;
            // ringing: tail spectrum against its 1/3-octave running median
            const size_t F = 4096; std::vector<double> P(F / 2, 0.0);   // 4096-point frames averaged over the tail (Welch)
            for (size_t f0 = (size_t) (.1 * fs); f0 + F <= (size_t) (.7 * fs); f0 += F / 2) {
                std::vector<std::complex<double>> a(F); for (size_t i = 0; i < F; ++i) a[i] = h[f0 + i] * (.5 - .5 * std::cos(2 * M_PI * i / F));
                fft(a); for (size_t k = 0; k < F / 2; ++k) P[k] += std::norm(a[k]);
            }
            for (auto& x : P) x = 10 * std::log10(x + 1e-30);
            double ring = 0;
            for (size_t k = (size_t) (200.0 * F / fs); k < (size_t) (8000.0 * F / fs); ++k) {
                const size_t lo = (size_t) (k / 1.12), hi = (size_t) (k * 1.12); std::vector<double> win(P.begin() + (long) lo, P.begin() + (long) hi + 1);
                std::nth_element(win.begin(), win.begin() + (long) win.size() / 2, win.end()); ring = std::max(ring, P[k] - win[win.size() / 2]);
            }
            // stereo
            double sl = 0, sr = 0, slr = 0; for (int i = (int) (.1 * fs) + 4800; i < (int) (.7 * fs) + 4800; ++i) { sl += L[(size_t) i] * L[(size_t) i]; sr += Rr[(size_t) i] * Rr[(size_t) i]; slr += L[(size_t) i] * Rr[(size_t) i]; }
            std::printf("%-8s %-16s %5.2fs %6.2f %5.1f dB %6.2f\n", eff == 7 ? "REVERB" : "SHIMMER", models[(size_t) m].c_str(), rt, ned, ring, slr / std::sqrt(sl * sr + 1e-30));
        }
    }
}
