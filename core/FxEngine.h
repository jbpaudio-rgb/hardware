// Octavio FX: the effect rack (8 slots, 16 effects with their models, mod matrix, LFOs, sequencer).
// A line-for-line port of the web AudioWorklet (fx-web/dsp.js); keep the two in step.
#pragma once
#include "DspBlocks.h"
#include "FxParams.h"
#include <memory>

namespace oct {

struct FxRack;
// SIDECHAIN's key from the other parts (the hardware synth fills it for each block before the effects run; the plugin
// has none): each part's sound before its effects, where its drum tracks / pads 1-8 were hit (-1 none), whether a note
// started in this block
struct SideKey { const float* L[4] {}; const float* R[4] {}; int n = 0; int hit[4][8] {}; bool note[4] {}; };

// ------- the reverb models: kind 0 = network (digital, chamber, plate, room), kind 1 = springs -------
struct ER { double ms, g, pan; };
struct RvMod {
    int k = 0; double size = 0, damp = 0, low = 0, diff = 0, mod = 0, bw = 0, hp = 0; int bits = 0; std::vector<ER> er; double rt = 0; bool plate = false;
    std::vector<double> sp; int st = 0; double c = 0, lp = 0, dr = 0, wob = 0; bool sdiff = false, lofi = false;
};
inline const std::vector<RvMod>& RVMOD() {
    static const std::vector<RvMod> M = [] {
        std::vector<RvMod> v;
        auto net = [&](double size, double damp, double low, double diff, double mod, double bw, double hp, std::vector<ER> er, double rt = 0, int bits = 0, bool plate = false) {
            RvMod m; m.k = 0; m.size = size; m.damp = damp; m.low = low; m.diff = diff; m.mod = mod; m.bw = bw; m.hp = hp; m.er = er; m.rt = rt; m.bits = bits; m.plate = plate; v.push_back(m); };
        auto spr = [&](std::vector<double> sp, int st, double c, double lp, double dr, double wob = 0, bool sdiff = false, bool lofi = false) {
            RvMod m; m.k = 1; m.sp = sp; m.st = st; m.c = c; m.lp = lp; m.dr = dr; m.wob = wob; m.sdiff = sdiff; m.lofi = lofi; v.push_back(m); };
        net(1.4, 9000, 1.2, .7, 1.0, 16000, 30, { { 8, .25, -.4 }, { 14, .2, .4 }, { 21, .15, -.2 } });                                  // 480L
        net(1.1, 6000, 1.3, .6, .3, 12000, 40, { { 11, .2, .3 }, { 19, .15, -.3 } }, 0, 12);                                             // EMT 250
        net(1.0, 11000, 1.0, .78, .5, 15000, 40, { { 6, .3, -.3 }, { 11, .25, .4 }, { 17, .2, -.5 }, { 24, .15, .3 } });                // RMX 16
        net(1.3, 12500, 1.1, .72, .6, 20000, 25, { { 9, .28, -.5 }, { 15, .24, .5 }, { 22, .2, -.3 }, { 31, .16, .4 }, { 40, .12, -.2 } }); // M7
        spr({ 33, 39, 46, 52 }, 6, .5, 9000, .4, 0, true);      // BX20
        spr({ 36, 41, 47 }, 10, .6, 7000, .5);                  // XL-305
        spr({ 33, 41 }, 14, .68, 5500, 1.0);                    // SUPER REVERB
        spr({ 35, 43 }, 12, .62, 4500, .7, .6);                 // EF-P2
        spr({ 31, 38, 44 }, 14, .66, 7500, .6);                 // FLINT
        spr({ 29, 37 }, 10, .6, 5000, .8, 1.2, false, true);    // PLACE
        net(1.25, 7500, 1.2, .55, .15, 9000, 120, { { 11, .6, -.4 }, { 19, .5, .5 }, { 27, .45, -.2 }, { 38, .4, .3 }, { 47, .3, -.5 }, { 61, .25, .6 } }, 1.1); // CAPITOL
        net(1.05, 9500, 1.0, .55, .15, 10000, 120, { { 9, .55, .3 }, { 16, .5, -.5 }, { 24, .45, .4 }, { 33, .38, -.2 }, { 44, .3, .6 }, { 57, .25, -.6 } }, 1.0); // ABBEY ROAD
        net(.9, 6500, 1.15, .55, .15, 8500, 120, { { 7, .6, -.3 }, { 13, .5, .4 }, { 21, .4, -.5 }, { 30, .35, .5 }, { 41, .3, -.2 } }, .85);  // SUNSET SOUND
        net(.7, 8500, 1.0, .5, .1, 9500, 150, { { 5, .65, .3 }, { 9, .55, -.4 }, { 15, .45, .5 }, { 22, .4, -.3 }, { 30, .3, .2 } }, .7);     // GOLD STAR
        net(.7, 7000, .8, .82, .15, 18000, 80, {}, 0, 0, true);     // EMT 140
        net(.45, 11000, .7, .84, .1, 20000, 100, {}, 0, 0, true);   // EMT 240
        net(.65, 12000, .85, .86, .15, 20000, 70, {}, 0, 0, true);  // ECOPLATE III
        net(.6, 9000, .8, .65, .05, 16000, 90, {}, 0, 0, true);     // RX4000
        net(.75, 6000, .95, .8, .12, 14000, 60, {}, 0, 0, true);    // AUDICON
        net(.95, 8000, 1.1, .5, .1, 16000, 40, { { 14, .55, -.5 }, { 23, .5, .5 }, { 31, .4, -.2 }, { 42, .35, .4 }, { 55, .3, -.6 }, { 68, .22, .6 } }, 1.0); // STUDIO 2
        net(.5, 6500, 1.0, .45, .05, 14000, 60, { { 4, .6, .2 }, { 8, .55, -.4 }, { 12, .45, .5 }, { 17, .4, -.2 }, { 23, .3, .4 } }, .6);     // SUN STUDIO
        net(.7, 7000, 1.2, .5, .08, 14000, 40, { { 7, .55, -.3 }, { 12, .5, .4 }, { 19, .4, -.5 }, { 26, .35, .3 }, { 35, .25, -.2 } }, .8);   // ELECTRIC LADY A
        net(.85, 8500, 1.1, .5, .08, 16000, 40, { { 10, .55, .4 }, { 17, .5, -.4 }, { 25, .42, .5 }, { 34, .35, -.5 }, { 46, .28, .3 }, { 58, .2, -.3 } }, .9); // CAPITOL A
        return v;
    }();
    return M;
}

// ------- per-block values of each effect (the web code's Q objects) -------
struct QOD { int md = 0, t = 0, m = 0; double g = 0, b = 0, aL = 0, aH = 0, lo = 0, hi = 0, out = 0, y0 = 0, gs = 0, gd = 0, den = 0, aA = 0, air = 0, bl = 0, mg = 0, comp = 0, thr = 0, fg = 0, gate = 0; bool alt = false, hp = false, lp = false; };
struct QPH { bool bi = false, par = false; double fa = 0, fbb = 0, w = 0, fb = 0, drive = 0; int n = 0; };
struct QFL { int m = 0; double w = 0, pol = 1, dr = 0, bl = 1, fb = 0, d[2] { 0, 0 }, c = 0; bool tz = false; };
struct QCH { int m = 0, sh = 0; double w = 0, base = 0, dep = 0, ap = 0, dg = 0, wg = 0, ph = 0; };
struct QMP { int m = 0; double r[2] { 1, 1 }, del = 1, fb = 0, w = 0, win = 1, rand = 0; };
struct QDL { int m = 0, type = 0, mode = 0; double t[2] { 0, 0 }, fb = 0, mr = 0, md = 0, lp = 0, hp = 0, grit = 0, r[2] { 1, 1 }, dr = 0, u = 0, wow = 0, pre = 0, bass = 0, treb = 0, rev = 0, aB = 0, aT = 0; int h[5] { 1, 0, 0, 0, 0 }; int nh = 1; };
struct QRV { int m = 0; const RvMod* M = nullptr; double w = 0, dr = 0, g = 0, a = 0, c = 0, wob = 0, pre = 0, erG = 0, lateG = 1, bw = 0, hp = 0; };
struct QGR { int m = 0, len = 1, st = 0, pat = 0; double spray = 0, intv = 1, pos = 0, win = .5, rev = 0, fb = 0, scan = 0, spread = .7, warp = 0, dr = 0, fg = 0; bool frz = false, sync = false, lp = false, hp = false; };
struct QFK { int m = 0; double d2[2] { 1, 1 }, f[2] { 0, 0 }, fb = 0, dr = 0, tg = 0, d1 = 1, bg = 0, vol = 0; bool res = false, lp = false, hp = false, kill = false; };
struct QBC { int m = 0; double q = 1, div = 1, fg = 0, gate = 0, k = 0, vol = 0, jit = 0, noise = 0, drop = 0, drive = 0; bool lp = false, hp = false; };
struct QRM { int m = 0, mode = 0; bool sq = false, lsq = false; double vol = 1, drive = 0, sp = 0, lfo = 0, f = 0; };
struct QST { int m = 0; double gate = 1, len = 0, pitch = 0, fade = 0, l0 = 64, l1 = 64, cv = 0, dur = 1; };
struct QRE { int m = 0; double T = 1, rate = 1, rand = 0, fb = 0, md = 0, mr = 0, a = 1; bool hold = false; };
struct QFZ { int m = 0; bool on = false, single = false, lp = false, hp = false, autom = false; double n = 0, G = 0, rate = 1, att = 0, rel = 0, dec = 1, lvl = 1, tg = 0; };
struct QTP { int m = 0, ty = 0; bool son = false, dbl = false, wide = false; double g = 0, out = 0, t = 0, ta = 0, ref = 0, lag = 0, wob = 0, gr = 0, gl = 0, s = 1, fb = 0, nm = 1, la = 0,
             h = 1, wow = 0, fl = 0, hiss = 0, wear = 0, ha = 0, gi = 1, lg = 0, bg = 0, bk = .8, bm = 0, as = 0; };
struct QSC { int src = 0, key = 2, mask = 0x1111; double rate = 4, up = 1, down = 0, amt = .7, bpm = 120; };
struct QSH { int m = 0, fl = 0, wt = 0, rt = 0, nv = 1; double wl = 0, pre = 0, v[3] { 2, 0, 0 }, amt = 0, la = 0, out = 1, fg = 0, dw = 1, dwn = 1, sg[2] { 0, 0 }, sa = 0, gn = 1, ha = 0, win = 1; };

struct Head { double c = 0, s = 0, p = 0, d = 1, r = -1; bool half = false, go = false; };
struct Layer { std::vector<float> b[2]; int n = 0; double env = 0; int st = 0; Head h[2]; };

// one effect instance in a slot: its own state and its own controls (slot base + local index)
struct FxUnit {
    FxRack* R = nullptr; int q = 0, base = 0, cur = -1; Rnd rnd; double fs = 48000;
    // control access (the web code's V(), p[I.x])
    inline double V(int l) const;
    inline double P(int l) const;
    double qz(double x, double q) { return jround(x * q + rnd() - rnd()) / q; }

    // state, allocated for the effect in the slot
    struct ODS { OP lo, hi, air; SVF mid, f; double dx = 0, dy = 0, ax = 0; } od[2]; double cEnv = 0, gEnv = 0, gG = 1;
    float phA[2][12] {}, phB[2][6] {}; double phYB[2] {}, phCB[2] {}, phL1 = 0, phL2 = 0, phY[2] {}, phL = 0, phC[2] {};
    DL flD[2]; double flL = 0, flC[2] {}, flA = 0, flAt = 1, flE = 0;
    BBD chB[3]; double chL = 0, chRT = 0, chRV = 0;
    Shift mpS[2]; DL mpD[2]; double mpY[2] {}, mpRt = 0, mpRv = 0, mpT[2] {}; int mpRc = 0;
    DL dlD[2]; double dlT[2] {}, dlF[2] {}, dlH[2] {}, dlB[2] {}, dlU = 0, dlP = 0, dlW = 0, dlWf = 0, dlRs = 0, dlRo[2] {}; int dlRc = 0; Shift dlX[2]; Tank dlRv; double dlA[4] {}, dlHB[4] {};   // dlA: the saturations' last inputs (adaa)
    FDN rvF; std::vector<Spring> rvSp; int rvSm = -1; DL rvPd[2]; double rvIn[4] {}, rvO[2] {}, rvW = 0, rvD[2] {};
    DL grB; static constexpr int grN = 16; float grP[16] {}, grR[16] {}, grL[16] {}, grT[16] {}, grG[16] {}; uint8_t grA[16] {}; double grC = 0, grS = 0, grY = 0; SVF grF[2];
    DL fkD[2]; SVF fkF[2], fkR[2]; double fkY[2] {};
    double bcH[2] {}, bcC = 0, rmP = 0, rmL = 0, bcE = 0, bcG = 1, bcZ[4] {}; SVF bcF[2]; float bcSeq[16] {}; DL bcB[2]; bool bcSt = false; double bcSs = 0; int bcSi = 0, bcSl = 1, bcDr = 0; double bcDg = 1; Hilbert rmH[2];
    DL stB[2]; bool stOn = false, stPrevOn = false; double stStart = 0, stLen = 1, stLenT = 0, stI = 0, stX = 0, stRep = 0, stPos = 0, stGt = 0; int stG = -1, stHold = 0;
    DL reB[2]; Head reH[2]; double reL = 0, reF[2] {}, reHold = -1;
    DL fzH[2]; Layer fzLy[3]; bool fzOn = false; double fzE = 0, fzT = 0, fzR = 1; SVF fzF[2]; int fzNext = 0;
    DL tpR[2], tpD[2], tpC[2]; double tpW = 0, tpF = 0, tpF2 = 0, tpN = 0, tpWb = 0, tpWt = 0, tpLg = -1, tpY[2] {}, tpYl[2] {}, tpT[2] {}, tpL1[2] {}, tpL2[2] {}, tpHP[2] {}; SVF tpB[2], tpH[2];
    FDN shF, shW; Spring shSp[2]; DL shPd[2]; Shift shS[3][2]; double shY[2] {}, shO[2] {}, shWo[2] {}, shL[2] {}, shH[2] {}, shRp = 0; SVF shLF[2], shOF[2];

    double scE = 0, scBeat = 0; long scK = 0; bool scR = false; int scSt = -1;   // SIDECHAIN: the ducking, the tempo clock
    QOD OD; QPH PH; QFL FL; QCH CH; QMP MP; QDL DLq; QRV RV; QGR GR; QFK FK; QBC BC; QRM RM; QST ST; QRE RE; QFZ FZ; QTP TPq; QSH SH; QSC SC;

    void initFx(int f);
    void fxCtrl(int f, double dt, double bpm, double beat);
    void fx(int f, double l, double r, double* o);
};

// the rack: parameters, modulation, clock, built-in sources and the chain of slot instances
struct FxRack {
    double fs = 48000;
    float p[fxp::NP] {}, v[fxp::NP] {}, off[fxp::NP] {};
    double src[14] {}, lp[4] { 0, .33, .66, .5 }, lr[4] {}, shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0; int step = 0; bool edge = false;
    Rnd rnd; int cc = 0; double ms = 0, env = 0, gain = 1, dw = 1, mt = .7; int src0 = 0;
    float mixS[8] {}, mixT[8] {}; std::unique_ptr<FxUnit> units[8]; int slotF[8] { -1, -1, -1, -1, -1, -1, -1, -1 }; int active[8] {}; int nActive = 0; double o[2] {};
    struct Demo { double ph = 0, f = 110, e = 0, lp1 = 0, lp2 = 0, kp = 0, ke = 0, he = 0, hp = 0; } dv;
    struct Voice { int n = -1; double ph = 0, f = 0, e = 0, vel = 0, lp1 = 0, lp2 = 0; bool g = false; int age = 0; } voices[6]; int vage = 0;
    // host transport: when set, the step clock follows the host's beat position
    bool hostClock = false; double hostPpq = 0; int curI = 0;
    const SideKey* side = nullptr;   // SIDECHAIN's key from the other parts (the hardware synth's), or none

    void prepare(double sampleRate) { fs = sampleRate; for (int i = 0; i < fxp::NP; ++i) p[i] = fxp::PDEFS[i].df; initState(); }
    void initState() {
        for (int i = 0; i < fxp::NP; ++i) { off[i] = 0; v[i] = 0; }
        for (auto& x : src) x = 0;
        const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        shp = sht = shv = sh2p = sh2t = sh2v = clock = 0; step = 0; rnd.s = 99991; cc = 0; ms = 0; env = 0;
        for (int q = 0; q < 8; ++q) { mixS[q] = mixT[q] = 0; units[q].reset(); slotF[q] = -1; }
        nActive = 0; o[0] = o[1] = 0; dv = Demo(); for (auto& vv : voices) vv = Voice(); vage = 0;
    }
    double g(int i) const { return p[i]; }
    double val(int i) const { const auto& d = fxp::PDEFS[i]; return cl((double) p[i] + off[i], d.mn, d.mx); }
    void noteOn(int n, double vel) {
        Voice* v0 = nullptr;
        for (auto& x : voices) if (x.n == n) { v0 = &x; break; }
        if (!v0) for (auto& x : voices) if (x.e < 1e-4 && !x.g) { v0 = &x; break; }
        if (!v0) { v0 = &voices[0]; for (auto& x : voices) if (x.age < v0->age) v0 = &x; }
        v0->n = n; v0->f = 440 * std::pow(2.0, (n - 69) / 12.0); v0->g = true; v0->vel = vel; v0->age = ++vage;
    }
    void noteOff(int n) { for (auto& x : voices) if (x.n == n) x.g = false; }
    void allOff() { for (auto& x : voices) x.g = false; }

    void ctrl() {
        using namespace fxp; const double dt = CT / fs;
        const double c0 = std::floor(clock);
        if (hostClock) clock = (hostPpq + curI / fs * g(G::bpm) / 60) * 4; else clock += g(G::bpm) / 60 * 4 * dt;   // host transport: the beat position at this sample
        step = ((int) std::floor(clock)) & 15; edge = std::floor(clock) != c0;
        double* s = src;
        const int RATE[4] = { G::l0rate, G::l1rate, G::l2rate, G::l3rate }, SHAPE[4] = { G::l0shape, G::l1shape, G::l2shape, G::l3shape };
        for (int i = 0; i < 4; ++i) {
            lp[i] += lfoHz(val(RATE[i])) * dt; if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph = lp[i];
            const double W[5] = { std::sin(TP * ph), 1 - 4 * std::fabs(ph - .5), 2 * ph - 1, ph < .5 ? 1.0 : -1.0, lr[i] };
            s[i < 3 ? 1 + i : 12] = W[iround(g(SHAPE[i]))];
        }
        shp += lfoHz(val(G::shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(G::shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = shv;
        sh2p += lfoHz(val(G::sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = g(G::sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[13] = sh2v;
        constexpr int SEQ = G::s1_0 - G::s0_0, MAT = G::m1src - G::m0src;   // lane and matrix row strides in the table
        for (int k = 0; k < 6; ++k) s[5 + k] = p[G::s0_0 + k * SEQ + step];
        s[11] = std::min(1.0, env * 4); s[0] = 0;
        for (int i = 0; i < NP; ++i) off[i] = 0;
        auto addMod = [&](int si, int di, double amt) {
            if (si <= 0 || di <= 0 || amt == 0) return; const int pi = DSTP[di];
            off[pi] = (float) (off[pi] + s[si] * amt * (PDEFS[pi].mx - PDEFS[pi].mn) * .5);
        };
        for (int m = 0; m < 8; ++m) addMod(iround(g(G::m0src + m * MAT)), iround(g(G::m0dst + m * MAT)), g(G::m0amt + m * MAT));
        for (int k = 0; k < 6; ++k) addMod(5 + k, iround(g(G::s0dst + k * SEQ)), g(G::s0amt + k * SEQ));
        for (int i = 0; i < NP; ++i) { const auto& d = PDEFS[i]; v[i] = (float) cl((double) p[i] + off[i], d.mn, d.mx); }
        // the chain: a slot instance is (re)made when its effect changes
        nActive = 0;
        for (int q = 0; q < 8; ++q) {
            const int f = iround(p[G::slot0 + q]) - 1;
            if (f != slotF[q]) {
                slotF[q] = f; mixS[q] = 0;
                if (f >= 0) { auto u = std::make_unique<FxUnit>(); u->R = this; u->q = q; u->base = SLOT_BASE + q * SLOT_STRIDE; u->rnd.s = 1234567u + (uint32_t) q * 7919u; u->cur = f; u->fs = fs; u->initFx(f); units[q] = std::move(u); }
                else units[q].reset();
            }
            if (f >= 0) { active[nActive++] = q; mixT[q] = v[SLOT_BASE + q * SLOT_STRIDE + mixIdx(f)]; } else mixT[q] = 0;
        }
        src0 = iround(p[G::insrc]); gain = std::pow(10.0, (v[G::ingain] - .5) * 36 / 20); dw = v[G::drywet]; mt = v[G::master];
        const double bpm = g(G::bpm), beat = 60 / bpm * fs;
        for (int k = 0; k < nActive; ++k) units[active[k]]->fxCtrl(slotF[active[k]], dt, bpm, beat);
    }
    static int mixIdx(int f) {
        using namespace fxp;
        static const int MIX[17] = { L::f0mix, L::f1mix, L::f2mix, L::f3mix, L::f4mix, L::f5mix, L::f6mix, L::f7mix, L::f8mix, L::f9mix, L::f10mix, L::f11mix, L::f12mix, L::f13mix, L::f14mix, L::f15mix, L::f16mix };
        return MIX[f];
    }

    void demo(double* out) {
        Demo& d = dv;
        if (edge) {
            static const int NOTES[16] = { 45, 57, 52, 57, 48, 60, 55, 52, 45, 57, 52, 64, 43, 55, 50, 59 }; const int st = step;
            d.f = 440 * std::pow(2.0, (NOTES[st] - 69) / 12.0); d.e = 1; if (st % 4 == 0) { d.kp = 0; d.ke = 1; } if (st % 4 == 2) d.he = 1;
        }
        d.ph += d.f / fs; d.ph -= std::floor(d.ph); d.e *= .99975; const double saw = 2 * d.ph - 1; const double fc = 1 - std::exp(-TP * (200 + 3000 * d.e * d.e) / fs);
        d.lp1 += fc * (saw - d.lp1); d.lp2 += fc * (d.lp1 - d.lp2);
        d.ke *= .9993; d.kp += (45 + 180 * d.ke * d.ke) / fs; const double kick = std::sin(TP * d.kp) * d.ke;
        d.he *= .996; const double n = rnd() * 2 - 1; const double hp = n - d.hp; d.hp = n; const double hat = hp * d.he * .25;
        const double y = d.lp2 * d.e * .45 + kick * .6; out[0] = y + hat * .7; out[1] = y + hat * 1.1;
    }
    double kbd() {
        double y = 0;
        for (auto& v0 : voices) {
            if (v0.e < 1e-5 && !v0.g) continue; v0.e += v0.g ? (v0.vel - v0.e) * .004 : -v0.e * .0006; v0.ph += v0.f / fs; v0.ph -= std::floor(v0.ph);
            const double saw = 2 * v0.ph - 1, sq = v0.ph < .5 ? .6 : -.6; const double fc = 1 - std::exp(-TP * (300 + 4000 * v0.e) / fs);
            v0.lp1 += fc * (saw * .7 + sq * .3 - v0.lp1); v0.lp2 += fc * (v0.lp1 - v0.lp2); y += v0.lp2 * v0.e * .3;
        }
        return y;
    }
    // process n samples; in may be null (no audio input)
    static double knee(double x) { const double a = std::fabs(x); return a < .6 ? x : (x < 0 ? -1 : 1) * (.6 + .4 * std::tanh((a - .6) / .4)); }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
        bool bad = false;
        for (int i = 0; i < n; ++i) {
            curI = i; if (cc == 0) ctrl(); cc = (cc + 1) % CT;
            double l, r;
            if (src0 == 0) { double d[2]; demo(d); const double k = kbd(); l = d[0] + k; r = d[1] + k; }
            else if (src0 == 1) { const double k = kbd(); l = k; r = k; }
            else { l = inL ? inL[i] : 0; r = inR ? inR[i] : l; }
            l *= gain; r *= gain; const double a = std::fabs(l) + std::fabs(r); env += (a > env ? .01 : .0004) * (a - env);
            const double dl = l, dr = r;
            for (int k = 0; k < nActive; ++k) {
                const int q = active[k]; const double mtq = mixT[q]; mixS[q] = (float) (mixS[q] + (mtq - mixS[q]) * .002); const double m = mixS[q];
                if (m < 1e-4 && mtq <= 0) continue;
                units[q]->fx(slotF[q], l, r, o); l += (o[0] - l) * m; r += (o[1] - r) * m;
            }
            l = dl + (l - dl) * dw; r = dr + (r - dr) * dw;
            ms += .004 * (mt - ms);
            if (!(l == l && r == r && std::fabs(l) < 1e6 && std::fabs(r) < 1e6)) { l = r = 0; bad = true; }
            const double gl = l * ms * 1.3, gr = r * ms * 1.3;
            // the rack's output: straight up to 0.6, then a soft knee to 1 (classic: tanh all the way, which bent and
            // folded back even quiet signals). Clean at normal levels: no harmonics, no aliasing, no top lost
            if (classicSound()) { outL[i] = (float) std::tanh(gl); outR[i] = (float) std::tanh(gr); }
            else { outL[i] = (float) knee(gl); outR[i] = (float) knee(gr); }
        }
        if (bad) initState();
    }
    // moving boxes: new slot k gets old slot map[k] (its settings were already moved in p)
    void permute(const int* map) {
        std::unique_ptr<FxUnit> U[8]; int F[8]; float S[8];
        for (int k = 0; k < 8; ++k) { U[k] = std::move(units[map[k]]); F[k] = slotF[map[k]]; S[k] = mixS[map[k]]; }
        for (int k = 0; k < 8; ++k) { units[k] = std::move(U[k]); slotF[k] = F[k]; mixS[k] = S[k]; if (units[k]) { units[k]->q = k; units[k]->base = fxp::SLOT_BASE + k * fxp::SLOT_STRIDE; } }
    }
};

inline double FxUnit::V(int l) const { return R->v[base + l]; }
inline double FxUnit::P(int l) const { return R->p[base + l]; }

} // namespace oct

#include "FxEffects.h"
