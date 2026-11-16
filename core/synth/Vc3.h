// VC3: pin-matrix synth after the EMS VCS3 (3 oscillators, noise, ring mod, filter, trapezoid envelope, spring, joystick).
// Port of web/vcs-web/vc3.html (WORKLET), line for line.
#pragma once
#include "LadderTune.h"
#include "EngineBase.h"
#include "Vc3Params.h"

namespace oct {

struct Vc3 : EngineBase {
    static constexpr int OS = 4, CTV = 16, NS = 15, ND = 14;
    static double o1hz(double k) { return 1 * std::pow(10000.0, k / 10); }
    static double o3hz(double k) { return .025 * std::pow(20000.0, k / 10); }
    static double fhz(double k) { return 10 * std::pow(1500.0, k / 10); }
    static double etime(double k) { return .002 * std::pow(5000.0, k / 10); }
    static double th(double x) { if (x > 3) return 1; if (x < -3) return -1; const double x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
    static double blep(double t, double dt) { return oct::blepAny(t, dt); }
    static double skew(double p, double k) { return p < k ? p / k * .5 : .5 + (p - k) / (1 - k) * .5; }

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
    struct SpringR {
        struct Buf { std::vector<float> b; int i = 0; double lp = 0; };
        Buf ap[4], cb[2];
        void init(double fs) {
            const double sc = fs / 48000; const int A[4] = { 113, 337, 617, 229 }, C[2] = { 1597, 1913 };
            for (int k = 0; k < 4; ++k) { ap[k].b.assign((size_t) iround(A[k] * sc), 0.f); ap[k].i = 0; }
            for (int k = 0; k < 2; ++k) { cb[k].b.assign((size_t) iround(C[k] * sc), 0.f); cb[k].i = 0; cb[k].lp = 0; }
        }
        double run(double x) {
            for (auto& a : ap) { const double d = a.b[(size_t) a.i]; const double y = -.6 * x + d; a.b[(size_t) a.i] = (float) (x + .6 * y); a.i = (a.i + 1) % (int) a.b.size(); x = y; }
            double o = 0; for (auto& c : cb) { const double d = c.b[(size_t) c.i]; c.lp += .35 * (d - c.lp); c.b[(size_t) c.i] = (float) (x + c.lp * .72); c.i = (c.i + 1) % (int) c.b.size(); o += d; }
            return o * .5;
        }
    };

    double ph[3] {}, f[3] {}, zf[3] {}; float S[NS] {}, IN[ND] {}; struct Pin { int a, b; double w; }; std::vector<Pin> pins; double nlp = 0, toneS[2] {};
    Dec dec[2]; SpringR spr; double env = 0, et = 0, envS = 0, decIn = 0; int est = 0;
    std::vector<int> held; int note = 60; double vel = .8; bool edge = false;
    float src[17] {}; double lp[4] {}, lr[4] {}, shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0; int stp = 0;
    double ms = 0, dl[2] {}, xl[2] {}; long mc = 0;
    static constexpr int NK = 30;
    struct KV { double o1f, o2f, o3f, o1k, o2k, o3k, o1sin, o1saw, o2sq, o2tri, o3sq, o3tri, ncol, nlvl, rlvl, ff, fres, flvl, trap, esig, rmix, rlev, out1, out2, pan1, pan2, tone, jx, jy, kbd; } K {}, KS {}; bool hasKS = false;
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {}, iPin[NS][ND] {};

    Vc3() {
        setTable(vc3::PDEFS, vc3::DSTP, vc3::NP);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        for (int a = 0; a < NS; ++a) for (int b = 0; b < ND; ++b) iPin[a][b] = id("pin" + std::to_string(a) + "_" + std::to_string(b));
        pins.reserve(NS * ND); prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 112358132; ph[0] = 0; ph[1] = .25; ph[2] = .5; for (auto& x : f) x = 0; for (auto& x : S) x = 0; for (auto& x : IN) x = 0; pins.clear(); nlp = 0; toneS[0] = toneS[1] = 0;
        dec[0] = Dec(); dec[1] = Dec(); spr.init(fs * OS); env = et = envS = decIn = 0; est = 0; held.clear(); note = 60; vel = .8; edge = false;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; } shp = sht = shv = sh2p = sh2t = sh2v = clock = 0; stp = 0;
        ms = 0; dl[0] = dl[1] = xl[0] = xl[1] = 0; mc = 0; hasKS = false;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = m[1] = (float) env; return 2; }
    void noteOn(int n, double v) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); held.push_back(n); note = n; vel = v; edge = true; }
    void noteOff(int n) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); if (!held.empty()) note = held.back(); }
    void allOff() override { held.clear(); }

    void ctrl() {
        using namespace vc3; const double dt = CTV / fs; float* s = src;
        clock = advance(clock, g(bpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool edg = ns != stp; stp = ns;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        const int ts = ri(vc3::trig); bool fire = edge; edge = false;
        if (ts > 0) fire = edg && p[(size_t) iS[ts - 1][stp]] != 0;
        const bool manual = g(eoff) >= 9.8; if (fire) { est = 1; et = 0; } else if (est == 0 && !manual) { est = 1; et = 0; }
        const double A = etime(val(ea)), ON = etime(val(eon)), DE = etime(val(edec)) * std::pow(2.0, -decIn * 2), OF = etime(val(eoff));
        et += dt;
        if (est == 1) { env += dt / A; if (env >= 1) { env = 1; est = 2; et = 0; } }
        else if (est == 2) { if (et >= ON && !(!held.empty() && ri(vc3::trig) == 0)) { est = 3; et = 0; } }
        else if (est == 3) { env -= dt / DE; if (env <= 0) { env = 0; est = 4; et = 0; } }
        else if (est == 4) { if (manual) est = 0; else if (et >= OF) { est = 1; et = 0; } }
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph_ = lp[i];
            const double W[5] = { std::sin(TP * ph_), 1 - 4 * std::fabs(ph_ - .5), 2 * ph_ - 1, ph_ < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 15] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[16] = (float) sh2v;
        s[11] = (float) env; s[12] = S[5]; s[13] = (float) vel; s[14] = (float) ((note - 60) / 24.0); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        pins.clear(); for (int a = 0; a < NS; ++a) for (int b = 0; b < ND; ++b) { const int pv = iround(p[(size_t) iPin[a][b]]); if (pv) pins.push_back({ a, b, pv == 1 ? 1.0 : .3 }); }
        K.o1f = o1hz(val(vc3::o1f)); K.o2f = o1hz(val(vc3::o2f)); K.o3f = o3hz(val(vc3::o3f)); K.o1k = cl(val(o1sh) / 10, .05, .95); K.o2k = cl(val(o2sh) / 10, .05, .95); K.o3k = cl(val(o3sh) / 10, .05, .95);
        K.o1sin = val(vc3::o1sin) / 10; K.o1saw = val(vc3::o1saw) / 10; K.o2sq = val(vc3::o2sq) / 10; K.o2tri = val(vc3::o2tri) / 10; K.o3sq = val(vc3::o3sq) / 10; K.o3tri = val(vc3::o3tri) / 10;
        K.ncol = val(vc3::ncol) / 5; K.nlvl = val(vc3::nlvl) / 10; K.rlvl = val(vc3::rlvl) / 10; K.ff = fhz(val(vc3::ff)); K.fres = val(vc3::fres) / 10; K.flvl = val(vc3::flvl) / 10;
        K.trap = val(etrap) / 5; K.esig = val(vc3::esig) / 10; K.rmix = val(vc3::rmix) / 10; K.rlev = val(vc3::rlev) / 10; K.out1 = val(vc3::out1) / 10; K.out2 = val(vc3::out2) / 10;
        K.pan1 = val(vc3::pan1) / 5; K.pan2 = val(vc3::pan2) / 5; K.tone = val(vc3::tone) / 5; K.jx = val(vc3::jx) * val(jrx) / 10; K.jy = val(vc3::jy) * val(jry) / 10; K.kbd = (note - 60) / 12.0 + (val(tune) + bend) / 12;
        if (!hasKS) { KS = K; hasKS = true; }
    }

    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const double k = .003; double* Kp = &K.o1f; double* KSp = &KS.o1f; for (int j = 0; j < NK; ++j) KSp[j] += k * (Kp[j] - KSp[j]);
            envS += .02 * (env - envS);
            for (int q = 0; q < OS; ++q) {
                for (auto& x : IN) x = 0; for (const auto& pn : pins) IN[pn.b] = (float) (IN[pn.b] + S[pn.a] * pn.w);
                const double f1 = KS.o1f * std::pow(2.0, (double) IN[7] + vOsc[0] / 12), f2 = KS.o2f * std::pow(2.0, (double) IN[8] + vOsc[1] / 12), f3 = KS.o3f * std::pow(2.0, (double) IN[9] + vOsc[2] / 12);
                const double d1 = std::min(f1 / fsO, .45), d2 = std::min(f2 / fsO, .45), d3 = std::min(f3 / fsO, .45);
                ph[0] += d1; if (ph[0] >= 1) ph[0] -= 1; ph[1] += d2; if (ph[1] >= 1) ph[1] -= 1; ph[2] += d3; if (ph[2] >= 1) ph[2] -= 1;
                const double p1 = ph[0], p2 = ph[1], p3 = ph[2];
                S[0] = (float) (std::sin(TP * skew(p1, KS.o1k)) * KS.o1sin); S[1] = (float) ((2 * p1 - 1 - blep(p1, d1)) * KS.o1saw);
                double t2 = p2 - KS.o2k; if (t2 < 0) t2 += 1; S[2] = (float) (((p2 < KS.o2k ? 1 : -1) + blep(p2, d2) - blep(t2, d2)) * KS.o2sq); const double sp2 = skew(p2, KS.o2k); S[3] = (float) ((1 - 4 * std::fabs(sp2 - .5)) * KS.o2tri);
                double t3 = p3 - KS.o3k; if (t3 < 0) t3 += 1; S[4] = (float) (((p3 < KS.o3k ? 1 : -1) + blep(p3, d3) - blep(t3, d3)) * KS.o3sq); const double sp3 = skew(p3, KS.o3k); S[5] = (float) ((1 - 4 * std::fabs(sp3 - .5)) * KS.o3tri);
                const double w = rnd() * 2 - 1; nlp += .05 * (w - nlp); S[6] = (float) ((KS.ncol < 0 ? w + (nlp * 2.5 - w) * (-KS.ncol) : w + (w - nlp) * KS.ncol * .8) * KS.nlvl);
                S[7] = (float) ((double) IN[3] * IN[4] * KS.rlvl * 2);
                double fc = KS.ff * std::pow(2.0, (double) IN[10]); fc = std::min(fc, fsO * .2); double kc; const double gg = ladderG(fc, fsO, 3, &kc); const double kq = KS.fres * 8.6 * kc; const double zg = zdfG(fc, fsO);
                if (classicSound()) { const double inp = th(IN[5] * .6 - kq * f[2] + (rnd() - .5) * 3e-4); const double a0 = th(f[0]), a1 = th(f[1]), a2 = th(f[2]);
                    f[0] += gg * (inp - a0); f[1] += gg * (a0 - a1); f[2] += gg * (a1 - a2); }
                else zdfLadder<3>(zf, f, IN[5] * .6 + (rnd() - .5) * 3e-4, zg, KS.fres * 8.6, [](double u) { return th(u); });   // the diode ladder's 3 poles
                S[8] = (float) (f[2] * KS.flvl * 1.4);
                S[9] = (float) (envS * KS.trap); S[10] = (float) (IN[2] * envS * KS.esig * 1.3); decIn = IN[11];
                const double wet = spr.run(IN[6] * .6); const double mx = cl(KS.rmix + IN[12] * .5, 0, 1); S[11] = (float) ((IN[6] * (1 - mx) + wet * mx * 1.6) * KS.rlev);
                S[12] = (float) KS.jx; S[13] = (float) KS.jy; S[14] = (float) KS.kbd;
                const double lvv = std::max(0.0, 1 + (double) IN[13]); const double c1 = IN[0] * KS.out1 * lvv * 2.4, c2 = IN[1] * KS.out2 * lvv * 2.4;
                const double a1p = (KS.pan1 + 1) * .25 * PI, a2p = (KS.pan2 + 1) * .25 * PI;
                dec[0].push(c1 * std::cos(a1p) + c2 * std::cos(a2p)); dec[1].push(c1 * std::sin(a1p) + c2 * std::sin(a2p));
            }
            double sl = dec[0].out(), sr = dec[1].out();
            const double tg = .08; toneS[0] += tg * (sl - toneS[0]); toneS[1] += tg * (sr - toneS[1]);
            if (KS.tone < 0) { sl += (toneS[0] - sl) * (-KS.tone); sr += (toneS[1] - sr) * (-KS.tone); } else { sl += (sl - toneS[0]) * KS.tone * .7; sr += (sr - toneS[1]) * KS.tone * .7; }
            ms += .004 * (val(vc3::master) - ms); sl *= ms; sr *= ms;
            const double yl = sl - xl[0] + .9995 * dl[0]; xl[0] = sl; dl[0] = yl; const double yr = sr - xl[1] + .9995 * dl[1]; xl[1] = sr; dl[1] = yr;
            L[i] = (float) std::tanh(yl); R[i] = (float) std::tanh(yr);
        }
    }
};

} // namespace oct
