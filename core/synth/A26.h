// A26: semi-modular duophonic voice after the ARP 2600 (3 VCOs, ring mod, S/H, 4-pole VCF, ADSR + AR, spring reverb).
// Port of web/arp-web/a26.html (WORKLET), line for line.
#pragma once
#include "LadderTune.h"
#include "EngineBase.h"
#include "A26Params.h"

namespace oct {

struct A26 : EngineBase {
    static constexpr int OS = 4, CTV = 16;
    static double envS(double k) { return .002 * std::pow(5000.0, k); }
    static double vibHz(double k) { return .5 * std::pow(24.0, k); }
    static double glideS(double k) { return k <= 0 ? 0 : .003 * std::pow(2000.0, k); }
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
    struct ADSR {
        double v = 0; int st = 0;
        void on() { st = 1; } void off() { if (st) st = 4; }
        double tick(double dt, double a, double d, double s, double r) {
            if (st == 1) { v += (1.3 - v) * (1 - std::exp(-dt / (a / 1.2))); if (v >= 1) { v = 1; st = 2; } }
            else if (st == 2) { v += (s - v) * (1 - std::exp(-dt / (d / 4))); }
            else if (st == 4) { v += (0 - v) * (1 - std::exp(-dt / (r / 4))); if (v < 1e-5) { v = 0; st = 0; } }
            return v;
        }
    };
    struct SpringR {   // allpass chain for the "boing" dispersion + two damped combs (L/R)
        struct Buf { std::vector<float> b; int i = 0; double lp = 0; };
        Buf ap[4], cb[2];
        void init(double fs) {
            const double sc = fs / 48000; const int A[4] = { 113, 337, 617, 229 }, C[2] = { 1597, 1913 };
            for (int k = 0; k < 4; ++k) { ap[k].b.assign((size_t) iround(A[k] * sc), 0.f); ap[k].i = 0; }
            for (int k = 0; k < 2; ++k) { cb[k].b.assign((size_t) iround(C[k] * sc), 0.f); cb[k].i = 0; cb[k].lp = 0; }
        }
        void run(double x, double fb, double* o) {
            for (auto& a : ap) { const double d = a.b[(size_t) a.i]; const double y = -.6 * x + d; a.b[(size_t) a.i] = (float) (x + .6 * y); a.i = (a.i + 1) % (int) a.b.size(); x = y; }
            for (int k = 0; k < 2; ++k) { auto& c = cb[k]; const double d = c.b[(size_t) c.i]; c.lp += .35 * (d - c.lp); c.b[(size_t) c.i] = (float) (x + c.lp * fb); c.i = (c.i + 1) % (int) c.b.size(); o[k] = d; }
        }
    };

    double ph[3] {}, s[4] {}, zs[4] {}; ADSR adsr, ar; Dec dec; SpringR spr;
    double lastSq1 = 0, lastSin2 = 0, lastV3 = 0, pk[7] {}, lfn = 0, shp = 0, shv = 0, shOut = 0, vibp = 0, gateT = 0, vib = 0;
    std::vector<int> held; bool gate = false, edge = false, rel = false, seqGate = false; double vel = .8, c1 = 48, c2 = 48, t1 = 48, t2 = 48;
    float src[19] {}; double lp[4] {}, lr[4] {}, mshp = 0, msht = 0, mshv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0; int stp = 0;
    struct TV { double v1 = 0, v2 = 0, v3 = 0, tune = 0, v1sh = 0, v1fm2 = 0, v1env = 0, v2pw = 0, v2pwm = 0, v2fm1 = 0, v2sh = 0, v2env = 0, v3pw = 0, v3noise = 0, v3env = 0, ncol = 0,
                fring = 0, fv1 = 0, fv2 = 0, fv3 = 0, fnoise = 0, cut = 0, res = 0, fkbd = 0, fenv = 0, ffm2 = 0, fsh = 0, vcaar = 0, vcaadsr = 0, vcaring = 0, rev = 0, pan = 0; } T, S; bool hasS = false;
    static constexpr int NT = 32;
    struct SW { bool k1, k2, k3, lf1, lf2, lf3, sync; int rm1, rm2, w1, w2, w3; } sw {};
    double revfb = .55, mt = 0, ms = 0, dl[2] {}, xl[2] {}; long mc = 0;
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {};

    A26() {
        setTable(a26::PDEFS, a26::DSTP, a26::NP);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        prepare(48000);
    }
    double adS = 0, arS = 0, dk = 0;
    void prepare(double sr) override {
        fs = sr; rs = 135792468; adS = arS = 0; dk = 1 - std::exp(-1 / (.001 * fs)); ph[0] = 0; ph[1] = .3; ph[2] = .6; for (auto& x : s) x = 0; adsr = ADSR(); ar = ADSR(); dec = Dec(); spr.init(fs);
        lastSq1 = lastSin2 = lastV3 = 0; for (auto& x : pk) x = 0; lfn = shp = shv = shOut = vibp = gateT = vib = 0;
        held.clear(); gate = edge = rel = seqGate = false; vel = .8; c1 = c2 = t1 = t2 = 48;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; } mshp = msht = mshv = sh2p = sh2t = sh2v = clock = 0; stp = 0;
        hasS = false; ms = 0; dl[0] = dl[1] = xl[0] = xl[1] = 0; mc = 0;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = src[11]; m[1] = src[12]; return 2; }
    void retarget() { const int lo = *std::min_element(held.begin(), held.end()), hi = *std::max_element(held.begin(), held.end()); t1 = lo; t2 = ri(a26::kmode) == 1 ? hi : lo; }
    void noteOn(int n, double v) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); held.push_back(n); vel = v; retarget(); edge = true; gate = true; }
    void noteOff(int n) override { held.erase(std::remove(held.begin(), held.end(), n), held.end()); if (held.empty()) { gate = false; rel = true; } else retarget(); }
    void allOff() override { held.clear(); gate = false; rel = true; }
    void sampleSH() { shv = ri(a26::shin) == 1 ? lastV3 : rnd() * 2 - 1; }

    void ctrl() {
        using namespace a26; const double dt = CTV / fs; float* sv_ = src;
        clock = advance(clock, g(bpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool edg = ns != stp; stp = ns; const bool half = std::fmod(clock, 1.0) < .5;
        for (int k = 0; k < 6; ++k) sv_[5 + k] = p[(size_t) iS[k][stp]];
        const int ts = ri(a26::trig); bool fire = edge, release = rel; edge = false; rel = false;
        if (ts > 0) { const float sv = p[(size_t) iS[ts - 1][stp]]; const bool on = sv != 0 && half; fire = edg && sv != 0; release = seqGate && !on; seqGate = on; }
        if (fire) { adsr.on(); ar.on(); gateT = 0; if (ri(shtrig) == 1) sampleSH(); } else if (release) { adsr.off(); ar.off(); }
        gateT += dt;
        sv_[11] = (float) adsr.tick(dt, envS(val(ea)), envS(val(ed)), val(es), envS(val(er)));
        sv_[12] = (float) ar.tick(dt, envS(val(ara)), 1e9, 1, envS(val(arr)));
        if (ri(shtrig) == 0) { shp += lfoHz(val(shclk)) * dt; if (shp >= 1) { shp -= 1; sampleSH(); } }
        const double lag = g(shlag); shOut += (lag <= 0 ? 1 : 1 - std::exp(-dt / (.001 + lag * .4))) * (shv - shOut);
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph_ = lp[i];
            const double W[5] = { std::sin(TP * ph_), 1 - 4 * std::fabs(ph_ - .5), 2 * ph_ - 1, ph_ < .5 ? 1.0 : -1.0, lr[i] }; sv_[i < 3 ? 1 + i : 15] = (float) W[ri(iL[i][1])];
        }
        mshp += lfoHz(val(shrate)) * dt; if (mshp >= 1) { mshp -= 1; msht = rnd() * 2 - 1; }
        const double sm = g(shsm); mshv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (msht - mshv); sv_[4] = (float) mshv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); sv_[16] = (float) sh2v;
        sv_[13] = (float) vel; sv_[14] = (float) ((t1 - 60) / 24); sv_[17] = (float) shOut; sv_[18] = (float) lastV3; sv_[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(sv_, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(sv_, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        const double gl = glideS(val(porta)); const double ga = gl <= 0 ? 1 : 1 - std::exp(-dt / gl); c1 += ga * (t1 - c1); c2 += ga * (t2 - c2);
        vibp += vibHz(val(vibr)) * dt; if (vibp >= 1) vibp -= 1; const double vd = envS(g(vibdel));
        vib = std::sin(TP * vibp) * val(vibd) * std::min(1.0, gateT / vd);
        T.v1 = val(v1co) + val(v1fi); T.v2 = val(v2co) + val(v2fi); T.v3 = val(v3co) + val(v3fi); T.tune = val(tune) + bend;
        T.v1sh = val(a26::v1sh); T.v1fm2 = val(a26::v1fm2); T.v1env = val(a26::v1env); T.v2pw = val(a26::v2pw); T.v2pwm = val(a26::v2pwm); T.v2fm1 = val(a26::v2fm1); T.v2sh = val(a26::v2sh); T.v2env = val(a26::v2env);
        T.v3pw = val(a26::v3pw); T.v3noise = val(a26::v3noise); T.v3env = val(a26::v3env); T.ncol = val(a26::ncol);
        T.fring = val(a26::fring) / 10; T.fv1 = val(a26::fv1) / 10; T.fv2 = val(a26::fv2) / 10; T.fv3 = val(a26::fv3) / 10; T.fnoise = val(a26::fnoise) / 10;
        T.cut = val(cutoff); T.res = val(a26::res); T.fkbd = val(a26::fkbd); T.fenv = val(a26::fenv); T.ffm2 = val(a26::ffm2); T.fsh = val(a26::fsh);
        T.vcaar = val(a26::vcaar); T.vcaadsr = val(a26::vcaadsr); T.vcaring = val(a26::vcaring) / 10; T.rev = val(a26::rev); T.pan = val(a26::pan);
        sw.k1 = g(v1kbd) > .5; sw.k2 = g(v2kbd) > .5; sw.k3 = g(v3kbd) > .5; sw.lf1 = g(v1lf) > .5; sw.lf2 = g(v2lf) > .5; sw.lf3 = g(v3lf) > .5; sw.sync = g(v2sync) > .5;
        sw.rm1 = ri(a26::rm1); sw.rm2 = ri(a26::rm2); sw.w1 = ri(fv1w); sw.w2 = ri(fv2w); sw.w3 = ri(fv3w);
        revfb = .55 + g(revdec) * .38; mt = val(master);
        if (!hasS) { S = T; hasS = true; }
    }

    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const double k = .003; double* Tp = &T.v1; double* Sp = &S.v1; for (int j = 0; j < NT; ++j) Sp[j] += k * (Tp[j] - Sp[j]);
            adS += dk * (src[11] - adS); arS += dk * (src[12] - arS); const double adsrV = adS, arV = arS, sh = shOut;   // envelopes step every CTV samples: smoothed per sample
            const double w = rnd() * 2 - 1; double* b = pk; b[0] = .99886 * b[0] + w * .0555179; b[1] = .99332 * b[1] + w * .0750759; b[2] = .969 * b[2] + w * .153852; b[3] = .8665 * b[3] + w * .3104856; b[4] = .55 * b[4] + w * .5329522; b[5] = -.7616 * b[5] - w * .016898;
            const double pink = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * .5362) * .11; b[6] = w * .115926; lfn += .004 * (w * 6 - lfn);
            const double nc = S.ncol, noise = nc < .5 ? w + (pink - w) * (nc * 2) : pink + (cl(lfn, -1, 1) - pink) * ((nc - .5) * 2);
            const double base1 = (sw.k1 ? c1 : 48) + S.tune + vib, base2 = (sw.k2 ? c2 : 48) + S.tune + vib, base3 = (sw.k3 ? c2 : 48) + S.tune + vib;
            const double oct = std::log2(1250.0) * S.cut + S.fkbd * (c1 - 48) / 12 + S.fenv * adsrV * 6 + S.fsh * sh * 3;
            for (int q = 0; q < OS; ++q) {
                double f3 = 440 * std::pow(2.0, (base3 + vOsc[2] + S.v3 + S.v3noise * noise * 24 + S.v3env * adsrV * 48 - 69) / 12); if (sw.lf3) f3 *= .01; const double d3 = f3 / fsO;
                ph[2] += d3; if (ph[2] >= 1) ph[2] -= 1; const double p3 = ph[2];
                const double saw3 = 2 * p3 - 1 - blep(p3, d3); const double pw3 = S.v3pw; double t3 = p3 - pw3; if (t3 < 0) t3 += 1; const double pul3 = (p3 < pw3 ? 1 : -1) + blep(p3, d3) - blep(t3, d3); const double tri3 = 1 - 4 * std::fabs(p3 - .5);
                const double V3[3] = { saw3, pul3, tri3 }; const double v3 = V3[sw.w3]; lastV3 = tri3;
                double f1 = 440 * std::pow(2.0, (base1 + vOsc[0] + S.v1 + S.v1sh * sh * 24 + S.v1fm2 * lastSin2 * 36 + S.v1env * adsrV * 48 - 69) / 12); if (sw.lf1) f1 *= .01; const double d1 = std::fabs(f1) / fsO;
                ph[0] += d1; bool wrap1 = false; if (ph[0] >= 1) { ph[0] -= 1; wrap1 = true; } const double p1 = ph[0];
                const double saw1 = 2 * p1 - 1 - blep(p1, d1); double tt1 = p1 - .5; if (tt1 < 0) tt1 += 1; const double sq1 = (p1 < .5 ? 1 : -1) + blep(p1, d1) - blep(tt1, d1); lastSq1 = sq1;
                double f2 = 440 * std::pow(2.0, (base2 + vOsc[1] + S.v2 + S.v2fm1 * sq1 * 24 + S.v2sh * sh * 24 + S.v2env * adsrV * 48 - 69) / 12); if (sw.lf2) f2 *= .01; const double d2 = std::fabs(f2) / fsO;
                ph[1] += d2; if (ph[1] >= 1) ph[1] -= 1; if (sw.sync && wrap1) ph[1] = 0; const double p2 = ph[1];
                const double saw2 = 2 * p2 - 1 - blep(p2, d2), tri2 = 1 - 4 * std::fabs(p2 - .5), sin2 = std::sin(TP * p2); lastSin2 = sin2;
                const double pw2 = cl(S.v2pw + S.v2pwm * tri3 * .45, .03, .97); double tt2 = p2 - pw2; if (tt2 < 0) tt2 += 1; const double pul2 = (p2 < pw2 ? 1 : -1) + blep(p2, d2) - blep(tt2, d2);
                const double R1[2] = { saw1, sq1 }, R2[4] = { saw2, sin2, tri2, pul2 }, W2[4] = { saw2, tri2, sin2, pul2 };
                const double ring = R1[sw.rm1] * R2[sw.rm2];
                const double x = ring * S.fring + R1[sw.w1] * S.fv1 + W2[sw.w2] * S.fv2 + v3 * S.fv3 + noise * S.fnoise * .8;
                double fc = 16 * std::pow(2.0, oct + S.ffm2 * sin2 * 3); fc = std::min(fc, fsO * .18); double kc; const double gg = ladderG(fc, fsO, 4, &kc); const double kq = S.res * 4.1 * kc; const double zg = zdfG(fc, fsO);
                if (classicSound()) { const double inp = th(th(x * .55) * 1.4 - kq * s[3] + (rnd() - .5) * 2e-4); const double a0 = th(s[0]), a1 = th(s[1]), a2 = th(s[2]), a3 = th(s[3]);
                    s[0] += gg * (inp - a0); s[1] += gg * (a0 - a1); s[2] += gg * (a1 - a2); s[3] += gg * (a2 - a3); }
                else zdfLadder<4>(zs, s, th(x * .55) * 1.4 + (rnd() - .5) * 2e-4, zg, S.res * 4.1, [](double u) { return th(u); });
                const double gain = cl(S.vcaar * arV + S.vcaadsr * adsrV, 0, 1.2);
                dec.push((s[3] * 1.3 + ring * S.vcaring * .6) * gain);
            }
            const double out = dec.out();
            double wet[2]; spr.run(out * .6, revfb, wet); const double pa = (S.pan + 1) * .25 * PI;
            double sl = out * std::cos(pa) * 1.41 * (1 - S.rev * .4) + wet[0] * S.rev * 1.3, sr = out * std::sin(pa) * 1.41 * (1 - S.rev * .4) + wet[1] * S.rev * 1.3;
            ms += .004 * (mt - ms); sl *= ms; sr *= ms;
            const double yl = sl - xl[0] + .9995 * dl[0]; xl[0] = sl; dl[0] = yl; const double yr = sr - xl[1] + .9995 * dl[1]; xl[1] = sr; dl[1] = yr;
            L[i] = (float) std::tanh(yl); R[i] = (float) std::tanh(yr);
        }
    }
};

} // namespace oct
