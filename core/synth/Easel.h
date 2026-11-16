// EASEL: West Coast synth after the Buchla Music Easel (complex oscillator with folding, pulser, sequential voltage source,
// envelope, two vactrol low pass gates, spring). Port of web/easel-web/easel.html (WORKLET), line for line.
#pragma once
#include "EngineBase.h"
#include "EaselParams.h"
#include <algorithm>

namespace oct {

struct Easel : EngineBase {
    static constexpr int OS = 8, CTV = 16;
    double foldX = 0;   // the folder's last input (ADAA)
    static double fgT(double k) { return .001 * std::pow(1e4, k); }
    static double glideS(double k) { return k <= 0 ? 0 : .002 * std::pow(1000.0, k); }
    static double perS(double k) { return .02 * std::pow(250.0, k); }
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
    struct LPG {
        double fs = 48000, v = 0, i1 = 0, i2 = 0;
        double run(double x, double c, int mode) {
            c = cl(c, 0, 1); const double v0 = v; const double tau = c > v0 ? .0016 + .004 * (1 - c) : .03 + .55 * (1 - v0) * (1 - v0);
            v += (1 - std::exp(-1 / (tau * fs))) * (c - v0); const double w = v; const double g = w * w * (3 - 2 * w) * .6 + w * .4; double o = x;
            if (mode >= 1) {
                const double fc = std::min(18 * std::pow(1100.0, w), fs * .45); const double gk = std::tan(PI * fc / fs), k = 1.25;
                const double a1 = 1 / (1 + gk * (gk + k)), a2 = gk * a1, a3 = gk * a2; const double xi = std::tanh(x * 1.1); const double v3 = xi - i2; const double v1 = a1 * i1 + a2 * v3; const double v2 = i2 + a2 * i1 + a3 * v3;
                i1 = 2 * v1 - i1; i2 = 2 * v2 - i2; o = v2;
            }
            if (mode == 0) o = x * g; else if (mode == 1) o *= g; return o;
        }
    };
    struct Spring {
        struct AP { std::vector<float> d; int i = 0; }; AP ap[6]; std::vector<float> dl[2]; int di[2] {}; double lp[2] {};
        void init(double fs) {
            for (int k = 0; k < 6; ++k) { ap[k].d.assign((size_t) iround(fs * (.0011 + .0007 * k)), 0.f); ap[k].i = 0; }
            dl[0].assign((size_t) iround(fs * .0371), 0.f); dl[1].assign((size_t) iround(fs * .0433), 0.f); di[0] = di[1] = 0; lp[0] = lp[1] = 0;
        }
        void run(double x, double time, double* o) {
            double y = x; for (auto& a : ap) { const double b = a.d[(size_t) a.i]; const double v = y + .62 * b; a.d[(size_t) a.i] = (float) v; y = b - .62 * v; a.i = (a.i + 1) % (int) a.d.size(); }
            const double fb = .55 + .4 * time;
            for (int c = 0; c < 2; ++c) { auto& d = dl[c]; const int j = di[c]; const double r = d[(size_t) j]; lp[c] += .35 * (r - lp[c]); d[(size_t) j] = (float) (y * .5 + lp[c] * fb * (c ? -1 : 1)); di[c] = (j + 1) % (int) d.size(); o[c] = r; }
        }
    };

    LPG lpg[2]; Dec dec[2]; Spring sp;
    double phM = 0, phP = 0; int cc = 0; std::vector<int> held; bool gate = false, edge = false; double vel = .8; int note = 60; double cur = 261.63, tgt = 261.63; int ai = 0;
    double pph = 1; bool prun = true; double pv = 0; int stage = 0; double sv = 0, ev = 0; int est = 0; double et = 0; bool eEnd = false;
    float src[18] {}; double lp[4] {}, lr[4] {}, sh2p = 0, sh2t = 0, sh2v = 0, shp = 0, sht = 0, shv = 0, clock = 0; int stp = 0;
    double lv[2] {}, pn[2] {}, ms = 0, dl = 0, dr = 0, xl = 0, xr = 0, rv = 0;
    struct TV { double fp, fm, ix, tb; } T {}, SM {}; bool hasSM = false;
    int mw = 0, md = 0, pw = 0, g2 = 0, lm[2] {}; double lc[2] {}, ll[2] {}, lpn[2] {}, mt = 0, rvT = 0, rtm = 0;
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {};

    Easel() {
        setTable(easel::PDEFS, easel::DSTP, easel::NP);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; for (auto& l : lpg) { l = LPG(); l.fs = fs; } dec[0] = Dec(); dec[1] = Dec(); sp.init(fs);
        phM = phP = 0; cc = 0; held.clear(); gate = edge = false; vel = .8; note = 60; cur = tgt = 261.63; ai = 0;
        pph = 1; prun = true; pv = 0; stage = 0; sv = ev = 0; est = 0; et = 0; eEnd = false;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        sh2p = sh2t = sh2v = shp = sht = shv = clock = 0; stp = 0; hasSM = false;
        lv[0] = lv[1] = pn[0] = pn[1] = 0; ms = dl = dr = xl = xr = 0; rs = 7654321; rv = 0;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = (float) ev; m[1] = (float) pv; m[2] = (float) stage; m[3] = (float) lpg[0].v; m[4] = (float) lpg[1].v; return 5; }
    static double hz(int n) { return 440 * std::pow(2.0, (n - 69) / 12.0); }
    void noteOn(int n, double v) override { held.push_back(n); note = n; tgt = hz(n); vel = v; gate = true; edge = true; }
    void noteOff(int n) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); if (held.empty()) gate = false; else { note = held.back(); tgt = hz(note); } }
    void allOff() override { held.clear(); gate = false; }
    double cv(int sel) const { return sel == 1 ? sv : sel == 2 ? pv : sel == 3 ? ev : sel == 4 ? vel : 0; }

    void ctrl() {
        using namespace easel; const double dt = CTV / fs;
        clock = advance(clock, g(bpm), dt); stp = ((int) std::floor(clock)) & 15;
        const bool keyEdge = edge; edge = false;
        const int pm = ri(pmode); bool pfire = false;
        if (pm == 1 && keyEdge) { pph = 0; prun = true; pfire = true; }
        if (pm != 2 && (pm == 0 || prun)) {
            const double per = perS(val(prate)) * std::pow(2.0, -cv(ri(pucv)) * g(pucva) * 3);
            pph += dt / per; if (pph >= 1) { if (pm == 0) { pph -= std::floor(pph); pfire = true; } else { pph = 1; prun = false; } }
        }
        pv = pm == 2 ? 0 : 1 - std::min(1.0, pph);
        if (g(arp) > .5 && pfire && held.size() > 1) { ai = (ai + 1) % (int) held.size(); note = held[(size_t) ai]; tgt = hz(note); }
        const int adv = ri(vadv); const int nst = 3 + ri(vstages); bool sfire = false;
        if ((adv == 0 && pfire) || (adv == 1 && keyEdge) || (adv == 2 && eEnd)) { stage = (stage + 1) % nst; sfire = true; } if (stage >= nst) stage = 0;
        eEnd = false; sv = val(v0 + stage);
        const int em = ri(emode), etg = ri(etrig); const bool kp = g(kpulse) > .5;
        const bool efire = (etg == 0 && keyEdge && kp) || (etg == 1 && pfire) || (etg == 2 && sfire) || (etg == 3 && ((keyEdge && kp) || pfire));
        const bool egate = etg == 0 || etg == 3 ? gate : true;
        if (efire) est = 1; if (em == 2 && est == 0) est = 1;
        if (est == 1) { ev += dt / fgT(val(eatt)); if (ev >= 1) { ev = 1; est = 2; et = 0; } }
        else if (est == 2) { et += dt; if (em == 1 ? !egate : et >= fgT(val(esus))) est = 3; }
        else if (est == 3) { ev -= dt / fgT(val(edec)); if (ev <= 0) { ev = 0; est = 0; eEnd = true; } }
        float* s = src;
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph_ = lp[i];
            const double W[5] = { std::sin(TP * ph_), 1 - 4 * std::fabs(ph_ - .5), 2 * ph_ - 1, ph_ < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 16] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; } const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[17] = (float) sh2v;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        s[11] = (float) ev; s[12] = (float) pv; s[13] = (float) sv; s[14] = (float) vel; s[15] = (float) ((note - 60) / 24.0); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        const double kr = cur / 261.63;
        const double pcv_ = cv(ri(pucv)) * g(pucva) * 36; const double mcv_ = cv(ri(mcv)) * g(mcva) * 4;
        const double tcv_ = cv(ri(tcv)) * g(tcva); const double icv = cv(ri(ixcv)) * g(ixcva);
        const int mr = ri(mrange); const double mf = val(mfreq); double fM = mr == 0 ? .05 * std::pow(1000.0, mf) : 8 * std::pow(1000.0, mf);
        if (g(mkbd) > .5) fM *= kr; fM *= std::pow(2.0, mcv_);
        const double pk = g(pkbd) > .5 ? cur : 261.63;
        T.fp = pk * std::pow(2.0, (val(ppitch) + val(pfine) + (val(tune) + bend) + pcv_ + vOsc[0]) / 12); T.fm = fM * std::pow(2.0, vOsc[1] / 12); T.ix = cl(val(modidx) + icv, 0, 1); T.tb = cl(val(timbre) + tcv_, 0, 1);
        mw = ri(mwave); md = ri(modmode); pw = ri(pwave); g2 = ri(g1src);
        static const int LM[3] = { 2, 1, 0 };
        for (int c = 0; c < 2; ++c) {
            const int b = c ? g1mode - g0mode : 0;
            lm[c] = LM[ri(g0mode + b)]; const int cs = ri(g0cv + b);
            const double cvv = cs == 0 ? ev : cs == 1 ? pv : cs == 2 ? sv : cs == 3 ? vel * (gate ? 1 : 0) : 0;
            lc[c] = val(g0lvl + b) + cvv * g(g0amt + b); ll[c] = g(g0out + b); lpn[c] = val(g0pan + b);
        }
        mt = val(master); rvT = val(rev); rtm = g(revtime);
        const double gs = glideS(val(porta)); cur += (gs <= 0 ? 1 : 1 - std::exp(-dt / gs)) * (tgt - cur);
        if (!hasSM) { SM = T; hasSM = true; }
    }

    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (cc == 0) ctrl(); cc = (cc + 1) % CTV;
            const double k = .006; double* Tp = &T.fp; double* Sp = &SM.fp; for (int j = 0; j < 4; ++j) Sp[j] += k * (Tp[j] - Sp[j]);
            const double fM = std::min(SM.fm, fsO * .2);
            for (int q = 0; q < OS; ++q) {
                const double dM = fM / fsO; phM += dM; if (phM >= 1) phM -= 1; const double pm = phM; double m;
                if (mw == 0) m = 2 * pm - 1 - blep(pm, dM); else if (mw == 1) { double t2 = pm + .5; if (t2 >= 1) t2 -= 1; m = (pm < .5 ? 1 : -1) + blep(pm, dM) - blep(t2, dM); } else m = 1 - 4 * std::fabs(pm - .5);
                double fI = SM.fp, am = 1; if (md == 2) fI = SM.fp * (1 + SM.ix * 4 * m); else if (md == 1) am = 1 - SM.ix * .5 + SM.ix * .5 * m;
                const double dP = std::min(std::fabs(fI), fsO * .2) / fsO * (fI < 0 ? -1 : 1); phP += dP; phP -= std::floor(phP); const double ph_ = phP, ad = std::fabs(dP);
                double base; const double sn = std::sin(TP * ph_);
                if (pw == 0) base = sn * sn * sn * sn * sn * 1.6;
                else if (pw == 1) { double t2 = ph_ + .5; if (t2 >= 1) t2 -= 1; base = ((ph_ < .5 ? 1 : -1) + blep(ph_, ad) - blep(t2, ad)) * .8; }
                else base = 1 - 4 * std::fabs(std::fmod(ph_ + .25, 1.0) - .5);
                // the wavefolder, antialiased (first-order ADAA: the mean of the fold between this sample and the last)
                const double x = base * (1 + SM.tb * 6), fa = SM.tb * .35;
                auto F = [&](double u) { return -(1 - fa) * std::cos(PI * .5 * u) / (PI * .5) - fa * std::cos(PI * 1.5 * u) / (PI * 1.5); };
                const double dx = x - foldX; const double out = classicSound() ? (1 - fa) * std::sin(PI * .5 * x) + fa * std::sin(PI * 1.5 * x) : std::fabs(dx) > 1e-6 ? (F(x) - F(foldX)) / dx : (1 - fa) * std::sin(PI * .5 * (x + foldX) * .5) + fa * std::sin(PI * 1.5 * (x + foldX) * .5);
                foldX = x;
                dec[0].push(out * am); dec[1].push(m * .8);
            }
            const double comp = dec[0].out(), modo = dec[1].out(); const double in2 = g2 == 0 ? modo : g2 == 1 ? comp : (rnd() * 2 - 1) * .6;
            double sl = 0, sr = 0; const double ins[2] = { comp, in2 };
            for (int c = 0; c < 2; ++c) {
                lv[c] += .004 * (ll[c] - lv[c]); pn[c] += .004 * (lpn[c] - pn[c]);
                const double y = lpg[c].run(ins[c], lc[c], lm[c]) * lv[c]; const double pa = (pn[c] + 1) * .25 * PI; sl += y * std::cos(pa); sr += y * std::sin(pa);
            }
            rv += .004 * (rvT - rv); if (rv > 1e-4) { double w[2]; sp.run((sl + sr) * .5, rtm, w); sl += w[0] * rv * 1.4; sr += w[1] * rv * 1.4; }
            ms += .004 * (mt - ms); sl *= ms * 1.25; sr *= ms * 1.25;
            const double yl = sl - xl + .9995 * dl; xl = sl; dl = yl; const double yr = sr - xr + .9995 * dr; xr = sr; dr = yr;
            L[i] = (float) std::tanh(yl); R[i] = (float) std::tanh(yr);
        }
    }
};

} // namespace oct
