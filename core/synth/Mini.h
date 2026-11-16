// MINI: mono voice after the Model D (3 oscillators, ladder filter, contours, fixed filter bank).
// Port of web/mini-web/mini.html (WORKLET), line for line.
#pragma once
#include "LadderTune.h"
#include "EngineBase.h"
#include "MiniParams.h"

namespace oct {

struct Mini : EngineBase {
    static constexpr int OS = 4, CTV = 16;
    static double attS(double k) { return .001 * std::pow(1e4, k); }
    static double decS(double k) { return .004 * std::pow(8750.0, k); }
    static double glideS(double k) { return .003 * std::pow(3000.0, k); }
    static double th(double x) { if (x > 3) return 1; if (x < -3) return -1; const double x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
    static double blep(double t, double dt) { return oct::blepAny(t, dt); }

    struct Dec {
        static constexpr int N = 48; float h[N] {}, b[2 * N] {}; int p = 0;
        Dec() {
            const double fc = .43 / OS; double s = 0;
            for (int i = 0; i < N; ++i) { const double m = i - (N - 1) / 2.0; double v = m == 0 ? 2 * fc : std::sin(TP * fc * m) / (PI * m); v *= .42 - .5 * std::cos(TP * i / (N - 1)) + .08 * std::cos(2 * TP * i / (N - 1)); h[i] = (float) v; s += v; }
            for (int i = 0; i < N; ++i) h[i] = (float) (h[i] / s);
        }
        void push(double x) { b[p] = b[p + N] = (float) x; p = (p + 1) % N; }
        double out() const { double a = 0; for (int i = 0; i < N; ++i) a += (double) h[i] * b[p + i]; return a; }
    };
    struct FFB {
        double gb[14] {}, kb[14] {}; float s1[14] {}, s2[14] {}, lv[14] {};
        void init(double fs) {
            const double F[14] = { 88, 125, 175, 250, 350, 500, 700, 1000, 1400, 2000, 2800, 4000, 5600, 8000 };
            for (int b = 0; b < 14; ++b) { gb[b] = std::tan(PI * std::min(F[b], fs * .45) / fs); kb[b] = b == 0 || b == 13 ? 1.3 : .35; s1[b] = s2[b] = 0; lv[b] = 1; }
        }
        double run(double x, const float* lvT) {
            double o = 0;
            for (int b = 0; b < 14; ++b) {
                lv[b] = (float) (lv[b] + .003 * (lvT[b] - lv[b])); const double g = gb[b], k = kb[b];
                const double hp = (x - (k + g) * s1[b] - s2[b]) / (1 + k * g + g * g); const double bp = g * hp + s1[b]; s1[b] = (float) (g * hp + bp); const double lp = g * bp + s2[b]; s2[b] = (float) (g * bp + lp);
                o += (b == 0 ? lp : b == 13 ? hp : bp * k) * lv[b];
            }
            return o * 1.05;
        }
    };
    struct Contour {
        double fs = 48000, v = 0; int st = 0;
        void gate(bool on) { if (on) st = 1; else if (st) st = 3; }
        double tick(int n, double att, double dec, double sus, bool decaySw) {
            const double dt = n / fs;
            if (st == 1) { const double tau = att / 1.609; v += (1.25 - v) * (1 - std::exp(-dt / tau)); if (v >= 1) { v = 1; st = 2; } }
            else if (st == 2) { v += (sus - v) * (1 - std::exp(-dt / (dec / 4.5))); }
            else if (st == 3) { const double tau = decaySw ? dec / 4.5 : .006; v += (0 - v) * (1 - std::exp(-dt / tau)); if (v < 1e-5) { v = 0; st = 0; } }
            return v;
        }
    };

    Contour fe, le; double feS = 0, leS = 0, dk = 0; Dec dec; FFB ffb; double ffbMix = 0;
    double ph[3] {}, drift[3] {}, s[4] {}, zs[4] {}, fb = 0, o3last = 0, pk[7] {};
    std::vector<int> held; bool gate = false, edge = false, rel = false, seqGate = false; double vel = .8; int note = 48; double tgt = 48, curN = 48;
    float src[17] {}; double lp[4] {}, lr[4] {}, sh2p = 0, sh2t = 0, sh2v = 0, shp = 0, sht = 0, shv = 0, clock = 0; int stp = 0;
    struct TV { double tune = 0, o2 = 0, o3 = 0, cut = 0, emph = 0, camt = 0, l1 = 0, l2 = 0, l3 = 0, ln = 0, le = 0, mm = 0, ma = 0; } T, S; bool hasS = false;
    double ms = 0, dl = 0, dr2 = 0, xl = 0, xr = 0, drv = 0; long mc = 0;
    int w1 = 0, w2 = 0, w3 = 0; double r1 = 1, r2 = 1, r3 = 1, kt = 0, mt = 0; bool o3kb = false, om = false, fm = false, pink = false, ffbOn = false; float lvB[14] {};
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {}, iF[14] {};

    Mini() {
        setTable(mini::PDEFS, mini::DSTP, mini::NP);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        for (int b = 0; b < 14; ++b) iF[b] = id("ffb" + std::to_string(b));
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; fe = Contour(); fe.fs = fs; le = Contour(); le.fs = fs; dec = Dec(); ffb.init(fs); ffbMix = 0;
        ph[0] = 0; ph[1] = .33; ph[2] = .66; for (int i = 0; i < 3; ++i) drift[i] = 0; for (auto& x : s) x = 0; fb = o3last = 0; for (auto& x : pk) x = 0;
        held.clear(); gate = edge = rel = seqGate = false; vel = .8; note = 48; tgt = 130.8; curN = 48;   // tgt starts at 130.8 as in the web code (silent until the first note)
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; } sh2p = sh2t = sh2v = shp = sht = shv = clock = 0; stp = 0;
        hasS = false; ms = dl = dr2 = xl = xr = 0; rs = 987654321; mc = 0; feS = leS = 0; dk = 1 - std::exp(-1 / (.001 * fs)); for (auto& x : lvB) x = 0;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = src[11]; m[1] = src[12]; return 2; }
    void retarget() { note = *std::min_element(held.begin(), held.end()); tgt = note; }   // low-note priority, like the Model D
    void noteOn(int n, double v) override { const bool was = !held.empty(); held.push_back(n); vel = v; retarget(); if (!was) { edge = true; gate = true; } }
    void noteOff(int n) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); if (held.empty()) { gate = false; rel = true; } else retarget(); }
    void allOff() override { held.clear(); gate = false; rel = true; }

    void ctrl() {
        using namespace mini; const double dt = CTV / fs; float* sv_ = src;
        clock = advance(clock, g(bpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool edg = ns != stp; stp = ns; const bool half = std::fmod(clock, 1.0) < .5;
        for (int k = 0; k < 6; ++k) sv_[5 + k] = p[(size_t) iS[k][stp]];
        const int ts = ri(trig); bool fire = edge, gt = gate, release = rel; edge = false; rel = false;
        if (ts > 0) { const float sv = p[(size_t) iS[ts - 1][stp]]; const bool on = sv != 0 && half; fire = edg && sv != 0; release = seqGate && !on; seqGate = on; gt = on; }
        if (fire) { fe.gate(true); le.gate(true); } else if (release || (!gt && (fe.st == 1 || fe.st == 2))) { fe.gate(false); le.gate(false); }
        const bool dsw = g(decaysw) > .5;
        sv_[11] = (float) fe.tick(CTV, attS(val(fatt)), decS(val(fdec)), val(fsus) / 10, dsw);
        sv_[12] = (float) le.tick(CTV, attS(val(latt)), decS(val(ldec)), val(lsus) / 10, dsw);
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph_ = lp[i];
            const double W[5] = { std::sin(TP * ph_), 1 - 4 * std::fabs(ph_ - .5), 2 * ph_ - 1, ph_ < .5 ? 1.0 : -1.0, lr[i] }; sv_[i < 3 ? 1 + i : 15] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); sv_[4] = (float) shv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; } const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); sv_[16] = (float) sh2v;
        sv_[13] = (float) vel; sv_[14] = (float) ((note - 60) / 24.0); sv_[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(sv_, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(sv_, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        const double drg = g(mini::drift); for (int i = 0; i < 3; ++i) { drift[i] += (rnd() - .5) * .02; drift[i] *= .995; }
        T.tune = val(tune) + bend; T.o2 = val(o2freq); T.o3 = val(o3freq); T.cut = val(cutoff); T.emph = val(emph); T.camt = val(camt);
        T.l1 = g(o1on) > .5 ? val(o1lvl) / 10 : 0; T.l2 = g(o2on) > .5 ? val(o2lvl) / 10 : 0; T.l3 = g(o3on) > .5 ? val(o3lvl) / 10 : 0;
        T.ln = g(noiseon) > .5 ? val(noiselvl) / 10 : 0; T.le = g(exton) > .5 ? val(extlvl) / 10 : 0; T.mm = val(modmix); T.ma = val(modamt);
        w1 = ri(o1wave); w2 = ri(o2wave); w3 = ri(o3wave);
        static const double RANGE_MUL[6] = { 1.0 / 64, 1.0 / 4, 1.0 / 2, 1, 2, 4 };
        r1 = RANGE_MUL[ri(o1range)]; r2 = RANGE_MUL[ri(o2range)]; r3 = RANGE_MUL[ri(o3range)];
        o3kb = g(osc3kb) > .5; om = g(oscmod) > .5; fm = g(filtmod) > .5; pink = g(noisecol) > .5;
        kt = (g(kb1) > .5 ? 1.0 / 3 : 0) + (g(kb2) > .5 ? 2.0 / 3 : 0); drv = drg;
        mt = val(master);
        for (int b = 0; b < 14; ++b) lvB[b] = p[(size_t) iF[b]]; ffbOn = g(ffbon) > .5;
        const double gl = g(glideon) > .5 ? glideS(val(mini::glide)) : 0; curN += (gl <= 0 ? 1 : 1 - std::exp(-dt / gl)) * (tgt - curN);
        if (!hasS) { S = T; hasS = true; }
    }
    double osc(int i, int w, double dt, bool rev) {
        const double p_ = ph[i]; double v;
        switch (w) {
            case 0: v = 1 - 4 * std::fabs(p_ - .5); break;
            case 1: { const double saw = 2 * p_ - 1 - blep(p_, dt); const double tri = 1 - 4 * std::fabs(p_ - .5); v = .55 * tri + .45 * saw; break; }
            case 2: v = (2 * p_ - 1 - blep(p_, dt)) * (rev ? -1 : 1); break;
            default: { const double pw = w == 3 ? .5 : w == 4 ? .3 : .1; double t2 = p_ - pw; if (t2 < 0) t2 += 1; v = (p_ < pw ? 1 : -1) + blep(p_, dt) - blep(t2, dt); v -= (2 * pw - 1); v *= w == 5 ? 1.25 : 1; }
        }
        return v;
    }
    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const double k = .003; double* Tp = &T.tune; double* Sp = &S.tune; for (int j = 0; j < 13; ++j) Sp[j] += k * (Tp[j] - Sp[j]);
            feS += dk * (src[11] - feS); leS += dk * (src[12] - leS); const double fe_ = feS, le_ = leS;   // the contours step every CTV samples: smoothed per sample (no zipper or click on fast settings)
            const double baseN = curN + S.tune; const double dr = drv * .12;
            double wn = rnd() * 2 - 1;
            if (pink) { double* b = pk; b[0] = .99886 * b[0] + wn * .0555179; b[1] = .99332 * b[1] + wn * .0750759; b[2] = .969 * b[2] + wn * .153852; b[3] = .8665 * b[3] + wn * .3104856; b[4] = .55 * b[4] + wn * .5329522; b[5] = -.7616 * b[5] - wn * .016898; const double pinkv = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + wn * .5362) * .11; b[6] = wn * .115926; wn = pinkv; }
            const double modSrc = ((1 - S.mm) * o3last + S.mm * wn) * S.ma;
            const double pmod = om ? modSrc * 2 : 0;
            const double f1 = 440 * std::pow(2.0, (baseN + pmod + drift[0] * dr * 10 - 69) / 12) * r1;
            const double f2 = 440 * std::pow(2.0, (baseN + S.o2 + pmod + drift[1] * dr * 10 - 69) / 12) * r2;
            const double f3 = 440 * std::pow(2.0, ((o3kb ? baseN : 48) + S.o3 + drift[2] * dr * 10 - 69) / 12) * r3;
            const double oct = (S.cut + 5) + S.camt * .5 * fe_ + kt * (baseN - 48) / 12 + (fm ? modSrc * 2 : 0);
            double fc = 30 * std::pow(2.0, oct); fc = std::min(fc, fsO * .18); double kc; const double gg = ladderG(fc, fsO, 4, &kc); const double res = S.emph / 10 * 4.15 * kc; const double zg = zdfG(fc, fsO);
            for (int q = 0; q < OS; ++q) {
                const double d1 = f1 / fsO, d2 = f2 / fsO, d3 = f3 / fsO;
                ph[0] += d1; if (ph[0] >= 1) ph[0] -= 1; ph[1] += d2; if (ph[1] >= 1) ph[1] -= 1; ph[2] += d3; if (ph[2] >= 1) ph[2] -= 1;
                const double v1 = osc(0, w1, d1, false), v2 = osc(1, w2, d2, false), v3 = osc(2, w3, d3, true); o3last = v3;
                const double mix = v1 * S.l1 + v2 * S.l2 + v3 * S.l3 + wn * S.ln * .8 + fb * S.le * 2.2;
                const double x = th(mix * .62) * 1.35;
                if (classicSound()) {
                    const double inp = th(x - res * s[3] + (rnd() - .5) * 2e-4);
                    const double t0 = th(s[0]), t1 = th(s[1]), t2 = th(s[2]), t3 = th(s[3]);
                    s[0] += gg * (inp - t0); s[1] += gg * (t0 - t1); s[2] += gg * (t1 - t2); s[3] += gg * (t2 - t3);
                    dec.push(s[3]);
                } else dec.push(zdfLadder<4>(zs, s, x + (rnd() - .5) * 2e-4, zg, S.emph / 10 * 4.15, [](double u) { return th(u); }));
            }
            double out = dec.out() * le_ * 1.6; fb = out;
            ffbMix += .002 * ((ffbOn ? 1 : 0) - ffbMix);
            if (ffbMix > 1e-4) out = out * (1 - ffbMix) + ffb.run(out, lvB) * ffbMix;
            double sl = out, sr = out;
            ms += .004 * (mt - ms); sl *= ms; sr *= ms;
            const double yl = sl - xl + .9995 * dl; xl = sl; dl = yl; const double yr = sr - xr + .9995 * dr2; xr = sr; dr2 = yr;
            L[i] = (float) std::tanh(yl); R[i] = (float) std::tanh(yr);
        }
    }
};

} // namespace oct
