// Octavio: DSP building blocks shared by the engines.
// Ported line for line from the web AudioWorklet code so the plugin and the web app sound the same.
// Buffers that are Float32Array in the web code are float here; everything else is double, like JavaScript numbers.
#pragma once
#include <cmath>
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>

namespace oct {
// the simulator's A/B switch: true plays the engines and FX as they were before the sound passes (old filter tuning,
// curves without antialiasing, 808 oscillators, no knob glide), to compare by ear. Off on the box.
inline bool& classicSound() { static bool b = false; return b; }


constexpr double PI = 3.14159265358979323846;
constexpr double TP = 2.0 * PI;
constexpr int CT = 32;   // control rate: values are recomputed every 32 samples

inline double cl(double x, double a, double b) { return x < a ? a : x > b ? b : x; }
inline double jround(double x) { return std::floor(x + 0.5); }          // Math.round
inline int iround(double x) { return (int) std::floor(x + 0.5); }
inline double hann(double t) { return 0.5 - 0.5 * std::cos(TP * t); }
inline double lfoHz(double k) { return 0.01 * std::pow(3000.0, k); }
inline double dbg(double db) { return std::pow(10.0, db / 20.0); }
inline double sgn(double x) { return x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0; }
inline double frac(double x) { return x - std::floor(x); }
static const double NOTEB[13] = { .125, 1.0 / 6, .25, 1.0 / 3, .375, .5, 2.0 / 3, .75, 1, 4.0 / 3, 1.5, 2, 4 };

// the web code's random generator (32-bit LCG), so both versions draw the same numbers
struct Rnd {
    uint32_t s = 99991;
    double operator()() { s = s * 1664525u + 1013904223u; return s / 4294967296.0; }
};

// delay line with linear interpolated reads
struct DL {
    std::vector<float> b; int n = 0, w = 0;
    void init(int N) { b.assign((size_t) N, 0.f); n = N; w = 0; }
    void put(double x) { b[(size_t) w] = (float) x; if (++w >= n) w = 0; }
    double get(double d) const {
        d = cl(d, 1, n - 2); double p = w - d; while (p < 0) p += n; if (p >= n) p -= n;   // p can round up to n when the read lands a hair before the write
        const int i = (int) std::floor(p); const double f = p - i;
        const double a = b[(size_t) i], c = b[(size_t) (i + 1 >= n ? 0 : i + 1)];
        if (classicSound()) return a + (c - a) * f;
        // between two samples: a cubic through the 4 around (Hermite) rather than a straight line, which dulled the
        // top and added grain whenever the delay moved (chorus, flanger, tape wow)
        const double z = b[(size_t) (i == 0 ? n - 1 : i - 1)], e = b[(size_t) (i + 2 >= n ? i + 2 - n : i + 2)];
        const double c1 = .5 * (c - z), c2 = z - 2.5 * a + 2 * c - .5 * e, c3 = .5 * (e - z) + 1.5 * (a - c);
        return ((c3 * f + c2) * f + c1) * f + a;
    }
    double at(double i) const { i = std::fmod(i, (double) n); if (i < 0) i += n; return b[(size_t) i]; }
};

struct Comb {
    std::vector<float> b; int i = 0; double s = 0;
    void init(int n) { b.assign((size_t) n, 0.f); i = 0; s = 0; }
    double run(double x, double fb, double damp) {
        const double y = b[(size_t) i]; s = y * (1 - damp) + s * damp; b[(size_t) i] = (float) (x + s * fb);
        if (++i >= (int) b.size()) i = 0; return y;
    }
};
struct AP {
    std::vector<float> b; int i = 0;
    void init(int n) { b.assign((size_t) n, 0.f); i = 0; }
    double run(double x) {
        const double y = b[(size_t) i]; b[(size_t) i] = (float) (x + y * .5);
        if (++i >= (int) b.size()) i = 0; return y - x;
    }
};

// a stereo Freeverb-style tank
struct Tank {
    std::vector<Comb> c[2]; std::vector<AP> a[2]; double g = 0;
    void init(double fs, int nc) {
        const double k = fs / 44100; const int CL[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, AL[4] = { 556, 441, 341, 225 };
        for (int ch = 0; ch < 2; ++ch) {
            c[ch].assign((size_t) nc, Comb()); a[ch].assign(4, AP());
            for (int j = 0; j < nc; ++j) c[ch][(size_t) j].init(iround((CL[j] + (ch ? 23 : 0)) * k));
            for (int j = 0; j < 4; ++j) a[ch][(size_t) j].init(iround((AL[j] + (ch ? 23 : 0)) * k));
        }
        g = .015 * 8 / nc;
    }
    void run(double l, double r, double fb, double damp, double* out) {
        const double x = (l + r) * g;
        for (int ch = 0; ch < 2; ++ch) { double y = 0; for (auto& cb : c[ch]) y += cb.run(x, fb, damp); for (auto& ap : a[ch]) y = ap.run(y); out[ch] = y; }
    }
};

// delay-line pitch shifter: two crossfaded heads
struct Shift {
    DL d; double ph = 0;
    void init(int n) { d.init(n); ph = 0; }
    // hard splice (edge = short crossfade), like an early harmonizer without de-glitching
    double runEdge(double x, double ratio, double win, double e) {
        d.put(x); ph += (1 - ratio) / win; ph -= std::floor(ph); const double p2 = std::fmod(ph + .5, 1.0);
        auto g = [e](double t) { return std::min({ 1.0, t / e, (1 - t) / e }); };
        const double a = g(ph), b = g(p2);
        return (d.get(1 + ph * win) * a + d.get(1 + p2 * win) * b) / std::max(.5, a + b);
    }
    double run(double x, double ratio, double win) {
        d.put(x); ph += (1 - ratio) / win; ph -= std::floor(ph); const double p2 = std::fmod(ph + .5, 1.0);
        return d.get(1 + ph * win) * hann(ph) + d.get(1 + p2 * win) * hann(p2);
    }
};

struct OP { double s = 0; double lp(double x, double a) { s += a * (x - s); return s; } };

// TPT state-variable filter
struct SVF {
    double a = 0, b = 0, lpo = 0, bpo = 0, hpo = 0;
    double run(double x, double g, double k) {
        const double h = (x - (k + g) * a - b) / (1 + k * g + g * g); const double bp = g * h + a; a = g * h + bp;
        const double lp = g * bp + b; b = g * bp + lp; lpo = lp; bpo = bp; hpo = h; return lp;
    }
};

// A static curve f (a distortion, a clipper) without aliasing: instead of f(x[n]), the mean of f over the straight
// line from x[n-1] to x[n] (first-order antiderivative antialiasing, the integral taken numerically: 3 Gauss points,
// so any curve works without its antiderivative). Folds back ~20-30 dB less, for half a sample of delay.
// The oscillators' band-limited steps. polyBLEP smooths a saw's or a pulse's jump over the 2 samples around it; the
// 4-point one (the integral of a cubic B-spline, over 4 samples) folds back much less of the top harmonics, still
// from the phase alone (no look-ahead). Returns the correction for a jump of 2 (a saw's): saw = 2p - 1 - blep(p, dt).
inline double bspS(double x) {   // the smoothed step: 0 at -2, 1/2 at 0, 1 at 2
    if (!(x > -2)) return 0; if (x >= 2) return 1;   // (a NaN phase: 0; x = 0 is the middle, so no recursion)
    if (x < -1) { const double a = x + 2; return a * a * a * a / 24; }
    if (x <= 0) return 1. / 24 + (4 * x - 2 * x * x * x - .75 * x * x * x * x) / 6 + 2.75 / 6;
    return 1 - bspS(-x);
}
inline double blep2(double t, double dt) { if (t < dt) { t /= dt; return t + t - t * t - 1; } if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; } return 0; }
inline double blep4(double t, double dt) {
    if (dt >= .2) return blep2(t, dt);   // too few samples a cycle for 4: the 2-point one
    if (t < 2 * dt) return 2 * (bspS(t / dt) - 1);
    if (t > 1 - 2 * dt) return 2 * bspS((t - 1) / dt);
    return 0;
}
inline double blepAny(double t, double dt) { return classicSound() ? blep2(t, dt) : blep4(t, dt); }
template <class F> inline double adaa(F f, double x0, double x1) {
    constexpr double a = .1127016653792583, b = .8872983346207417;   // .5 -+ sqrt(.15)
    const double d = x1 - x0;
    return (5 * f(x0 + a * d) + 8 * f(x0 + .5 * d) + 5 * f(x0 + b * d)) / 18;
}
inline double triode(double u) { return u >= 0 ? std::tanh(u) : std::tanh(u * .55) / .55 * .72; }

// bucket-brigade line: a delay with its anti-alias and reconstruction low passes and a soft limit
struct BBD {
    DL d; double a = 0, b = 0, c = 0, ax = 0, env = 0; uint32_t ns = 22222;
    void init(int n) { d.init(n); a = b = c = ax = env = 0; }
    double run(double x, double del, double ap) {
        a += ap * (x - a);
        if (classicSound()) d.put(std::tanh(a * 1.1) / 1.1);
        else { d.put(adaa([](double v) { return std::tanh(v); }, ax, a * 1.1) / 1.1); ax = a * 1.1; }   // the chip's clipping, antialiased
        double y = d.get(del);
        if (!classicSound()) {   // its hiss, which the compander lets rise and fall with the signal (the BBD's breathing)
            env += .002 * (std::fabs(a) - env); ns = ns * 1664525u + 1013904223u;
            y += ((double) ns / 4294967296.0 - .5) * (2e-4 + .006 * env);
        }
        b += ap * (y - b); c += ap * (b - c); return c;
    }
};

// 8-line feedback delay network: input diffusion, per-line low / high decay, air damping, slow modulation, Hadamard mixing
struct FDN {
    double fs = 48000; DL d[8]; float lo[8] {}, hs[8] {}, v[8] {}, y[8] {}, len[8] {}, gL[8] {}, gH[8] {}, ph[8] {};
    DL ap[2][4]; double aX = 0, aD = 0, diff = 0, md = 0, mr = 0;
    // an allpass inside each line's loop: every echo that comes round is smeared again, so the tail turns to a dense
    // wash in tens of milliseconds instead of a cloud of separate repeats, and the modes ring less (classic: off)
    DL la[8]; int laN[8] {}; double laG = 0;
    void init(double sr) {
        fs = sr; for (auto& x : d) x.init(iround(fs * .22));
        const float P0[8] = { 0, .13f, .29f, .41f, .55f, .67f, .79f, .91f };
        for (int k = 0; k < 8; ++k) { lo[k] = hs[k] = v[k] = y[k] = len[k] = gL[k] = gH[k] = 0; ph[k] = P0[k]; }
        const double A0[4] = { 4.7, 3.6, 12.7, 9.3 }, A1[4] = { 5.3, 4.1, 11.3, 8.1 };
        for (int k = 0; k < 4; ++k) { ap[0][k].init(iround(fs * A0[k] / 1000)); ap[1][k].init(iround(fs * A1[k] / 1000)); }
        for (auto& x : la) x.init(iround(fs * .02));
        aX = aD = diff = md = mr = 0;
    }
    void set(double size, double rt, double lowMul, double dampHz, double df, double mod) {
        const double B[8] = { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.7, 73.1 }, LA[8] = { 3.1, 4.3, 5.9, 2.7, 6.7, 3.7, 5.3, 4.9 };
        const bool inner = !classicSound(); laG = inner ? .35 + .3 * df : 0;
        for (int k = 0; k < 8; ++k) {
            laN[k] = inner ? (int) cl(LA[k] * std::sqrt(size) * fs / 1000, 8, fs * .019) : 0;
            const double n = cl(B[k] * size * fs / 1000, 16, fs * .21) + laN[k]; len[k] = (float) (n - laN[k]);
            gH[k] = (float) std::pow(10.0, -3 * n / (rt * fs)); gL[k] = (float) std::pow(10.0, -3 * n / (rt * lowMul * fs));
        }
        aX = 1 - std::exp(-TP * 250 / fs); aD = 1 - std::exp(-TP * std::min(dampHz, fs * .45) / fs); diff = df; md = mod * .0012 * fs; mr = .4 / fs;
    }
    void run(double il, double ir, double* out) {
        const double c = diff; double xs[2] = { il, ir };
        for (int ch = 0; ch < 2; ++ch) {
            double x = xs[ch];
            for (int k = 0; k < 4; ++k) { DL& dl = ap[ch][k]; const double z = dl.b[(size_t) dl.w]; const double vv = x + c * z; dl.put(vv); x = z - c * vv; }
            xs[ch] = x;
        }
        for (int k = 0; k < 8; ++k) {
            ph[k] = (float) (ph[k] + mr * (1 + k * .13)); if (ph[k] >= 1) ph[k] = (float) (ph[k] - 1);
            double s = md != 0 ? d[k].get(len[k] + md * (1 + std::sin(TP * ph[k]))) : d[k].get(len[k]);
            if (laN[k]) { DL& a = la[k]; int ri = a.w - laN[k]; if (ri < 0) ri += a.n; const double z = a.b[(size_t) ri]; const double vv = s + laG * z; a.put(vv); s = z - laG * vv; }
            y[k] = (float) s;
            lo[k] = (float) (lo[k] + aX * (s - lo[k])); const double w = lo[k] * (double) gL[k] + (s - lo[k]) * (double) gH[k];
            hs[k] = (float) (hs[k] + aD * (w - hs[k])); v[k] = hs[k];
        }
        for (int h = 1; h < 8; h <<= 1) for (int i = 0; i < 8; i += h << 1) for (int j = i; j < i + h; ++j) { const float a = v[j], b = v[j + h]; v[j] = (float) ((double) a + b); v[j + h] = (float) ((double) a - b); }
        const double nr = .35355; for (int k = 0; k < 8; ++k) d[k].put(v[k] * nr + ((k & 1) ? xs[1] : xs[0]) * .5);
        out[0] = ((double) y[0] - y[2] + y[4] - y[6] + y[1] * .3) * .45; out[1] = ((double) y[1] - y[3] + y[5] - y[7] + y[0] * .3) * .45;
    }
};

// spring tank: a chain of dispersive allpasses (the chirp and drip) in a feedback loop with its delay
struct Spring {
    std::vector<float> ap; DL d; int n = 0; double lp = 0, dc = 0, fb = 0;
    void init(double fs, double ms, int stages) { ap.assign((size_t) stages, 0.f); d.init(iround(fs * ms / 1000) + 4); n = iround(fs * ms / 1000); lp = dc = fb = 0; }
    double run(double x, double c, double g, double a, double wob) {
        double y = x + fb;
        for (auto& s : ap) { const double z = c * y + s; s = (float) (y - c * z); y = z; }
        d.put(y); double o = d.get(n * (1 + wob)); lp += a * (o - lp); o = lp; dc += .002 * (o - dc); o -= dc; fb = std::tanh(o * g * 1.2) / 1.2; return o;
    }
};

// Hilbert pair (two allpass chains 90 degrees apart) for frequency shifting
struct Hilbert {
    float sa[16] {}, sb[16] {}; double dl = 0, re = 0, im = 0;
    static double chain(double x, const double* c, float* s) {
        for (int k = 0; k < 4; ++k) { const double y = c[k] * (x + s[k * 4 + 3]) - s[k * 4 + 1]; s[k * 4 + 1] = s[k * 4]; s[k * 4] = (float) x; s[k * 4 + 3] = s[k * 4 + 2]; s[k * 4 + 2] = (float) y; x = y; }
        return x;
    }
    void run(double x) {
        static const double A[4] = { .6923878, .9360654322959, .988229522686, .9987488452737 }, B[4] = { .4021921162426, .856171088242, .9722909545651, .9952884791278 };
        const double r = chain(x, A, sa); const double i0 = chain(x, B, sb); im = dl; dl = i0; re = r;
    }
};

} // namespace oct
