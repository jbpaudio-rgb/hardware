// SP64: 64-pad sampler (4 banks of 16), built-in kit on bank A, chromatic mode. Port of web/samp-web/sp64.html (WORKLET), line for line.
// Samples are given to it by the plugin (setSlot); the built-in kit is made here, the same way as in the web engine.
#pragma once
#include "EngineBase.h"
#include "Sp64Params.h"
#include <memory>
#include <cstdint>

namespace oct {

struct SmpData { std::vector<float> L, R; double sr = 48000; };   // R empty: mono

struct Sp64 : EngineBase {
    static constexpr int CTV = 16, NPAD = 64, NV = 24;
    static double attS(double k) { return .0005 * std::pow(2000.0, k); }
    static double decS(double k) { return .02 * std::pow(200.0, k); }
    static double relS(double k) { return .005 * std::pow(400.0, k); }
    static double cutHz(double k) { return 20 * std::pow(1000.0, k); }

    // the built-in kit (bank A): 16 sounds made here, the same in the web app and the plugin
    static std::vector<float> kitGen(int k, double sr) {
        uint32_t s = (uint32_t) (k * 7919 + 17); auto r = [&] { s = s * 1664525u + 1013904223u; return s / 4294967296.0 * 2 - 1; };
        static const double DUR[16] = { .7, .4, .2, .7, .5, .12, .6, .5, .5, 2, .2, 1.2, .8, 1.5, 1.8, .3 };
        const int n = iround(DUR[k] * sr); std::vector<float> L((size_t) n, 0.f);
        double ph = 0, ph2 = 0, lp = 0, lp2 = 0, hp = 0, y = 0; const double C3 = 130.8128, C4 = 261.6256;
        std::vector<float> dl; if (k == 13) { dl.assign((size_t) iround(sr / C3), 0.f); for (auto& x : dl) x = (float) r(); } int di = 0;
        for (int i = 0; i < n; ++i) {
            const double t = i / sr;
            if (k == 0) { ph += (45 + 120 * std::exp(-t / .03)) / sr; y = std::sin(TP * ph) * std::exp(-t / .35) + r() * std::exp(-t / .002) * .3; }
            else if (k == 1) { ph += 190 / sr; const double w = r(); y = std::sin(TP * ph) * std::exp(-t / .08) * .5 + (w - lp) * std::exp(-t / .16) * .7; lp = w; }
            else if (k == 2 || k == 3) { const double w = r(); lp += .45 * (w - lp); y = (w - lp) * std::exp(-t / (k == 2 ? .035 : .28)); }
            else if (k == 4) { const double w = r(); lp += .3 * (w - lp); hp += .05 * (lp - hp); double e = 0; for (int b = 0; b < 3; ++b) { const double tb = t - b * .011; if (tb >= 0) e += std::exp(-tb / .006); } if (t >= .033) e += .8 * std::exp(-(t - .033) / .15); y = (lp - hp) * e * 1.5; }
            else if (k == 5) { ph += 1700 / sr; ph2 += 450 / sr; y = std::sin(TP * ph) * std::exp(-t / .012) + std::sin(TP * ph2) * std::exp(-t / .02) * .6; }
            else if (k == 6 || k == 7) { ph += (k == 6 ? 90 + 60 * std::exp(-t / .05) : 160 + 80 * std::exp(-t / .05)) / sr; y = std::sin(TP * ph) * std::exp(-t / (k == 6 ? .3 : .25)); }
            else if (k == 8) { ph += 540 / sr; ph2 += 800 / sr; const double q = (std::sin(TP * ph) >= 0 ? 1 : -1) + (std::sin(TP * ph2) >= 0 ? 1 : -1); lp += .25 * (q - lp); y = lp * .4 * (.7 * std::exp(-t / .01) + .3 * std::exp(-t / .15)); }
            else if (k == 9) { const double w = r(); lp += .5 * (w - lp); double m = 0; for (int j = 0; j < 6; ++j) m += std::sin(TP * t * (3150 + j * 1371.3)) >= 0 ? 1 : -1; y = ((w - lp) * .8 + m * .04) * std::exp(-t / .9); }
            else if (k == 10) { const double w = r(); lp += .55 * (w - lp); y = (w - lp) * std::min(1.0, t / .01) * std::exp(-t / .06); }
            else if (k == 11) { ph += C3 / 2 / sr; y = std::sin(TP * ph) * (1 - std::exp(-t / .005)) * std::exp(-t / .8); }
            else if (k == 12) { double q = 0; for (double f : { C4, C4 * 1.189207, C4 * 1.498307, C4 * 1.781797 }) q += 2 * std::fmod(t * f, 1.0) - 1; lp += .18 * (q - lp); lp2 += .18 * (lp - lp2); y = lp2 * .3 * std::exp(-t / .25) * std::min(1.0, t / .003); }
            else if (k == 13) { const float a = dl[(size_t) di], b = dl[(size_t) ((di + 1) % (int) dl.size())]; y = a; dl[(size_t) di] = (float) ((a + (double) b) * .498); di = (di + 1) % (int) dl.size(); }
            else if (k == 14) { ph += C4 * 2 / sr; ph2 += C4 * 7 / sr; y = std::sin(TP * ph + 2 * std::exp(-t / .3) * std::sin(TP * ph2)) * std::exp(-t / .9); }
            else { ph += (80 + 2000 * std::exp(-t / .05)) / sr; y = std::sin(TP * ph) * std::exp(-t / .12); }
            L[(size_t) i] = (float) y;
        }
        double pk = 0; for (float v : L) pk = std::max(pk, (double) std::fabs(v)); const double g = pk > 0 ? .9 / pk : 1; for (auto& v : L) v = (float) (v * g);
        const int f = std::min(n, iround(sr * .004)); for (int i = 0; i < f; ++i) L[(size_t) (n - 1 - i)] = (float) (L[(size_t) (n - 1 - i)] * ((double) i / f));
        return L;
    }
    static double herm(const std::vector<float>& d, int i, double f) {
        const int n = (int) d.size();
        const double a = i > 0 && i <= n ? d[(size_t) (i - 1)] : 0, b = i >= 0 && i < n ? d[(size_t) i] : 0, c = i + 1 >= 0 && i + 1 < n ? d[(size_t) (i + 1)] : 0, e = i + 2 >= 0 && i + 2 < n ? d[(size_t) (i + 2)] : 0;
        const double c1 = .5 * (c - a), c2 = a - 2.5 * b + 2 * c - .5 * e, c3 = .5 * (e - a) + 1.5 * (b - c); return ((c3 * f + c2) * f + c1) * f + b;
    }

    struct Voice { bool on = false; int pad = 0, key = 0; const SmpData* sm = nullptr; double pos = 0, inc = 0; int a = 0, b = 0, stage = 0; double g = 0, ai = 1, dk = 1, rk = 1, s1[2] {}, s2[2] {}, fg = 0, fk = 2; bool byp = true; double gl = 0, gr = 0, vel = 1; int semis = 0; long age = 0; int grp = 0, mode = 0; bool rev = false; };
    std::shared_ptr<const SmpData> slot[NPAD]; bool userSlot[NPAD] {};
    Voice v[NV]; long age = 0; uint64_t hitMask = 0; double vel = .8; int note = 60;
    float src[15] {}; double lp[4] {}, lr[4] {}, shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0; int stp = 0;
    double ms = 0, mt = 0; long mc = 0;
    int iK[NPAD][15] {}; enum { SRC_, ST, EN, TUNE, LVL, PAN, ATT, DEC, REL, CUT, RES, REV, MODE, GRP, VEL };
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {};

    Sp64() {
        setTable(sp64::PDEFS, sp64::DSTP, sp64::NP);
        static const char* SUF[15] = { "src", "st", "en", "tune", "lvl", "pan", "att", "dec", "rel", "cut", "res", "rev", "mode", "grp", "vel" };
        for (int k = 0; k < NPAD; ++k) for (int j = 0; j < 15; ++j) iK[k][j] = id("k" + std::to_string(k) + SUF[j]);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        prepare(48000);
    }
    // the kit slots that hold no sample of the user's are (re)made at the engine's rate
    void prepare(double sr) override {
        fs = sr; rs = 246813579;
        for (int k = 0; k < NPAD; ++k) if (!userSlot[k]) { if (k < 16) { auto d = std::make_shared<SmpData>(); d->L = kitGen(k, fs); d->sr = fs; slot[k] = d; } else slot[k].reset(); }
        for (auto& w : v) w = Voice(); age = 0; hitMask = 0; vel = .8; note = 60;
        for (auto& x : src) x = 0; const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        shp = sht = shv = sh2p = sh2t = sh2v = clock = 0; stp = 0; ms = mt = 0; mc = 0;
    }
    // a slot gets the user's sample (d), nothing (d null, user true) or the kit back (user false). Returns what it held, to be freed off the audio thread.
    std::shared_ptr<const SmpData> setSlot(int s, std::shared_ptr<const SmpData> d, bool user) {
        if (s < 0 || s >= NPAD) return {};
        kill(s); auto old = slot[s]; userSlot[s] = user;
        if (user) slot[s] = std::move(d);
        else if (s < 16) { if (d) slot[s] = std::move(d); else { auto k = std::make_shared<SmpData>(); k->L = kitGen(s, fs); k->sr = fs; slot[s] = k; } }   // the kit sound back: made beforehand by the plugin, or here
        else slot[s].reset();
        return old;
    }
    int step() const override { return stp; }
    int meters(float* m) override { for (int b = 0; b < 4; ++b) m[b] = (float) ((hitMask >> (16 * b)) & 0xffff); hitMask = 0; return 4; }
    double vi(int i) const { return val(i); }
    void kill(int s) { for (auto& x : v) if (x.on && x.sm == slot[s].get()) x.on = false; }
    void noteOn(int n, double vl) override { vel = vl; note = n; const int ch = ri(sp64::chrom); if (ch > 0) trig(ch - 1, vl, n, n - 60); else if (n >= 36 && n < 36 + NPAD) trig(n - 36, vl, n, 0); }
    void noteOff(int n) override { release(n); }
    void pad(int k) override { padOn(k, .85); }
    void padOn(int k, double vl) override { if (k >= 0 && k < NPAD) trig(k, vl, 1000 + k, 0); }
    void padOff(int k) override { release(1000 + k); }
    void allOff() override { for (auto& x : v) if (x.on && x.stage < 2) { x.stage = 2; x.rk = std::exp(-1 / (.01 * fs)); } }
    void release(int key) { for (auto& x : v) if (x.on && x.key == key && x.stage < 2 && x.mode > 0) x.stage = 2; }
    void trig(int k, double vl, int key, int semis) {
        const SmpData* sm = slot[ri(iK[k][SRC_])].get(); hitMask |= (uint64_t) 1 << k; markHit(k); if (!sm || sm->L.empty()) return;
        const int grp = ri(iK[k][GRP]);
        if (grp > 0) for (auto& y : v) if (y.on && y.grp == grp && y.stage < 2) { y.stage = 2; y.rk = std::exp(-1 / (.004 * fs)); }
        Voice* w = nullptr; for (auto& y : v) if (!y.on) { w = &y; break; }
        if (!w) { w = &v[0]; for (auto& y : v) if (y.age < w->age) w = &y; }
        w->on = true; w->pad = k; w->key = key; w->sm = sm; w->vel = vl; w->semis = semis; w->age = ++age; w->grp = grp; w->stage = 0; w->g = 0; w->s1[0] = w->s1[1] = w->s2[0] = w->s2[1] = 0;
        w->mode = ri(iK[k][MODE]); w->rev = p[(size_t) iK[k][REV]] > .5;
        const int len = (int) sm->L.size(); int a = (int) std::floor(vi(iK[k][ST]) * len), b = (int) std::floor(vi(iK[k][EN]) * len); if (b < a) std::swap(a, b); w->a = a; w->b = b;
        w->pos = w->rev ? b - 1 : a; coef(*w); if (b - a < 2) w->on = false;
    }
    void coef(Voice& w) {
        const int* K = iK[w.pad];
        const double tune = vi(K[TUNE]) + (val(sp64::tune) + bend) + w.semis; w.inc = w.sm->sr / fs * std::pow(2.0, tune / 12) * (w.rev ? -1 : 1);
        const double c = cl(vi(K[CUT]) + val(sp64::gcut), 0, 1); w.byp = c >= .999; if (!w.byp) { w.fg = std::tan(PI * std::min(cutHz(c), fs * .45) / fs); w.fk = 2 - 1.94 * vi(K[RES]); }
        const double lv = vi(K[LVL]), vs = vi(K[VEL]); const double amp = lv * lv * (1 - vs + vs * w.vel); const double pa = (vi(K[PAN]) + 1) * .25 * PI; w.gl = std::cos(pa) * amp; w.gr = std::sin(pa) * amp;
        w.ai = 1 / std::max(1.0, attS(vi(K[ATT])) * fs);
        const double d = cl(vi(K[DEC]) + val(sp64::gdec), 0, 1); w.dk = d >= .999 ? 1 : std::exp(-1 / (decS(d) * fs));
        if (w.stage < 2) w.rk = std::exp(-1 / (relS(vi(K[REL])) * fs));
    }
    void ctrl() {
        using namespace sp64; const double dt = CTV / fs; float* s = src;
        clock = advance(clock, g(bpm), dt); stp = ((int) std::floor(clock)) & 15;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph_ = lp[i];
            const double W[5] = { std::sin(TP * ph_), 1 - 4 * std::fabs(ph_ - .5), 2 * ph_ - 1, ph_ < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 13] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(shrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(shsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(sh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = g(sh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[14] = (float) sh2v;
        s[11] = (float) vel; s[12] = (float) ((note - 60) / 24.0); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        for (auto& w : v) if (w.on) coef(w);
        mt = val(master);
    }
    void process(float* L, float* R, int n) override {
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            double sl = 0, sr = 0;
            for (auto& w : v) {
                if (!w.on) continue; const SmpData& sm = *w.sm; const int ip = (int) std::floor(w.pos); const double f = w.pos - ip;
                double yl = herm(sm.L, ip, f), yr = !sm.R.empty() ? herm(sm.R, ip, f) : yl;
                if (w.stage == 0) { w.g += w.ai; if (w.g >= 1) { w.g = 1; w.stage = 1; } }
                else if (w.stage == 1) { w.g *= w.dk; if (w.g < 1e-4) { w.on = false; continue; } }
                else { w.g *= w.rk; if (w.g < 1e-4) { w.on = false; continue; } }
                const bool loop = w.mode == 2; const double ai = std::fabs(w.inc); const double rem = w.inc >= 0 ? (w.b - w.pos) / ai : (w.pos - w.a) / ai; const double fe = !loop && rem < 32 ? std::max(0.0, rem / 32) : 1;
                if (!w.byp) {
                    const double g = w.fg, k = w.fk, a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                    double v3 = yl - w.s2[0], v1 = a1 * w.s1[0] + a2 * v3, v2 = w.s2[0] + a2 * w.s1[0] + a3 * v3; w.s1[0] = 2 * v1 - w.s1[0]; w.s2[0] = 2 * v2 - w.s2[0]; yl = v2;
                    v3 = yr - w.s2[1]; v1 = a1 * w.s1[1] + a2 * v3; v2 = w.s2[1] + a2 * w.s1[1] + a3 * v3; w.s1[1] = 2 * v1 - w.s1[1]; w.s2[1] = 2 * v2 - w.s2[1]; yr = v2;
                }
                const double e = w.g * fe; sl += yl * e * w.gl; sr += yr * e * w.gr;
                w.pos += w.inc;
                if (w.inc >= 0) { if (w.pos >= w.b) { if (loop) w.pos -= w.b - w.a; else w.on = false; } } else if (w.pos < w.a) { if (loop) w.pos += w.b - w.a; else w.on = false; }
            }
            ms += .004 * (mt - ms); L[i] = (float) std::tanh(sl * ms * 1.4); R[i] = (float) std::tanh(sr * ms * 1.4);
        }
    }
};

} // namespace oct
