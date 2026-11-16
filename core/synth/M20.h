// M20: two-VCO monosynth after the Korg MS-20 (screaming 12 dB high-pass and low-pass, EG1/EG2, MG, S&H).
// Port of web/ms-web/m20.html (WORKLET), line for line.
#pragma once
#include "EngineBase.h"
#include "M20Params.h"
#include <algorithm>

namespace oct {

struct M20 : EngineBase {
    static constexpr int OS = 2, CTV = 16;
    static double etime(double k) { return .0015 * std::pow(6000.0, k / 10); }
    static double mghz(double k) { return .1 * std::pow(500.0, k / 10); }
    static double cutHz(double k) { return 20 * std::pow(1000.0, k / 10); }
    static double hcutHz(double k) { return 10 * std::pow(800.0, k / 10); }
    static double portaS(double k) { return k <= 0 ? 0 : .003 * std::pow(1500.0, k / 10); }
    static double th(double x) { if (x > 3) return 1; if (x < -3) return -1; const double x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
    static double blep(double t, double dt) { return oct::blepAny(t, dt); }

    struct Dec {
        static constexpr int N = 32; float h[N] {}, b[2 * N] {}; int p = 0;
        Dec() {
            const double fc = .42 / OS; double s = 0;
            for (int i = 0; i < N; ++i) { const double m = i - (N - 1) / 2.0; double v = m == 0 ? 2 * fc : std::sin(TP * fc * m) / (PI * m); v *= .42 - .5 * std::cos(TP * i / (N - 1)) + .08 * std::cos(2 * TP * i / (N - 1)); h[i] = (float) v; s += v; }
            for (int i = 0; i < N; ++i) h[i] = (float) (h[i] / s);
        }
        void push(double x) { b[p] = b[p + N] = (float) x; p = (p + 1) % N; }
        double out() const { double a = 0; for (int i = 0; i < N; ++i) a += (double) h[i] * b[p + i]; return a; }
    };
    struct Scream {
        double s1 = 0, s2 = 0, hp = 0;
        double step(double x, double g, double k) { hp = (x - (k + g) * s1 - s2) / (1 + g * (g + k)); const double bp = g * hp + s1; s1 = th(g * hp + bp); const double lp = g * bp + s2; s2 = g * bp + lp; return lp; }
    };

    double ph[2] {}; Scream hpf, lpf; Dec dec;
    double eg1 = 0, eg1t = 0, eg2 = 0, holdT = 0, mgp = 0, shv = 0; int eg1s = 0, eg2s = 0;
    std::vector<int> held; bool gate = false, edge = false, rel = false, seqGate = false; double vel = .8, cur = 60; int tgt = 60;
    float src[20] {}; double lp[4] {}, lr[4] {}, mshp = 0, msht = 0, mshv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0; int stp = 0;
    double ms = 0, dl = 0, xl = 0, ampS = 0; long mc = 0;
    double mgTri = 0, mgSq = 0, semi = 0, hg = 0, hk = 0, lg = 0, lk = 0, amp = 0, mt = 0; int w1 = 0, w2 = 0, sc1 = 0, sc2 = 0;
    struct TV { double v1pw, v1l, v2p, v2l, drive; } T {}, SM {}; bool hasSM = false;
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {};

    M20() {
        setTable(m20::PDEFS, m20::DSTP, m20::NP);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 271828182; ph[0] = 0; ph[1] = .5; hpf = Scream(); lpf = Scream(); dec = Dec();
        eg1 = eg1t = eg2 = holdT = mgp = shv = 0; eg1s = eg2s = 0;
        held.clear(); gate = edge = rel = seqGate = false; vel = .8; cur = 60; tgt = 60;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        mshp = msht = mshv = sh2p = sh2t = sh2v = clock = 0; stp = 0; ms = dl = xl = ampS = 0; mc = 0; hasSM = false;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = (float) eg1; m[1] = (float) eg2; return 2; }
    int maxHeld() const { return *std::max_element(held.begin(), held.end()); }
    void noteOn(int n, double v) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); held.push_back(n); vel = v; tgt = maxHeld(); edge = true; gate = true; }
    void noteOff(int n) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); if (held.empty()) { gate = false; rel = true; } else tgt = maxHeld(); }
    void allOff() override { held.clear(); gate = false; rel = true; }

    void ctrl() {
        const double dt = CTV / fs; float* s = src;
        clock = advance(clock, g(m20::bpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool edg = ns != stp; stp = ns; const bool half = std::fmod(clock, 1.0) < .5;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        const int ts = ri(m20::trig); bool fire = edge, release = rel, gt = gate; edge = false; rel = false;
        if (ts > 0) { const float sv = p[(size_t) iS[ts - 1][stp]]; const bool on = sv != 0 && half; fire = edg && sv != 0; release = seqGate && !on; seqGate = on; gt = on; }
        if (fire) { eg1s = 1; eg1t = 0; eg2s = 1; if (ri(m20::shclk) == 1) shv = rnd() * 2 - 1; }
        if (release) holdT = etime(val(m20::e2h)) * (val(m20::e2h) > 0 ? 1 : 0);
        if (!gt && holdT > 0) holdT -= dt; const bool g2 = gt || holdT > 0;
        eg1t += dt;
        if (eg1s == 1) { if (eg1t >= etime(val(m20::e1d)) * (val(m20::e1d) > 0 ? 1 : 0)) eg1s = 2; }
        else if (eg1s == 2) {   // EG1's attack: a capacitor charging (a curve, fast then slower), as EG2's; classic: a straight ramp
            if (classicSound()) eg1 += dt / etime(val(m20::e1a)); else eg1 += (1.3 - eg1) * (1 - std::exp(-dt / (etime(val(m20::e1a)) / 1.2)));
            if (eg1 >= 1) { eg1 = 1; eg1s = 3; } }
        if (!gt && eg1s > 0 && eg1s < 4) eg1s = 4;
        if (eg1s == 4) { eg1 += (0 - eg1) * (1 - std::exp(-dt / (etime(val(m20::e1r)) / 4))); if (eg1 < 1e-4) { eg1 = 0; eg1s = 0; } }
        if (eg2s == 1) { eg2 += (1.3 - eg2) * (1 - std::exp(-dt / (etime(val(m20::e2a)) / 1.2))); if (eg2 >= 1) { eg2 = 1; eg2s = 2; } }
        else if (eg2s == 2) eg2 += (val(m20::e2s) / 10 - eg2) * (1 - std::exp(-dt / (etime(val(m20::e2d)) / 4)));
        if (!g2 && (eg2s == 1 || eg2s == 2)) eg2s = 3;
        if (eg2s == 3) { eg2 += (0 - eg2) * (1 - std::exp(-dt / (etime(val(m20::e2r)) / 4))); if (eg2 < 1e-5) { eg2 = 0; eg2s = 0; } }
        mgp += mghz(val(m20::mgfreq)) * dt; bool wrap = false; if (mgp >= 1) { mgp -= 1; wrap = true; }
        const double pk = cl(val(m20::mgshape) / 10, .001, .999), mp = mgp; mgTri = (mp < pk ? mp / pk : 1 - (mp - pk) / (1 - pk)) * 2 - 1; mgSq = mp < .5 ? 1 : -1;
        if (wrap && ri(m20::shclk) == 0) shv = rnd() * 2 - 1;
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph_ = lp[i];
            const double W[5] = { std::sin(TP * ph_), 1 - 4 * std::fabs(ph_ - .5), 2 * ph_ - 1, ph_ < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 15] = (float) W[ri(iL[i][1])];
        }
        mshp += lfoHz(val(m20::shrate)) * dt; if (mshp >= 1) { mshp -= 1; msht = rnd() * 2 - 1; }
        const double sm = g(m20::shsm); mshv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (msht - mshv); s[4] = (float) mshv;
        sh2p += lfoHz(val(m20::sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = g(m20::sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[16] = (float) sh2v;
        s[11] = (float) eg1; s[12] = (float) eg2; s[13] = (float) vel; s[14] = (float) ((tgt - 60) / 24.0); s[17] = (float) mgTri; s[18] = (float) mgSq; s[19] = (float) shv; s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        const double ps = portaS(val(m20::porta)); cur += (ps <= 0 ? 1 : 1 - std::exp(-dt / ps)) * (tgt - cur);
        const double fsO = fs * OS, mg = mgTri;
        semi = cur + (val(m20::tune) + bend) + val(m20::fmmg) / 10 * 12 * mg + val(m20::fmeg) / 5 * 24 * eg1;
        const double hfc = std::min(hcutHz(val(m20::hpf)) * std::pow(2.0, val(m20::hmg) / 10 * 3 * mg + val(m20::heg) / 5 * 5 * eg2), fsO * .22);
        const double lfc = std::min(cutHz(val(m20::lpf)) * std::pow(2.0, val(m20::lmg) / 10 * 3 * mg + val(m20::leg) / 5 * 5 * eg2 + (tgt - 60) / 24.0), fsO * .22);
        hg = std::tan(PI * hfc / fsO); hk = 2 * (1 - val(m20::hpk) / 10 * 1.03); lg = std::tan(PI * lfc / fsO); lk = 2 * (1 - val(m20::lpk) / 10 * 1.03);
        T.v1pw = val(m20::v1pw); T.v1l = val(m20::v1lvl) / 10; T.v2p = val(m20::v2pitch); T.v2l = val(m20::v2lvl) / 10; T.drive = val(m20::drive) / 10;
        static const int SC[4] = { -24, -12, 0, 12 };
        w1 = ri(m20::v1wave); w2 = ri(m20::v2wave); sc1 = SC[ri(m20::v1scale)]; sc2 = SC[ri(m20::v2scale)];
        amp = eg2; mt = val(m20::master);
        if (!hasSM) { SM = T; hasSM = true; }
    }

    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const double k = .003; double* Tp = &T.v1pw; double* Sp = &SM.v1pw; for (int j = 0; j < 5; ++j) Sp[j] += k * (Tp[j] - Sp[j]);
            ampS = ampS + .02 * (amp - ampS);
            const double f1 = 440 * std::pow(2.0, (semi + sc1 + vOsc[0] - 69) / 12), f2 = 440 * std::pow(2.0, (semi + sc2 + SM.v2p + vOsc[1] - 69) / 12), d1 = f1 / fsO, d2 = f2 / fsO;
            const double pw1 = .5 - SM.v1pw / 10 * .45;
            for (int q = 0; q < OS; ++q) {
                ph[0] += d1; if (ph[0] >= 1) ph[0] -= 1; ph[1] += d2; if (ph[1] >= 1) ph[1] -= 1; const double p1 = ph[0], p2 = ph[1];
                double v1;
                if (w1 == 0) v1 = 1 - 4 * std::fabs(p1 - .5); else if (w1 == 1) v1 = 2 * p1 - 1 - blep(p1, d1);
                else if (w1 == 2) { double t = p1 - pw1; if (t < 0) t += 1; v1 = (p1 < pw1 ? 1 : -1) + blep(p1, d1) - blep(t, d1); } else v1 = rnd() * 2 - 1;
                double t2 = p2 - .5; if (t2 < 0) t2 += 1; const double sq2 = (p2 < .5 ? 1 : -1) + blep(p2, d2) - blep(t2, d2); double v2;
                if (w2 == 0) v2 = 2 * p2 - 1 - blep(p2, d2); else if (w2 == 1) v2 = sq2; else if (w2 == 2) { double t = p2 - .12; if (t < 0) t += 1; v2 = (p2 < .12 ? 1 : -1) + blep(p2, d2) - blep(t, d2); } else v2 = v1 * sq2;
                const double x = th((v1 * SM.v1l + v2 * SM.v2l) * (.5 + SM.drive * 2.2)) * .8 + (rnd() - .5) * 2e-4;
                hpf.step(x, hg, hk); const double lpo = lpf.step(hpf.hp, lg, lk);
                dec.push(lpo * ampS);
            }
            double y = dec.out() * 1.6;
            ms += .004 * (mt - ms); y *= ms;
            const double yy = y - xl + .9995 * dl; xl = y; dl = yy; L[i] = R[i] = (float) std::tanh(yy);
        }
    }
};

} // namespace oct
