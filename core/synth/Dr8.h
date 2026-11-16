// DR8: 8-track drum machine (808, 909, LINN, VERMONA and DX kits, 12 instruments each) with a 16-step pattern and mod section.
// Port of web/drum-web/dr8.html (WORKLET), line for line.
#pragma once
#include "EngineBase.h"
#include "Dr8Params.h"
#include <algorithm>
#include <array>

namespace oct {

namespace dr8k {
using std::vector;
struct Osc { bool on = false; double f = 0, sw = 0, swT = 1, sw2 = 0, swT2 = 1, d = 0, l = 0; int w = 0; bool bt = false; };   // bt: a bridged-T resonator (808)
struct Noise { bool on = false; double l = 0, d = 0, fc = 0, q = 0; int m = 0; vector<double> claps; bool clap = false, shake = false, grain = false; };
struct Metal { bool on = false; vector<double> fr; double l = 0, d = 0, fast = 0, bp1 = 0, bp2 = 0, hp = 0, cb1 = 0, cb2 = 0, cd1 = 0; };   // cb1 cb2 cd1: the circuit's bands (and the low one's own decay), when set
struct Membrane { bool on = false; double f = 0; vector<std::array<double, 3>> m; double d = 0, bend = 0, bendT = 1; };
struct Beater { bool on = false; double l = 0, d = 0, fc = 0, q = 0; };
struct Wires { bool on = false; double l = 0, d = 0, hp = 0, lp = 0, dl = 0; };
struct Cymbal { bool on = false; vector<double> fr, pa; double l = 0, pd = 0, d = 0, nf0 = 0, nf1 = 0, nq = 2, hp = 0; };
struct Crush { bool on = false; double q = 0, sr = 0; bool mu = false; };
struct Room { bool on = false; double s = 0, w = 0, lp = 0; };
struct Recipe { Osc o1, o2; Noise n; Metal mt; Membrane md; Beater bt; Wires wr; Cymbal cy; Crush crush; Room room; double click = 0, drv = 0, lp = 0, hp = 0, g = 1, k = 1; };
enum { LP = 0, BP = 1, HP = 2 };

inline Osc osc(double f, double d, double sw = 0, double swT = 1, int w = 0, double l = 0) { Osc o; o.on = true; o.f = f; o.d = d; o.sw = sw; o.swT = swT; o.w = w; o.l = l; return o; }
inline Noise noise(double l, double d, int m, double fc, double q) { Noise n; n.on = true; n.l = l; n.d = d; n.m = m; n.fc = fc; n.q = q; return n; }
inline Metal metal(vector<double> fr, double d, double bp1, double bp2, double hp, double fast = 0) { Metal m; m.on = true; m.fr = std::move(fr); m.l = 1; m.d = d; m.bp1 = bp1; m.bp2 = bp2; m.hp = hp; m.fast = fast; return m; }
inline Membrane memb(double f, vector<std::array<double, 3>> m, double d, double bend = 0, double bendT = 1) { Membrane r; r.on = true; r.f = f; r.m = std::move(m); r.d = d; r.bend = bend; r.bendT = bendT; return r; }
inline Beater beat(double l, double d, double fc, double q) { Beater b; b.on = true; b.l = l; b.d = d; b.fc = fc; b.q = q; return b; }
inline Cymbal cym(int seed, int n, double lo, double hi, double l, double pd, double d, double nf0, double nf1, double nq, double hp) {
    uint32_t s = (uint32_t) (seed * 7919 + 13); auto r = [&] { s = s * 1664525u + 1013904223u; return s / 4294967296.0; };
    Cymbal c; c.on = true; for (int i = 0; i < n; ++i) { c.fr.push_back(lo * std::pow(hi / lo, r())); c.pa.push_back(.4 + .6 * r()); }
    c.l = l; c.pd = pd; c.d = d; c.nf0 = nf0; c.nf1 = nf1; c.nq = nq; c.hp = hp; return c;
}
inline Room room(double s, double w, double lp) { Room r; r.on = true; r.s = s; r.w = w; r.lp = lp; return r; }
inline Recipe tom(double f, double sw, double swT, double d, double nl, double nfc, double drv = 0) { Recipe r; r.o1 = osc(f, d, sw, swT); r.n = noise(nl, .06, LP, nfc, .7); r.g = 1; r.drv = drv; return r; }
inline Recipe hat(const vector<double>& fr, double d, double bp1, double bp2, double hp, double nl, double nfc, double g = .75) { Recipe r; r.mt = metal(fr, d, bp1, bp2, hp); r.n = noise(nl, d * .9, HP, nfc, .7); r.g = g; return r; }

struct Kits {
    vector<Recipe> R[5]; float MU[129];
    Kits() {
        for (int j = 0; j <= 128; ++j) MU[j] = (float) ((std::pow(256.0, j / 128.0) - 1) / 255);
        const vector<double> M8 = { 205.3, 304.4, 369.6, 522.7, 540, 800 }, MV = { 280, 391, 463, 615, 721, 1012 };
        const vector<std::array<double, 3>> KM = { { { 1, 1, 1 } }, { { 1.59, .2, .35 } }, { { 2.14, .1, .25 } } };
        const vector<std::array<double, 3>> SMd = { { { 1, .7, 1 } }, { { 1.59, .5, .75 } }, { { 2.14, .4, .6 } }, { { 2.3, .35, .55 } }, { { 2.65, .25, .5 } }, { { 2.92, .18, .45 } } };
        const vector<std::array<double, 3>> TMO = { { { 1, 1, 1 } }, { { 1.5, .4, .65 } }, { { 1.98, .22, .5 } }, { { 2.44, .12, .4 } } };
        const vector<std::array<double, 3>> BELL = { { { 1, 1, 1 } }, { { 1.5, .6, .8 } }, { { 2.62, .3, .5 } }, { { 3.7, .15, .4 } } };
        const Room ROOM_L = room(1, .5, 5500), ROOM_X = room(1.35, .62, 4800);
        Crush CR9; CR9.on = true; CR9.q = 32; CR9.sr = 32000; Crush CRL; CRL.on = true; CRL.sr = 28000; CRL.mu = true; Crush CRX; CRX.on = true; CRX.sr = 26000; CRX.mu = true;
        auto ltom = [&](double f) { Recipe r; r.md = memb(f, TMO, .2, .12, .05); r.bt = beat(.4, .004, 2500, .8); r.room = ROOM_L; r.g = 1; return r; };
        auto xtom = [&](double f) { Recipe r; r.md = memb(f, TMO, .25, .15, .05); r.bt = beat(.35, .005, 2200, .8); r.room = ROOM_X; r.g = 1; return r; };
        auto tom9 = [&](double f) { Recipe r; r.o1 = osc(f, .24, .45, .05, 2); r.n = noise(.22, .05, LP, 2500, .7); r.g = 1; return r; };
        auto mk = [](auto fn) { Recipe r; fn(r); return r; };
        // 808
        R[0] = {
            mk([&](Recipe& r) { r.o1 = osc(52, .45, 1.6, .006); r.click = .2; r.lp = 6000; r.g = 1.1; }),
            mk([&](Recipe& r) { r.o1 = osc(185, .11, .3, .01); r.o2 = osc(330, .07, 0, 1, 0, .55); r.n = noise(.65, .17, HP, 1800, .7); r.g = .9; }),
            mk([&](Recipe& r) { r.n = noise(1, .22, BP, 1100, 2.2); r.n.clap = true; r.g = 1.1; }),
            mk([&](Recipe& r) { r.o1 = osc(1667, .022); r.o2 = osc(455, .026, 0, 1, 0, .6); r.hp = 350; r.g = .8; }),
            hat(M8, .045, 7500, 10500, 6000, .12, 8000), hat(M8, .32, 7500, 10500, 6000, .12, 8000),
            tom(88, .25, .05, .33, .1, 2500), tom(128, .25, .05, .3, .1, 2500), tom(172, .25, .05, .27, .1, 2500),
            hat(M8, 1.3, 5200, 8800, 4000, .22, 6000, .7),
            mk([&](Recipe& r) { r.mt = metal({ 540, 800 }, .28, 2640, 0, 300, .012); r.g = .8; }),
            mk([&](Recipe& r) { r.o1 = osc(310, .17, .18, .02); r.g = .9; }) };
        // the 808's drums are bridged-T resonators rung by the trigger pulse, not oscillators: their tones are those
        for (auto& r : R[0]) { r.o1.bt = r.o1.on; r.o2.bt = r.o2.on; }
        // the 808's hats and cymbal as its circuit: the six square waves only (no noise), the hats through one band
        // around 7.1 kHz, the cymbal through two (3.44 and 7.1 kHz), the low one dying away faster
        R[0][4].mt.cb1 = R[0][5].mt.cb1 = 7100; R[0][4].mt.cb2 = R[0][5].mt.cb2 = -1;
        R[0][9].mt.cb1 = 3440; R[0][9].mt.cb2 = 7100; R[0][9].mt.cd1 = .35;
        // 909
        R[1] = {
            mk([&](Recipe& r) { r.o1 = osc(52, .26, 2.6, .004, 2); r.o1.sw2 = .55; r.o1.swT2 = .035; r.click = .5; r.drv = .12; r.lp = 9000; r.g = 1; }),
            mk([&](Recipe& r) { r.o1 = osc(178, .075, .55, .012, 2); r.o2 = osc(331, .05, 0, 1, 2, .6); r.n = noise(.85, .13, BP, 4200, .45); r.drv = .05; r.g = .9; }),
            mk([&](Recipe& r) { r.n = noise(1, .18, BP, 1150, 1.4); r.n.claps = { 0, .011, .022 }; r.g = 1.1; }),
            mk([&](Recipe& r) { r.o1 = osc(1820, .016); r.o2 = osc(505, .02, 0, 1, 0, .5); r.n = noise(.3, .012, HP, 3000, .7); r.hp = 400; r.g = .8; }),
            mk([&](Recipe& r) { r.cy = cym(9, 10, 5000, 14000, 1, .03, .045, 8000, 11000, 1.6, 6500); r.crush = CR9; r.g = .75; }),
            mk([&](Recipe& r) { r.cy = cym(9, 10, 5000, 14000, 1, .18, .26, 8000, 11000, 1.6, 6500); r.crush = CR9; r.g = .7; }),
            tom9(100), tom9(142), tom9(190),
            mk([&](Recipe& r) { r.cy = cym(19, 10, 2500, 12000, 1, .9, 1.4, 5000, 9000, 1.2, 3500); r.crush = CR9; r.g = .7; }),
            mk([&](Recipe& r) { r.mt = metal({ 587, 845 }, .25, 2800, 0, 300, .01); r.g = .8; }),
            mk([&](Recipe& r) { r.cy = cym(29, 10, 3000, 11000, 1.4, 1.6, 1.2, 6500, 10000, 2, 4000); r.crush = CR9; r.g = .6; }) };
        // LINN
        R[2] = {
            mk([&](Recipe& r) { r.md = memb(64, KM, .12, .35, .012); r.bt = beat(.6, .006, 3200, .8); r.g = 1.15; }),
            mk([&](Recipe& r) { r.md = memb(205, SMd, .06, .06, .01); r.wr.on = true; r.wr.l = .9; r.wr.d = .11; r.wr.hp = 2200; r.wr.lp = 9000; r.wr.dl = .0015; r.bt = beat(.45, .004, 4500, .7); r.room = ROOM_L; r.g = .9; }),
            mk([&](Recipe& r) { r.n = noise(1, .11, BP, 1500, 1.1); r.n.claps = { 0, .007, .015, .021, .03 }; r.room = ROOM_L; r.g = 1.1; }),
            mk([&](Recipe& r) { r.md = memb(420, { { { 1, .6, 1 } }, { { 2.3, .5, .6 } }, { { 3.5, .35, .4 } } }, .035); r.bt = beat(.9, .003, 2500, 1.2); r.room = ROOM_L; r.g = .8; }),
            mk([&](Recipe& r) { r.cy = cym(41, 10, 4000, 13500, .7, .025, .035, 7000, 10500, 2.5, 5500); r.g = .8; }),
            mk([&](Recipe& r) { r.cy = cym(41, 10, 4000, 13500, .7, .14, .2, 7000, 10500, 2.5, 5500); r.g = .7; }),
            ltom(98), ltom(138), ltom(185),
            mk([&](Recipe& r) { r.cy = cym(43, 10, 2000, 11000, .8, .8, 1, 4500, 8000, 1.5, 3000); r.g = .7; }),
            mk([&](Recipe& r) { r.md = memb(560, BELL, .12); r.bt = beat(.4, .002, 3000, 1); r.g = .8; }),
            mk([&](Recipe& r) { r.n = noise(1, .08, HP, 6500, .7); r.n.shake = r.n.grain = true; r.g = .8; }) };
        // VERMONA
        R[3] = {
            mk([&](Recipe& r) { r.o1 = osc(47, .55, 5, .02); r.click = .3; r.drv = .5; r.lp = 7000; r.g = 1; }),
            mk([&](Recipe& r) { r.o1 = osc(175, .14, .6, .02); r.o2 = osc(262, .1, 0, 1, 0, .5); r.n = noise(.8, .22, BP, 4500, .8); r.drv = .25; r.g = .9; }),
            mk([&](Recipe& r) { r.n = noise(1, .34, BP, 900, 2.5); r.n.clap = true; r.drv = .2; r.g = 1.1; }),
            mk([&](Recipe& r) { r.o1 = osc(780, .04, 1, .005); r.o2 = osc(1560, .02, 0, 1, 0, .4); r.drv = .2; r.g = .85; }),
            hat(MV, .04, 8000, 11000, 6500, .3, 8000), hat(MV, .3, 8000, 11000, 6500, .3, 8000),
            tom(85, 1.2, .04, .4, .15, 2200, .15), tom(120, 1.2, .04, .36, .15, 2200, .15), tom(165, 1.2, .04, .32, .15, 2200, .15),
            hat(MV, 1.2, 5000, 8500, 4000, .3, 6000, .7),
            mk([&](Recipe& r) { r.mt = metal({ 600, 890 }, .25, 2400, 0, 300, .012); r.g = .8; }),
            mk([&](Recipe& r) { r.o1 = osc(420, .12, 2, .01); r.n = noise(.3, .03, BP, 2500, .8); r.drv = .3; r.g = .85; }) };
        // DX
        R[4] = {
            mk([&](Recipe& r) { r.md = memb(56, KM, .19, .5, .015); r.bt = beat(.45, .008, 2200, .8); r.drv = .08; r.g = 1.15; }),
            mk([&](Recipe& r) { r.md = memb(190, SMd, .08, .08, .012); r.wr.on = true; r.wr.l = .8; r.wr.d = .14; r.wr.hp = 1800; r.wr.lp = 8000; r.wr.dl = .002; r.bt = beat(.4, .005, 3800, .7); r.room = ROOM_X; r.g = .9; }),
            mk([&](Recipe& r) { r.n = noise(1, .13, BP, 1300, 1.2); r.n.claps = { 0, .009, .017, .026 }; r.room = ROOM_X; r.g = 1.1; }),
            mk([&](Recipe& r) { r.md = memb(480, { { { 1, .7, 1 } }, { { 2.6, .4, .5 } } }, .03); r.bt = beat(.9, .003, 2800, 1.1); r.room = ROOM_X; r.g = .8; }),
            mk([&](Recipe& r) { r.cy = cym(51, 10, 3500, 12000, .7, .03, .04, 6000, 9000, 2.2, 5000); r.g = .8; }),
            mk([&](Recipe& r) { r.cy = cym(51, 10, 3500, 12000, .7, .16, .23, 6000, 9000, 2.2, 5000); r.g = .7; }),
            xtom(90), xtom(125), xtom(170),
            mk([&](Recipe& r) { r.cy = cym(53, 10, 1800, 10000, .8, .9, 1.1, 4000, 7500, 1.4, 2800); r.g = .7; }),
            mk([&](Recipe& r) { r.md = memb(545, BELL, .13); r.bt = beat(.4, .002, 2800, 1); r.g = .8; }),
            mk([&](Recipe& r) { r.n = noise(1, .09, HP, 5000, .7); r.n.shake = r.n.grain = true; r.g = .8; }) };
        for (auto& r : R[2]) r.crush = CRL; for (auto& r : R[4]) r.crush = CRX;
        static const double NORM[5][12] = { { 0.85, 0.65, 3.61, 1.2, 5, 3.78, 0.75, 0.76, 0.78, 2.03, 1.73, 0.68 }, { 0.84, 0.98, 2.28, 1.19, 1.32, 1.01, 0.8, 0.8, 0.81, 0.61, 1.96, 1.11 }, { 0.95, 0.95, 1.97, 1.01, 2.95, 1.65, 0.9, 0.95, 0.7, 0.72, 0.5, 2.4 }, { 0.69, 0.63, 3.03, 0.62, 4.19, 2.15, 0.6, 0.6, 0.61, 1.57, 2.06, 0.49 }, { 0.84, 0.95, 2.16, 1.15, 1.9, 1.26, 0.95, 0.65, 0.79, 0.81, 0.49, 2.23 } };
        for (int k = 0; k < 5; ++k) for (int i = 0; i < 12; ++i) R[k][(size_t) i].k = NORM[k][i];
    }
    double muq(double x, double r) const {
        const double a = x < 0 ? -x : x; int lo = 0, hi = 128; while (hi - lo > 1) { const int m = (lo + hi) >> 1; if (MU[m] <= a) lo = m; else hi = m; }
        const double fr = (a - MU[lo]) / ((double) MU[hi] - MU[lo]); const double q = (fr + (r - .5) * .7 > .5) ? MU[hi] : MU[lo]; return x < 0 ? -q : q;
    }
};
inline const Kits& kits() { static const Kits K; return K; }

struct SvfC { double a1 = 0, a2 = 0, a3 = 0, k = 0; int mode = 0; bool on = false; };
inline SvfC svfc(double fc, double q, int mode, double fs) { const double g = std::tan(PI * std::min(fc, fs * .45) / fs), k = 1 / q; const double a1 = 1 / (1 + g * (g + k)); SvfC c; c.a1 = a1; c.a2 = g * a1; c.a3 = g * g * a1; c.k = k; c.mode = mode; c.on = true; return c; }
inline double svf(double* st, double x, const SvfC& c) { const double v3 = x - st[1]; const double v1 = c.a1 * st[0] + c.a2 * v3; const double v2 = st[1] + c.a2 * st[0] + c.a3 * v3; st[0] = 2 * v1 - st[0]; st[1] = 2 * v2 - st[1]; return c.mode == 0 ? v2 : c.mode == 1 ? v1 * c.k : x - c.k * v1 - v2; }
inline double th(double x) { if (x > 3) return 1; if (x < -3) return -1; const double x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
inline double blep(double t, double dt) { return oct::blepAny(t, dt); }

struct HitP { double tune = 0, dec = .5, tone = 0, snap = .5, drv = 0, noise = .5, ndec = .5; };

struct DV {
    double fs = 48000; EngineBase* E = nullptr; bool on = false; int inst = -1; float mph[6] {}, mdp[8] {}, cyp[12] {};
    double bz1[2] {}, bz2[2] {};   // the bridged-T resonators' states
    double ns[2] {}, b1[2] {}, b2[2] {}, bs[2] {}, ws[2] {}, cs1[2] {}, cs2[2] {}, wl = 0, mhp = 0, chp = 0, ohp = 0, lpS = 0;
    double crPh = 0, crH = 0, crP = 0, rc1 = 0, rc2 = 0, gr = 0; float rb[2048] {}; int ri = 0; double rl = 0; int rtap[4] {};
    const Recipe* r = nullptr; double t = 0, vel = 0, choke = 1; bool chk = false;
    double tm = 1, dm = 1, drv = 0, swm = 0, sn = 0, cspr = 0, nl = 0, nm = 0, ph1 = 0, ph2 = 0, spread = 0, wlA = 0, chA = 0, mhpA = 0, ck = 0, lpA = 0, ohpA = 0, crS = 0, crA = 0, rA = 0;
    double dryEnd = 0, fadeS = 0, fadeK = 0, end = 0;
    SvfC nc, bc, whc, cc1, cc2, c1, c2; bool circ = false;   // circ: the metal through its circuit's bands (808 hats, cymbal), no noise
    double rnd() { return E->rnd(); }

    void hit(const Recipe& R, const HitP& p, double v) {
        r = &R; t = 0; on = true; vel = v * (R.g ? R.g : 1) * (R.k ? R.k : 1); choke = 1; chk = false;
        const double tmv = std::pow(2.0, p.tune / 12); double dmv = std::pow(4.0, p.dec * 2 - 1);
        const bool smp = R.crush.on && R.crush.mu; if (smp) dmv /= tmv;
        tm = tmv; dm = dmv; drv = R.drv + p.drv; swm = .4 + p.snap * 1.2; sn = .3 + p.snap * 1.4; cspr = .6 + p.snap * .8; nl = p.noise * 2;
        nm = std::pow(dmv, 1.4) * (p.ndec < .5 ? .12 * std::pow(1 / .12, p.ndec * 2) : std::pow(2.0, (p.ndec - .5) * 2));
        ph1 = R.o1.on && R.o1.w ? .25 : 0; ph2 = R.o2.on && R.o2.w ? .25 : 0; bz1[0] = bz1[1] = bz2[0] = bz2[1] = 0; for (auto& x : mdp) x = 0;
        const double tb = std::pow(2.0, p.tone * 1.5); double mx = .05;
        if (R.o1.on) mx = std::max(mx, R.o1.d * dmv); if (R.o2.on) mx = std::max(mx, R.o2.d * dmv); if (R.md.on) mx = std::max(mx, R.md.d * dmv);
        if (R.n.on) { const Noise& n = R.n; nc = svfc(n.fc * tb * std::sqrt(tmv), n.q, n.m, fs); spread = .006 + p.snap * .012;
            mx = std::max(mx, n.d * nm + (!n.claps.empty() ? n.claps.back() * cspr : n.clap ? .06 : 0)); }
        if (R.bt.on) bc = svfc(R.bt.fc * tb * std::sqrt(tmv), R.bt.q, 1, fs);
        if (R.wr.on) { const Wires& w = R.wr; whc = svfc(w.hp * tb, .7, 2, fs); wlA = 1 - std::exp(-TP * std::min(w.lp * tb, fs * .45) / fs); mx = std::max(mx, w.d * nm + w.dl); }
        if (R.cy.on) { const Cymbal& c = R.cy; cc1 = svfc(c.nf0 * tb, c.nq ? c.nq : 2, 1, fs); cc2 = svfc(c.nf1 * tb, c.nq ? c.nq : 2, 1, fs); chA = 1 - std::exp(-TP * c.hp / fs); mx = std::max({ mx, c.d * nm, c.pd * nm }); }
        circ = R.mt.on && R.mt.cb1 > 0 && !classicSound();
        if (R.mt.on && circ) { c1 = svfc(R.mt.cb1 * tb, 3, 1, fs); c2 = R.mt.cb2 > 0 ? svfc(R.mt.cb2 * tb, 3, 1, fs) : SvfC(); mhpA = 1 - std::exp(-TP * R.mt.hp / fs); mx = std::max(mx, R.mt.d * dmv); }
        else if (R.mt.on) { c1 = svfc(R.mt.bp1 * tb, 3, 1, fs); c2 = R.mt.bp2 ? svfc(R.mt.bp2 * tb, 3, 1, fs) : SvfC(); mhpA = 1 - std::exp(-TP * R.mt.hp / fs); mx = std::max(mx, R.mt.d * dmv); }
        ck = R.click ? R.click * p.snap * 2 : 0;
        const double lpf = std::min((R.lp ? R.lp : 16000) * std::pow(2.0, p.tone * 2), fs * .45); lpA = lpf >= 17000 ? 0 : 1 - std::exp(-TP * lpf / fs);
        ohpA = R.hp ? 1 - std::exp(-TP * R.hp / fs) : 0;
        if (R.crush.on) { const double sr = R.crush.sr * (smp ? tmv : 1); crS = sr / fs; crA = 1 - std::exp(-TP * std::min(sr * .45, fs * .45) / fs); }
        if (R.room.on) { const double T[4] = { .0031, .0053, .0079, .0113 }; for (int k = 0; k < 4; ++k) rtap[k] = (int) std::min(2040.0, jround(T[k] * R.room.s * fs)); rA = 1 - std::exp(-TP * R.room.lp / fs); }
        dryEnd = mx * 3.2 + .012; fadeS = mx * 2.2 + .008; fadeK = 1 / (dryEnd - fadeS); end = dryEnd + (R.room.on ? .02 : 0);
    }
    // the 808's bridged-T: a resonator (ringing at f, its decay d) struck by the 1 ms trigger pulse, which also leaks
    // through as the attack's knock; a hard strike drives it a little sharp (the circuit's soft limit) at the start
    double bridged(double* st, double f, double d) {
        const double tau = std::min(.001, .25 / f), w = TP * f;   // the pulse: 1 ms, shorter for the high ones (the rim)
        const double x = t < tau ? PI / (2 * tau * w) * std::sin(PI * t / tau) : 0;   // its area rings the resonator to ~1
        const double lv = std::fabs(st[0]) + std::fabs(st[1]); const double fe = f * (1 + .04 * std::min(1.0, lv));
        const double g = std::tan(PI * std::min(fe, fs * .45) / fs), k = 1 / std::max(.5, PI * f * d);
        const double a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - st[1], v1 = a1 * st[0] + a2 * v3, v2 = st[1] + a2 * st[0] + a3 * v3; st[0] = 2 * v1 - st[0]; st[1] = 2 * v2 - st[1];
        return v1;
    }
    static double wave(int w, double ph) { if (w == 2) { const double x = 1 - 4 * std::fabs(ph - .5); return 1.5 * x - .5 * x * x * x; } if (w) return 1 - 4 * std::fabs(ph - .5); return std::sin(TP * ph); }

    double sample() {
        const Recipe& R = *r; double y = 0;
        if (R.o1.on) { const Osc& o = R.o1; const double f = o.f * tm * (1 + (o.sw * std::exp(-t / o.swT) + o.sw2 * std::exp(-t / o.swT2)) * swm);
            if (o.bt && !classicSound()) y += bridged(bz1, f, o.d * dm); else { ph1 += f / fs; if (ph1 >= 1) ph1 -= 1; y += wave(o.w, ph1) * std::exp(-t / (o.d * dm)); } }
        if (R.o2.on) { const Osc& o = R.o2;
            if (o.bt && !classicSound()) y += bridged(bz2, o.f * tm, o.d * dm) * o.l; else { ph2 += o.f * tm / fs; if (ph2 >= 1) ph2 -= 1; y += wave(o.w, ph2) * std::exp(-t / (o.d * dm)) * o.l; } }
        if (R.md.on) { const Membrane& m = R.md; const double fb = m.f * tm * (1 + m.bend * swm * std::exp(-t / m.bendT)); double s = 0;
            for (size_t i = 0; i < m.m.size(); ++i) { const auto& q = m.m[i]; double ph = mdp[i] + fb * q[0] / fs; if (ph >= 1) ph -= 1; mdp[i] = (float) ph; s += std::sin(TP * ph) * q[1] * std::exp(-t / (m.d * q[2] * dm)); }
            y += s; }
        if (ck) y += (rnd() * 2 - 1) * std::exp(-t / .0012) * ck * .8 + (t < .0005 ? ck * .6 : 0);
        if (R.bt.on) { const Beater& b = R.bt; const double e = std::exp(-t / b.d); if (e > 1e-4) y += svf(bs, rnd() * 2 - 1, bc) * e * b.l * sn * 2; }
        if (R.wr.on) { const Wires& w = R.wr; const double tt = t - w.dl; if (tt > 0) { const double e = (1 - std::exp(-tt / .0015)) * std::exp(-tt / (w.d * nm));
            if (e > 1e-5) { const double x = svf(ws, rnd() * 2 - 1, whc); wl += wlA * (x - wl); y += wl * e * w.l * nl * 1.6; } } }
        if (R.n.on && !circ) { const Noise& n = R.n; double env;
            if (!n.claps.empty()) { const double cs = cspr; const auto& cp = n.claps; env = 0; for (size_t b = 0; b < cp.size(); ++b) { const double tb = t - cp[b] * cs; if (tb >= 0) env += std::exp(-tb / .0045) * (1 - b * .08); }
                const double tt = t - cp.back() * cs; if (tt >= 0) env += .8 * std::exp(-tt / (n.d * nm)); }
            else if (n.clap) { const double s = spread; env = 0; for (int b = 0; b < 3; ++b) { const double tb = t - b * s; if (tb >= 0) env += std::exp(-tb / .005); } const double tt = t - 3 * s; if (tt >= 0) env += .85 * std::exp(-tt / (n.d * nm)); }
            else { env = std::exp(-t / (n.d * nm)); if (n.shake) env *= std::min(1.0, t / .008); if (n.grain) { gr *= .995; if (rnd() < .0025) gr = 1; env *= .3 + .7 * gr; } }
            if (env > 1e-5) y += svf(ns, rnd() * 2 - 1, nc) * env * n.l * nl; }
        if (R.cy.on) { const Cymbal& c = R.cy; const auto& fr = c.fr; double s = 0; const double pe = std::exp(-t / (c.pd * nm));
            if (pe > 1e-4) { for (size_t i = 0; i < fr.size(); ++i) { double ph = cyp[i] + fr[i] * tm / fs; if (ph >= 1) ph -= 1; cyp[i] = (float) ph; s += std::sin(TP * ph) * c.pa[i]; } s *= pe * c.l * 2.5 / fr.size(); }
            const double ne = std::exp(-t / (c.d * nm)); if (ne > 1e-5) { const double w = rnd() * 2 - 1; const double x = svf(cs1, w, cc1) + svf(cs2, w, cc2) + w * .35; chp += chA * (x - chp); s += (x - chp) * ne * nl * .8; }
            y += s; }
        if (R.mt.on) { const Metal& m = R.mt; const double env = m.fast ? (.7 * std::exp(-t / m.fast) + .3 * std::exp(-t / (m.d * dm))) : std::exp(-t / (m.d * dm));
            if (env > 1e-5) { double sq = 0; const auto& fr = m.fr; for (size_t i = 0; i < fr.size(); ++i) { const double dt = fr[i] * tm / fs; double pp = mph[i] + dt; if (pp >= 1) pp -= 1; mph[i] = (float) pp; double t2 = pp - .5; if (t2 < 0) t2 += 1; sq += (pp < .5 ? 1 : -1) + blep(pp, dt) - blep(t2, dt); }
                sq /= fr.size(); double x = svf(b1, sq, c1) * (circ && m.cd1 > 0 ? std::exp(-t / (m.d * m.cd1 * dm)) / std::max(env, 1e-9) : 1); if (c2.on) x += svf(b2, sq, c2) * (circ && m.cb2 > 0 ? 1.3 : 1); mhp += mhpA * (x - mhp); y += (x - mhp) * env * m.l * (circ ? 4.0 : 2.2); } }   // the circuit's bands pass less: as loud as before
        y *= vel;
        if (drv > 0) y = th(y * (1 + drv * 5)) / (1 + drv * .6);
        if (R.room.on) { const Room& rm = R.room; rb[ri] = (float) y;
            const double e = (double) rb[(ri - rtap[0] + 2048) & 2047] * .45 + (double) rb[(ri - rtap[1] + 2048) & 2047] * .33 - (double) rb[(ri - rtap[2] + 2048) & 2047] * .24 + (double) rb[(ri - rtap[3] + 2048) & 2047] * .17;
            ri = (ri + 1) & 2047; rl += rA * (e - rl); y += rl * rm.w; }
        if (R.crush.on) { const Crush& c = R.crush; crPh += crS; while (crPh >= 1) { crPh -= 1; crP = crH; const double x = y < -1 ? -1 : y > 1 ? 1 : y; crH = c.mu ? kits().muq(x, rnd()) : jround(x * c.q) / c.q; }
            y = crP + (crH - crP) * crPh; rc1 += crA * (y - rc1); rc2 += crA * (rc1 - rc2); y = rc2; }
        if (t > fadeS) { const double f = 1 - (t - fadeS) * fadeK; y *= f > 0 ? f * f : 0; }
        if (ohpA) { ohp += ohpA * (y - ohp); y -= ohp; }
        if (lpA) { lpS += lpA * (y - lpS); y = lpS; }
        if (chk) { choke *= .9965; y *= choke; if (choke < 1e-3) on = false; }
        t += 1 / fs; if (t > end) on = false;
        return y;
    }
};
} // namespace dr8k

struct Dr8 : EngineBase {
    static constexpr int CTV = 16;
    dr8k::DV v[8]; dr8k::HitP hp;
    struct TrI { int kit, inst, tune, dec, tone, snap, drv, noise, ndec, lvl, pan, mute, pat[16]; } TR[8] {};
    int LR[4] {}, LS[4] {}, SQ[6][16] {}, SD[6] {}, SA[6] {}, MS[8] {}, MD[8] {}, MA[8] {};
    float lvS[8] {}, pnS[8] {}, lvT[8] {}, pnT[8] {}, src[15] {}, lp[4] {}, lr[4] {};
    double shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0, nextT = 0, srnd = 0, ms = 0, mt = .7, bpmv = 120, sw = 0, acc = .5;
    long next = 0, mc = 0; bool playing = false; int dstep = -1, stp = 0; unsigned hitMask = 0; double lastClock = 0;

    Dr8() {
        setTable(dr8::PDEFS, dr8::DSTP, dr8::NP); (void) dr8k::kits();
        for (int t = 0; t < 8; ++t) {
            const std::string x = "t" + std::to_string(t); auto& T = TR[t];
            T.kit = id(x + "kit"); T.inst = id(x + "inst"); T.tune = id(x + "tune"); T.dec = id(x + "dec"); T.tone = id(x + "tone"); T.snap = id(x + "snap"); T.drv = id(x + "drv");
            T.noise = id(x + "noi"); T.ndec = id(x + "ndec"); T.lvl = id(x + "lvl"); T.pan = id(x + "pan"); T.mute = id(x + "mute");
            for (int k = 0; k < 16; ++k) T.pat[k] = id("p" + std::to_string(t) + "_" + std::to_string(k));
        }
        for (int i = 0; i < 4; ++i) { LR[i] = id("l" + std::to_string(i) + "rate"); LS[i] = id("l" + std::to_string(i) + "shape"); }
        for (int q = 0; q < 6; ++q) { for (int k = 0; k < 16; ++k) SQ[q][k] = id("s" + std::to_string(q) + "_" + std::to_string(k)); SD[q] = id("s" + std::to_string(q) + "dst"); SA[q] = id("s" + std::to_string(q) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); MS[m] = id(x + "src"); MD[m] = id(x + "dst"); MA[m] = id(x + "amt"); }
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 314159265; for (auto& x : v) { x = dr8k::DV(); x.fs = fs; x.E = this; } hp = dr8k::HitP();
        for (int i = 0; i < 8; ++i) { lvS[i] = pnS[i] = pnT[i] = 0; lvT[i] = .8f; } for (auto& x : src) x = 0; const float L0[4] = { 0, .33f, .66f, .5f }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        shp = sht = shv = sh2p = sh2t = sh2v = 0; clock = 0; next = 0; nextT = 0; playing = false; dstep = -1; stp = 0; srnd = 0; hitMask = 0; ms = 0; mt = .7; mc = 0; bpmv = 120; sw = 0; acc = .5; lastClock = 0;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = (float) dstep; m[1] = (float) hitMask; hitMask = 0; return 2; }
    double vi(int i) const { return val(i); }
    void trigger(int tr, double vel) {
        const auto& T = TR[tr]; if (p[(size_t) T.mute] > .5) return; const int kit = ri(T.kit), inst = ri(T.inst);
        if (inst == 4) for (int j = 0; j < 8; ++j) if (j != tr && v[j].on && v[j].inst == 5) v[j].chk = true;
        hp.tune = vi(T.tune); hp.dec = vi(T.dec); hp.tone = vi(T.tone); hp.snap = vi(T.snap); hp.drv = vi(T.drv); hp.noise = vi(T.noise); hp.ndec = vi(T.ndec);
        v[tr].inst = inst; v[tr].hit(dr8k::kits().R[cl(kit, 0, 4) == kit ? kit : 0][(size_t) std::max(0, std::min(11, inst))], hp, vel); hitMask |= 1u << tr; markHit(tr);
    }
    // MIDI, as in the app: the General MIDI drum notes where they fit (kick, snare, clap, hats, toms...), other notes from C1 up play
    // the 8 tracks in order. A pad from the panel plays at .9; MIDI velocity scales that.
    void noteOn(int n, double vel) override {
        int t; switch (n) { case 36: case 35: t = 0; break; case 38: case 40: t = 1; break; case 39: t = 2; break; case 42: case 44: t = 3; break; case 46: t = 4; break;
            case 45: case 41: t = 5; break; case 37: t = 6; break; case 56: t = 7; break; default: t = ((n - 36) % 8 + 8) % 8; }
        trigger(t, .9 * vel); }
    void pad(int t) override { if (t >= 0 && t < 8) trigger(t, .9); }
    void noteOff(int) override {}
    void allOff() override {}
    void fire(int st) {
        dstep = st; srnd = rnd() * 2 - 1;
        for (int tr = 0; tr < 8; ++tr) { const int vv = ri(TR[tr].pat[st]); if (vv > 0) trigger(tr, vv == 2 ? .78 * (1 + acc * .6) : .78); }
    }
    static double lfo(double ph, int shape, double r) { switch (shape) { case 0: return std::sin(TP * ph); case 1: return 1 - 4 * std::fabs(ph - .5); case 2: return 2 * ph - 1; case 3: return ph < .5 ? 1 : -1; default: return r; } }
    void addModD(int si, int di, double amt) { if (si <= 0 || di <= 0 || amt == 0 || di >= dr8::NDST) return; const int pi = DSTP[di]; if (pi < 0) return; off[(size_t) pi] = (float) (off[(size_t) pi] + src[si] * amt * (D[pi].mx - D[pi].mn) * .5); }
    void ctrl() {
        using namespace dr8; const double dt = CTV / fs; float* s = src;
        stp = ((int) std::floor(clock)) & 15; for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) SQ[k][stp]];
        for (int i = 0; i < 4; ++i) { lp[i] = (float) lfoAdv(i, lp[i], vi(LR[i]), dt); if (lp[i] >= 1) { lp[i] = (float) (lp[i] - 1); lr[i] = (float) (rnd() * 2 - 1); } s[i < 3 ? 1 + i : 13] = (float) lfo(lp[i], ri(LS[i]), lr[i]); }
        shp += lfoHz(vi(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = p[shsm]; shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(vi(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = p[sh2sm]; sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[14] = (float) sh2v;
        s[11] = (float) ((std::fmod(clock, 16.0) / 16) * 2 - 1); s[12] = (float) srnd; s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addModD(ri(MS[m]), ri(MD[m]), p[(size_t) MA[m]]);
        for (int k = 0; k < 6; ++k) addModD(5 + k, ri(SD[k]), p[(size_t) SA[k]]);
        bpmv = vi(bpm); sw = vi(swing) * .5; acc = vi(accent);
        for (int tr = 0; tr < 8; ++tr) { lvT[tr] = (float) vi(TR[tr].lvl); pnT[tr] = (float) vi(TR[tr].pan); }
        mt = vi(master);
    }
    void process(float* L, float* R, int n) override {
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const bool play = p[dr8::play] > .5;
            if (hostClock) {
                // following the host: the pattern position is the host's beat position, re-synced when the host jumps or loops
                clock = hostPpq * 4 + curI / fs * bpmv / 60 * 4;
                if (play && !playing) { playing = true; next = (long) std::ceil(clock - 1e-9); nextT = next + ((next & 1) ? sw : 0); }
                else if (!play && playing) { playing = false; dstep = -1; }
                if (playing && (clock < lastClock - 1e-6 || clock > nextT + 1)) { next = (long) std::ceil(clock - 1e-9); nextT = next + ((next & 1) ? sw : 0); }
                lastClock = clock;
                if (playing && clock >= nextT) { fire((int) (next & 15)); next++; nextT = next + ((next & 1) ? sw : 0); }
            } else {
                if (play && !playing) { playing = true; clock = 0; next = 0; nextT = 0; } else if (!play && playing) { playing = false; dstep = -1; }
                if (playing && clock >= nextT) { fire((int) (next & 15)); next++; nextT = next + ((next & 1) ? sw : 0); }
                clock += bpmv / 60 * 4 / fs;
            }
            double sl = 0, sr = 0;
            for (int tr = 0; tr < 8; ++tr) {
                lvS[tr] = (float) (lvS[tr] + .002 * (lvT[tr] - lvS[tr])); pnS[tr] = (float) (pnS[tr] + .002 * (pnT[tr] - pnS[tr])); auto& vv = v[tr]; if (!vv.on) continue;
                const double y = vv.sample() * lvS[tr]; const double pa = (pnS[tr] + 1) * .25 * PI; sl += y * std::cos(pa); sr += y * std::sin(pa);
            }
            ms += .004 * (mt - ms); L[i] = (float) std::tanh(sl * ms * 1.2); R[i] = (float) std::tanh(sr * ms * 1.2);
        }
    }
};

} // namespace oct
