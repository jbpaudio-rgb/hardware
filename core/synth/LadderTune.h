#pragma once
// Tuning for the explicit ladders (Mini, A26, Pro70, J8, VC3, DFAM): each pole is s += g (x - s), one sample late, and
// the resonance comes back from the last pole. With g = 1 - exp(-w) the loop's extra phase pulls the resonance flat
// (-1 semitone at 1 kHz, -6 at 10 kHz) and its strength changes with the cutoff. Here g is chosen so every pole lags
// exactly 180/N degrees at the cutoff (the resonant peak, and the filter played as an oscillator, are in tune), and kc
// rescales the resonance so the same knob setting rings the same at every cutoff.
#include <cmath>
#include <algorithm>
#include "EngineBase.h"

namespace oct {
inline double ladderG(double fc, double fs, int N, double* kc = nullptr) {
    const double pi = 3.14159265358979323846, lim = pi / N * .92;
    if (classicSound()) { if (kc) *kc = 1; return 1 - std::exp(-2 * pi * fc / fs); }
    const double w = std::min(2 * pi * fc / fs, lim), t = std::tan(pi / N - w);
    const double a = t / (std::sin(w) + t * std::cos(w)), g = 1 - a;
    if (kc) {
        if (N < 3) *kc = 1;
        else { const double mag = g / std::hypot(1 - a * std::cos(w), a * std::sin(w)); *kc = std::pow(std::cos(pi / N) / mag, N); }
    }
    return g;
}
// The ladders without the shortcut: N one-pole sections in their trapezoidal (TPT) form, the resonance's loop solved
// in the same sample (zero-delay feedback), so the cutoff is exact (prewarped) at every frequency and k = 4 (4 poles)
// or 8 (3) is exactly where it starts to sing; the input stage's curve (sat) is applied to the solved input (the
// usual cheap nonlinear ZDF: one evaluation, no iteration). g = tan(pi fc / fs). Returns the last pole's output.
// zs: the sections' own (TPT) states; y: each section's output, written here (what the engines read: y[N-1] the
// lowpass, the others for the DFAM's highpass); uOut: the input stage's output.
template <int N, class Sat> inline double zdfLadder(double* zs, double* y, double x, double g, double k, Sat sat, double* uOut = nullptr) {
    const double G = g / (1 + g), h = 1 / (1 + g);
    double S = 0, Gp = 1; for (int i = N - 1; i >= 0; --i) { S += Gp * zs[i] * h; Gp *= G; }   // Gp ends at G^N
    double u = sat((x - k * S) / (1 + k * Gp)); if (uOut) *uOut = u;
    for (int i = 0; i < N; ++i) { const double v = (u - zs[i]) * G, o = v + zs[i]; zs[i] = o + v; y[i] = o; u = o; }
    return u;
}
inline double zdfG(double fc, double fs) { return std::tan(3.14159265358979323846 * std::min(fc, fs * .45) / fs); }
} // namespace oct
