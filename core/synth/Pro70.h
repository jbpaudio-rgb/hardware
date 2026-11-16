// PRO70: 6-voice poly after the Prophet-5 (2 oscillators + sub, Poly-Mod, wheel-mod, 4-pole filter, vintage tolerances).
// Port of web/pro-web/pro.html (WORKLET), line for line.
#pragma once
#include "LadderTune.h"
#include "EngineBase.h"
#include "Pro70Params.h"

namespace oct {

struct Pro70 : EngineBase {
    static constexpr int OS = 2, CTV = 16, NV = 6;
    static double envS(double k) { return .0015 * std::pow(8000.0, k); }
    static double wlfo(double k) { return .05 * std::pow(500.0, k); }
    static double glideS(double k) { return k <= 0 ? 0 : .003 * std::pow(2000.0, k); }
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
    struct Voice {
        int note = 60; bool gate = false; int age = 0; double curN = 60, tgtN = 60, vel = .8; ADSR fe, ae; double feS = 0, aeS = 0, pa = 0, pb = 0, ps = 0, s[4] {}, zs[4] {}, ob = 0, tP = 0, tB = 0, tC = 0, tE = 0, dr = 0;
        bool active() const { return ae.st != 0; }
    };

    Voice v[NV]; Dec dec; std::vector<int> held; int clockAge = 0;
    float src[17] {}; double lp[4] {}, lr[4] {}, sh2p = 0, sh2t = 0, sh2v = 0, shp = 0, sht = 0, shv = 0, clock = 0; int stp = 0;
    double wl = 0, wrnd = 0, wm = 0, ms = 0, dl = 0, xl = 0; long mc = 0; bool seqGate = false; int lastNote = 60; double lastVel = .8;
    struct TV { double tune = 0, oa = 0, ob = 0, obf = 0, pwa = 0, pwb = 0, mxa = 0, mxb = 0, mxn = 0, mxs = 0, cut = 0, res = 0, fea = 0, pmf = 0, pmb = 0; } T, S; bool hasS = false;
    struct SW { bool asaw, apul, sync, bsaw, btri, bpul, blo, bkbd, pmA, pmPW, pmF, wA, wB, wPA, wPB, wF; } sw {};
    double kt = 0, subDiv = .5, Vv = 0, velAmt = 0, mt = 0;
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {};

    Pro70() {
        setTable(pro70::PDEFS, pro70::DSTP, pro70::NP);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 246813579; dk = 1 - std::exp(-1 / (.001 * fs));
        for (auto& x : v) { x = Voice(); x.pa = rnd(); x.pb = rnd(); x.tP = (rnd() - .5) * 2; x.tB = (rnd() - .5) * 2; x.tC = (rnd() - .5) * 2; x.tE = (rnd() - .5) * 2; }
        dec = Dec(); held.clear(); clockAge = 0;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; } sh2p = sh2t = sh2v = shp = sht = shv = clock = 0; stp = 0;
        wl = wrnd = wm = 0; hasS = false; ms = dl = xl = 0; mc = 0; seqGate = false; lastNote = 60; lastVel = .8;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = src[11]; m[1] = src[12]; int k = 2; for (const auto& x : v) m[k++] = x.active() ? 1.f : 0.f; return k; }
    bool isHeld(int n) const { return std::find(held.begin(), held.end(), n) != held.end(); }
    void trig(Voice& x, int n, double vel, bool retrig) { x.tgtN = n; if (!x.active()) x.curN = n; x.note = n; x.vel = vel; x.gate = true; x.age = ++clockAge; if (retrig) { x.fe.on(); x.ae.on(); } }
    Voice* oldest(bool (*pred)(const Voice&)) { Voice* r = nullptr; for (auto& x : v) if (pred(x) && (!r || x.age < r->age)) r = &x; return r; }   // stable sort, first by age
    void noteOn(int n, double vel) override {
        held.erase(std::remove(held.begin(), held.end(), n), held.end()); held.push_back(n); lastNote = n; lastVel = vel;
        if (g(pro70::unison) > .5) { bool legato = false; for (auto& x : v) if (x.gate) legato = true; for (auto& x : v) trig(x, n, vel, !legato); return; }
        Voice* x = nullptr; for (auto& y : v) if (y.note == n && y.active()) { x = &y; break; }
        if (!x) x = oldest([](const Voice& y) { return !y.active(); });
        if (!x) x = oldest([](const Voice& y) { return !y.gate; });
        if (!x) x = oldest([](const Voice&) { return true; });
        trig(*x, n, vel, true);
    }
    void noteOff(int n) override {
        held.erase(std::remove(held.begin(), held.end(), n), held.end());
        if (g(pro70::unison) > .5) {
            if (!held.empty()) { const int last = held.back(); for (auto& x : v) trig(x, last, x.vel, false); }
            else for (auto& x : v) { x.gate = false; x.fe.off(); x.ae.off(); }
            return;
        }
        for (auto& x : v) if (x.note == n && x.gate) { x.gate = false; x.fe.off(); x.ae.off(); }
    }
    void allOff() override { held.clear(); for (auto& x : v) { x.gate = false; x.fe.off(); x.ae.off(); } }

    void ctrl() {
        using namespace pro70; const double dt = CTV / fs; float* s = src;
        clock = advance(clock, g(bpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool edge = ns != stp; stp = ns; const bool half = std::fmod(clock, 1.0) < .5;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        const int ts = ri(pro70::trig);
        if (ts > 0) {
            const float sv = p[(size_t) iS[ts - 1][stp]]; const bool on = sv != 0 && half;
            if (edge && sv != 0) { bool anyGate = false; for (auto& x : v) if (x.gate) anyGate = true; if (held.empty() && !anyGate) trig(v[0], lastNote, lastVel, true); for (auto& x : v) if (x.gate || isHeld(x.note)) { x.fe.on(); x.ae.on(); } }
            if (seqGate && !on) for (auto& x : v) { x.fe.off(); x.ae.off(); if (!isHeld(x.note)) x.gate = false; }
            seqGate = on;
        }
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph = lp[i];
            const double W[5] = { std::sin(TP * ph), 1 - 4 * std::fabs(ph - .5), 2 * ph - 1, ph < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 15] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; } const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[16] = (float) sh2v;
        Voice* lv = &v[0]; for (auto& x : v) if (x.age > lv->age) lv = &x; s[11] = (float) lv->fe.v; s[12] = (float) lv->ae.v; s[13] = (float) lv->vel; s[14] = (float) ((lv->note - 60) / 24.0); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        wl += wlfo(val(lfofreq)) * dt; if (wl >= 1) wl -= 1; const double w = wl; double lf = 0; int nw = 0;
        if (g(lfosaw) > .5) { lf += 2 * w - 1; nw++; } if (g(lfotri) > .5) { lf += 1 - 4 * std::fabs(w - .5); nw++; } if (g(lfosq) > .5) { lf += w < .5 ? 1 : -1; nw++; } if (nw) lf /= nw;
        const double mix = val(wmmix); wrnd += .3 * ((rnd() * 2 - 1) - wrnd); wm = ((1 - mix) * lf + mix * wrnd) * val(wmamt);
        const double Vi = val(vintage);
        T.tune = val(tune) + bend; T.oa = jround(val(oafreq)); T.ob = jround(val(obfreq)); T.obf = val(obfine); T.pwa = val(oapw); T.pwb = val(obpw);
        T.mxa = val(pro70::mxa) / 10; T.mxb = val(pro70::mxb) / 10; T.mxn = val(pro70::mxn) / 10; T.mxs = val(pro70::mxs) / 10; T.cut = val(cutoff); T.res = val(pro70::res); T.fea = val(fenvamt); T.pmf = val(pmfenv); T.pmb = val(pmoscb);
        sw.asaw = g(oasaw) > .5; sw.apul = g(oapulse) > .5; sw.sync = g(oasync) > .5; sw.bsaw = g(obsaw) > .5; sw.btri = g(obtri) > .5; sw.bpul = g(obpulse) > .5;
        sw.blo = g(oblo) > .5; sw.bkbd = g(obkbd) > .5; sw.pmA = g(pmfreqa) > .5; sw.pmPW = g(pmpwa) > .5; sw.pmF = g(pmfilt) > .5;
        sw.wA = g(wmfreqa) > .5; sw.wB = g(wmfreqb) > .5; sw.wPA = g(wmpwa) > .5; sw.wPB = g(wmpwb) > .5; sw.wF = g(wmfilt) > .5;
        const double KT[3] = { 0, .5, 1 }; kt = KT[ri(kbd)]; subDiv = g(suboct) > .5 ? .25 : .5; Vv = Vi; velAmt = g(velamt); mt = val(master);
        const bool rel = g(release) > .5;
        const double fa = envS(val(fatt)), fd = envS(val(fdec)), fsu = val(fsus), fr = rel ? envS(val(frel)) : .008;
        const double aa = envS(val(aatt)), ad = envS(val(adec)), asu = val(asus), ar = rel ? envS(val(arel)) : .008;
        const double gl = glideS(val(pro70::glide)); const double ga = gl <= 0 ? 1 : 1 - std::exp(-dt / gl);
        for (auto& x : v) {
            if (!x.active() && !x.gate) continue; const double e = 1 + x.tE * Vi * .12;
            x.fe.tick(dt, fa * e, fd * e, fsu, fr * e); x.ae.tick(dt, aa * e, ad * e, asu, ar * e); x.curN += ga * (x.tgtN - x.curN);
            x.dr += (rnd() - .5) * .03; x.dr *= .996;
        }
        if (!hasS) { S = T; hasS = true; }
    }

    double dk = 0;   // per-sample smoothing of the envelopes (they step every CTV samples): no click on fast settings
    void process(float* L, float* R, int n) override {
        const double fsO = fs * OS;
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const double k = .003; double* Tp = &T.tune; double* Sp = &S.tune; for (int j = 0; j < 15; ++j) Sp[j] += k * (Tp[j] - Sp[j]);
            const double V = Vv; double accO[OS] {};
            for (auto& x : v) {
                if (!x.active()) continue;
                x.feS += dk * (x.fe.v - x.feS); x.aeS += dk * (x.ae.v - x.aeS); const double fe = x.feS, ae = x.aeS, velG = 1 - velAmt + velAmt * x.vel;
                const double nA = x.curN + S.tune + S.oa + x.tP * V * .1 + x.dr * V * .08 + (sw.wA ? wm * .6 : 0);
                const double nB = (sw.bkbd ? x.curN : 60) + S.tune + S.ob + S.obf + x.tB * V * .1 + x.dr * V * .08 + (sw.wB ? wm * .6 : 0);
                double fB = 440 * std::pow(2.0, (nB - 69) / 12); if (sw.blo) fB *= .01;
                const double fA0 = 440 * std::pow(2.0, (nA - 69) / 12);
                const double pmBase = S.pmf * fe;
                const double oct = S.cut * 9.97 + S.fea * fe * 7 + kt * (x.curN - 60) / 12 + x.tC * V * .12 + (sw.wF ? wm * 3 : 0) + (velG - 1) * 1.5;
                for (int q = 0; q < OS; ++q) {
                    const double dB = fB / fsO; x.pb += dB; bool wrapB = false; if (x.pb >= 1) { x.pb -= 1; wrapB = true; }
                    const double pwB = cl(S.pwb + (sw.wPB ? wm * .4 : 0), .02, .98); double ob = 0; int nb = 0; const double pb = x.pb;
                    if (sw.bsaw) { ob += 2 * pb - 1 - blep(pb, dB); nb++; } if (sw.btri) { ob += 1 - 4 * std::fabs(pb - .5); nb++; }
                    if (sw.bpul) { double t2 = pb - pwB; if (t2 < 0) t2 += 1; ob += (pb < pwB ? 1 : -1) + blep(pb, dB) - blep(t2, dB); nb++; } if (nb > 1) ob *= .7; x.ob = ob;
                    const double pm = pmBase + S.pmb * ob;
                    const double fA = sw.pmA ? fA0 * std::pow(2.0, pm * 3) : fA0; const double dA = std::fabs(fA) / fsO;
                    x.pa += dA; if (x.pa >= 1) x.pa -= 1; if (sw.sync && wrapB) x.pa = 0;
                    const double pwA = cl(S.pwa + (sw.pmPW ? pm * .45 : 0) + (sw.wPA ? wm * .4 : 0), .02, .98); double oa = 0; int na = 0; const double pa = x.pa;
                    if (sw.asaw) { oa += 2 * pa - 1 - blep(pa, dA); na++; } if (sw.apul) { double t2 = pa - pwA; if (t2 < 0) t2 += 1; oa += (pa < pwA ? 1 : -1) + blep(pa, dA) - blep(t2, dA); na++; } if (na > 1) oa *= .7;
                    const double dS = fA0 * subDiv / fsO; x.ps += dS; if (x.ps >= 1) x.ps -= 1; double t2s = x.ps - .5; if (t2s < 0) t2s += 1; const double sub = (x.ps < .5 ? 1 : -1) + blep(x.ps, dS) - blep(t2s, dS);
                    const double noise = rnd() * 2 - 1;
                    const double xin = oa * S.mxa + ob * S.mxb + noise * S.mxn * .7 + sub * S.mxs * .8;
                    double fc = 20 * std::pow(2.0, oct + (sw.pmF ? pm * 4 : 0)); fc = std::min(fc, fsO * .22); double kc; const double gg = ladderG(fc, fsO, 4, &kc); const double kq = S.res * 4.05 * kc; const double zg = zdfG(fc, fsO);
                    double* st = x.s;
                    if (classicSound()) { const double inp = th(xin * .8 * (1 + .45 * kq) - kq * st[3] + (rnd() - .5) * 2e-4);
                        st[0] += gg * (inp - st[0]); st[1] += gg * (st[0] - st[1]); st[2] += gg * (st[1] - st[2]); st[3] += gg * (th(st[2]) - st[3]); }
                    else { const double kz = S.res * 4.05; zdfLadder<4>(x.zs, st, xin * .8 * (1 + .45 * kz) + (rnd() - .5) * 2e-4, zg, kz, [](double u) { return th(u); }); }
                    accO[q] += st[3] * ae * velG;
                }
            }
            for (int q = 0; q < OS; ++q) dec.push(accO[q]);
            double out = dec.out() * .5;
            ms += .004 * (mt - ms); out *= ms;
            const double y = out - xl + .9995 * dl; xl = out; dl = y;
            L[i] = R[i] = (float) std::tanh(y);
        }
    }
};

} // namespace oct
