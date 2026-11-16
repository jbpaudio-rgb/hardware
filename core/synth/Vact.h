// VACT: West Coast complex voice (complex oscillator, 4 low-pass gates, 4 function generators, frequency shifter,
// spectral processor). Port of web/vact-web/vact.html (WORKLET), line for line.
#pragma once
#include "EngineBase.h"
#include "VactParams.h"

namespace oct {

struct Vact : EngineBase {
    static constexpr int OS = 4, CTV = 16;
    double foldX = 0;   // the folder's last input (ADAA)
    static double fgT(double k) { return .001 * std::pow(1e4, k); }
    static double glideS(double k) { return k <= 0 ? 0 : .002 * std::pow(1000.0, k); }
    static double modFreqHz(double k) { return .1 * std::pow(5e4, k); }
    static double modRatio(double k) { return std::pow(2.0, (k - .5) * 8); }
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
    struct FG {
        double fs = 48000, val = 0; int st = 0;
        void trig() { st = 1; }
        bool tick(int n, double att, double dec, int mode, bool gate) {
            bool eoa = false; if (mode == 2 && st == 0) st = 1; const double up = n / (fgT(att) * fs), dn = n / (fgT(dec) * fs);
            if (st == 1) { val += up; if (val >= 1) { val = 1; eoa = true; st = (mode == 1 && gate) ? 2 : 3; } }
            else if (st == 2) { if (!gate || mode != 1) st = 3; }
            else if (st == 3) { val -= dn; if (val <= 0) { val = 0; st = mode == 2 ? 1 : 0; } }
            return eoa;
        }
    };
    struct FS {   // frequency shifter
        double fs = 48000, ph = 0, d1 = 0; float st[32] {};
        double ap(double x, double a2, int o) { float* s = st; const double y = a2 * (x + s[o + 3]) - s[o + 1]; s[o + 1] = s[o]; s[o] = (float) x; s[o + 3] = s[o + 2]; s[o + 2] = (float) y; return y; }
        double run(double x, double hz, double bl) {
            static const double A[4] = { .6923878, .9360654322959, .988229522686, .9987488452737 }, B[4] = { .4021921162426, .856171088242, .9722909545651, .9952884791278 };
            double i = x, q = x; for (int k = 0; k < 4; ++k) i = ap(i, A[k] * A[k], k * 4); for (int k = 0; k < 4; ++k) q = ap(q, B[k] * B[k], 16 + k * 4); const double qd = d1; d1 = q;
            ph += hz / fs; ph -= std::floor(ph); const double c = std::cos(TP * ph), s = std::sin(TP * ph); const double up = i * c - qd * s, dn = i * c + qd * s, ri = i * c;
            return bl >= 0 ? ri + (up - ri) * bl : ri + (dn - ri) * (-bl);
        }
    };
    struct SPEC {   // spectral processor: 16 bands, even / odd
        double fs = 48000, ms = 0; double gb[16] {}, kb[16] {}; float s1[32] {}, s2[32] {}, env[16] {}, yb[2][16] {};
        void init(double sr) {
            fs = sr; const double fc[16] = { 100, 150, 250, 350, 500, 630, 800, 1000, 1300, 1600, 2000, 2600, 3500, 5000, 8000, 10000 };
            for (int b = 0; b < 16; ++b) { gb[b] = std::tan(PI * std::min(fc[b], fs * .45) / fs); kb[b] = b == 0 || b == 15 ? 1.2 : .28; }
            for (auto& x : s1) x = 0; for (auto& x : s2) x = 0; for (auto& x : env) x = 0; for (auto& r : yb) for (auto& x : r) x = 0; ms = 0;
        }
        void run(double* io, const float* lv, double cen, double wid, bool oe, bool eo, int em, double pe, double po, int os, int mode, bool useE, double eIn, double mix, int st) {
            ms += .002 * (mix - ms); if (ms < 1e-4 && mix <= 0) return;
            const double aU = 1 - std::exp(-1 / ((em == 2 ? .18 : .004) * fs)), aD = 1 - std::exp(-1 / ((em == 0 ? .004 : .18) * fs)); const double c = cen * 15, sg = .35 + wid * 7; const double spread = st == 0 ? 0 : st == 1 ? .5 : 1;
            for (int ch = 0; ch < 2; ++ch) {
                float* Y = yb[ch];
                for (int b = 0; b < 16; ++b) {
                    const double x = (useE && b % 2 == 1) ? eIn : io[ch]; const double g = gb[b], k = kb[b]; const int j = ch * 16 + b; const double hp = (x - (k + g) * s1[j] - s2[j]) / (1 + k * g + g * g);
                    const double bp = g * hp + s1[j]; s1[j] = (float) (g * hp + bp); const double lp = g * bp + s2[j]; s2[j] = (float) (g * bp + lp); Y[b] = (float) (b == 0 ? lp : b == 15 ? hp : bp * k * 2);
                }
            }
            double o[2] = { 0, 0 };
            for (int b = 0; b < 16; ++b) { const double m = std::fabs(.5 * ((double) yb[0][b] + yb[1][b])) * (1 + 15 * (b % 2 == 0 ? po : pe)); env[b] = (float) (env[b] + (m > env[b] ? aU : aD) * (m - env[b])); }
            for (int b = 0; b < 16; ++b) {
                const bool odd = b % 2 == 0; if (os == 1 && !odd) continue; if (os == 2 && odd) continue; double g = mode == 0 ? 1 : lv[b];
                if (mode == 2) { if (wid < .999) { const double d = (b - c) / sg; g *= std::exp(-.5 * d * d); } if (oe && !odd) g *= std::min(1.5, (double) env[b - 1]); if (eo && odd) g *= std::min(1.5, (double) env[std::min(b + 1, 15)]); }
                const double gl = odd ? 1 : 1 - spread, gr = odd ? 1 - spread : 1; o[0] += yb[0][b] * g * gl; o[1] += yb[1][b] * g * gr;
            }
            io[0] = io[0] * (1 - ms) + o[0] * 1.6 * ms; io[1] = io[1] * (1 - ms) + o[1] * 1.6 * ms;
        }
    };

    // state
    LPG lpg[4]; FG fg[4]; Dec dec[4]; FS fsh; SPEC spec; double io[2] {}, shS = 0, blS = 1; float lvB[16] {};
    double phM = 0, phP = 0; int cc = 0; std::vector<int> held; bool gate = false, edge = false; double vel = .8; int note = 60; double cur = 220, tgt = 220;
    float src[21] {}; double lp[4] { 0, .33, .66, .5 }, lr[4] {}, sh2p = 0, sh2t = 0, sh2v = 0, shp = 0, sht = 0, shv = 0, clock = 0; int stp = 0;
    struct TV { double mf = 0, mfi = 0, pi = 0, ix = 0, tb = 0, sy = 0, or_ = 0, wv = 0; } T, S; bool hasS = false;
    double lv[4] {}, pn[4] {}, ms = 0, dl = 0, dr = 0, xl = 0, xr = 0;
    int mw = 0, md = 0, fss = 0, spe = 0, spo = 0, spmd = 0, spei = 0, spst = 0; bool mk = false, pl = false, spoe = false, speo = false;
    int ls[4] {}, lm[4] {}; double lc[4] {}, ll[4] {}, lpn[4] {}, mt = 0, shT = 0, blT = 0, spc = 0, spw = 0, spm = 0, sppe = 0, sppo = 0;
    // control indices
    int iF[4][4] {}, iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {}, iG[4][7] {}, iSp[16] {};

    Vact() {
        setTable(vact::PDEFS, vact::DSTP, vact::NP);
        for (int i = 0; i < 4; ++i) { const std::string f = "f" + std::to_string(i); iF[i][0] = id(f + "mode"); iF[i][1] = id(f + "trig"); iF[i][2] = id(f + "att"); iF[i][3] = id(f + "dec"); }
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string s = "m" + std::to_string(m); iM[m][0] = id(s + "src"); iM[m][1] = id(s + "dst"); iM[m][2] = id(s + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        const char* GN[7] = { "src", "mode", "cv", "off", "amt", "lvl", "pan" };
        for (int c = 0; c < 4; ++c) for (int j = 0; j < 7; ++j) iG[c][j] = id("g" + std::to_string(c) + GN[j]);
        for (int b = 0; b < 16; ++b) iSp[b] = id("sp" + std::to_string(b));
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; for (auto& l : lpg) l = LPG(), l.fs = fs; for (auto& f : fg) f = FG(), f.fs = fs; for (auto& d : dec) d = Dec(); fsh = FS(); fsh.fs = fs; spec.init(fs);
        io[0] = io[1] = 0; shS = 0; blS = 1; for (auto& x : lvB) x = 0; phM = phP = 0; cc = 0; held.clear(); gate = edge = false; vel = .8; note = 60; cur = tgt = 220;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; } sh2p = sh2t = sh2v = shp = sht = shv = clock = 0; stp = 0;
        hasS = false; for (int c = 0; c < 4; ++c) lv[c] = pn[c] = 0; ms = dl = dr = xl = xr = 0; rs = 1234567;
    }
    int step() const override { return stp; }
    int meters(float* m) override { for (int i = 0; i < 4; ++i) { m[i] = (float) fg[i].val; m[4 + i] = (float) lpg[i].v; } return 8; }
    void noteOn(int n, double v) override { held.push_back(n); note = n; tgt = 440 * std::pow(2.0, (n - 69) / 12.0); vel = v; gate = true; edge = true; }
    void noteOff(int n) override {
        held.erase(std::remove(held.begin(), held.end(), n), held.end());
        if (held.empty()) gate = false; else { note = held.back(); tgt = 440 * std::pow(2.0, (note - 69) / 12.0); }
    }
    void allOff() override { held.clear(); gate = false; }

    void ctrl() {
        using namespace vact; const double dt = CTV / fs;
        clock = advance(clock, g(bpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool stepEdge = ns != stp; stp = ns; const bool half = std::fmod(clock, 1.0) < .5;
        const bool trig = edge; edge = false; const bool q[2] = { g(quadab) > .5, g(quadcd) > .5 }; bool eoa[4] = { false, false, false, false };
        for (int i = 0; i < 4; ++i) {
            const int mode = ri(iF[i][0]); const bool second = i % 2 == 1; const int ts = ri(iF[i][1]);
            bool fire = trig, gt = gate;
            if (ts > 0) { const float sv = p[(size_t) iS[ts - 1][stp]]; fire = stepEdge && sv != 0; gt = sv != 0 && half; }
            if (q[i >> 1] && second) { if (eoa[i - 1]) fg[i].trig(); } else if (fire && mode != 2) fg[i].trig();
            eoa[i] = fg[i].tick(CTV, val(iF[i][2]), val(iF[i][3]), mode, gt);
        }
        float* s = src;
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph = lp[i];
            const double W[5] = { std::sin(TP * ph), 1 - 4 * std::fabs(ph - .5), 2 * ph - 1, ph < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 19] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; } const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[20] = (float) sh2v;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        for (int i = 0; i < 4; ++i) s[11 + i] = (float) fg[i].val; s[15] = std::max(s[11], s[12]); s[16] = std::max(s[13], s[14]); s[17] = (float) vel; s[18] = (float) ((note - 60) / 24.0); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        T.mf = val(mfreq); T.mfi = val(mfine); T.pi = (val(ppitch) + bend) + val(pfine); T.ix = val(modidx); T.tb = val(timbre); T.sy = val(sym); T.or_ = val(order); T.wv = val(pwave);
        mw = ri(mwave); mk = g(mkbd) > .5; md = ri(moddest); pl = g(plock) > .5;
        for (int c = 0; c < 4; ++c) {
            ls[c] = ri(iG[c][0]); lm[c] = ri(iG[c][1]); const int cv = ri(iG[c][2]);
            const double cvv = cv < 4 ? fg[cv].val : cv == 4 ? (double) s[15] : cv == 5 ? (double) s[16] : 0; lc[c] = val(iG[c][3]) + cvv * g(iG[c][4]); ll[c] = val(iG[c][5]); lpn[c] = val(iG[c][6]);
        }
        mt = val(master);
        fss = ri(fssrc); const double sv = val(fsshift); shT = (sv < 0 ? -1 : 1) * 1000 * sv * sv + g(fsfine) * 5; blT = val(fsblend);
        for (int b = 0; b < 16; ++b) lvB[b] = p[(size_t) iSp[b]]; spc = val(spfreq); spw = val(spwidth); spm = val(spmix); spoe = g(sptrans) > .5; speo = g(sptreo) > .5; spe = ri(spresp); spo = ri(spout);
        spmd = ri(spmode); spei = ri(spevenin); sppe = g(sppreeven); sppo = g(sppreodd); spst = ri(spstereo);
        const double gs = glideS(g(glide)); cur += (gs <= 0 ? 1 : 1 - std::exp(-dt / gs)) * (tgt - cur);
        if (!hasS) { S = T; hasS = true; }
    }

    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (cc == 0) ctrl(); cc = (cc + 1) % CTV;
            const double k = .004; S.mf += k * (T.mf - S.mf); S.mfi += k * (T.mfi - S.mfi); S.pi += k * (T.pi - S.pi); S.ix += k * (T.ix - S.ix); S.tb += k * (T.tb - S.tb); S.sy += k * (T.sy - S.sy); S.or_ += k * (T.or_ - S.or_); S.wv += k * (T.wv - S.wv);
            const double fP = cur * std::pow(2.0, S.pi / 12); double fM = mk ? cur * modRatio(S.mf) * std::pow(2.0, S.mfi / 12) : modFreqHz(S.mf) * std::pow(2.0, S.mfi / 12); fM = std::min(fM, fsO * .2);
            for (int q = 0; q < OS; ++q) {
                const double dM = fM / fsO; phM += dM; bool wr = false; if (phM >= 1) { phM -= 1; wr = true; } const double pm = phM; double m;
                if (mw == 0) m = 2 * pm - 1 - blep(pm, dM); else if (mw == 1) { double t2 = pm + .5; if (t2 >= 1) t2 -= 1; m = (pm < .5 ? 1 : -1) + blep(pm, dM) - blep(t2, dM); } else m = 1 - 4 * std::fabs(pm - .5);
                double fI = fP, tb = S.tb, am = 1; if (md == 1) fI = fP * (1 + S.ix * 3 * m); else if (md == 0) am = 1 - std::fabs(S.ix) * .5 + S.ix * .5 * m; else tb = cl(S.tb + S.ix * .6 * m, 0, 1);
                const double dP = fI / fsO; phP += dP; phP -= std::floor(phP); if (pl && wr) phP = 0; const double ph = phP, ad = std::fabs(dP);
                const double sine = std::sin(TP * ph); double t2 = ph + .5; if (t2 >= 1) t2 -= 1; const double sq = (ph < .5 ? 1 : -1) + blep(ph, ad) - blep(t2, ad);
                const double saw = 2 * ph - 1 - blep(ph, ad); const double tri = 4 * std::fabs(std::fmod(ph + .75, 1.0) - .5) - 1; const double w3 = S.wv * 3; const int wi = std::min(2, (int) std::floor(w3)); const double wf = w3 - wi;
                const double WV[4] = { sine, tri, saw, sq }; const double base = WV[wi] + (WV[wi + 1] - WV[wi]) * wf;
                const double x = base * (1 + tb * 7) + S.sy * .9 * (.3 + tb);   // the folder, antialiased (first-order ADAA)
                const double dx = x - foldX; const double folded = classicSound() ? std::sin(PI * .5 * x) : std::fabs(dx) > 1e-6 ? (std::cos(PI * .5 * foldX) - std::cos(PI * .5 * x)) / (PI * .5 * dx) : std::sin(PI * .25 * (x + foldX)); foldX = x;
                const double mS = 2 * std::fabs(std::sin(TP * ph)) - 1; const double s2 = .5 * (1 - S.sy) * saw + .5 * (1 + S.sy) * mS; const double alt = (1 - tb) * base + tb * s2;
                const double fin = std::cos(S.or_ * PI * .5) * folded + std::sin(S.or_ * PI * .5) * alt;
                dec[0].push(fin * am); dec[1].push(sine * am); dec[2].push(sq * .7 * am); dec[3].push(m);
            }
            bool need[5] = { false, false, false, false, false }; for (int c = 0; c < 4; ++c) if (ll[c] > 0 || lv[c] > 1e-4) need[ls[c]] = true; if (spei == 2 && spm > 0) need[4] = true; if (need[4]) need[fss] = true;
            double v[5] = { need[0] ? dec[0].out() : 0, need[1] ? dec[1].out() : 0, need[2] ? dec[2].out() : 0, need[3] ? dec[3].out() * .8 : 0, 0 };
            shS += .002 * (shT - shS); blS += .004 * (blT - blS); if (need[4]) v[4] = fsh.run(v[fss], shS, blS);
            double sl = 0, sr = 0;
            for (int c = 0; c < 4; ++c) {
                lv[c] += .004 * (ll[c] - lv[c]); pn[c] += .004 * (lpn[c] - pn[c]);
                if (lv[c] < 1e-4 && ll[c] <= 0) { lpg[c].run(0, lc[c], lm[c]); continue; }
                const double y = lpg[c].run(v[ls[c]], lc[c], lm[c]) * lv[c]; const double pa = (pn[c] + 1) * .25 * PI; sl += y * std::cos(pa); sr += y * std::sin(pa);
            }
            const double eIn = spei == 1 ? (rnd() * 2 - 1) * .5 : spei == 2 ? v[4] : 0;
            io[0] = sl; io[1] = sr; spec.run(io, lvB, spc, spw, spoe, speo, spe, sppe, sppo, spo, spmd, spei != 0, eIn, spm, spst); sl = io[0]; sr = io[1];
            ms += .004 * (mt - ms); sl *= ms * 1.2; sr *= ms * 1.2;
            const double yl = sl - xl + .9995 * dl; xl = sl; dl = yl; const double yr = sr - xr + .9995 * dr; xr = sr; dr = yr;
            L[i] = (float) std::tanh(yl); R[i] = (float) std::tanh(yr);
        }
    }
};

} // namespace oct
