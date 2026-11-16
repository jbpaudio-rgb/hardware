// DX7: 6-operator FM synth after the Yamaha DX7 (32 algorithms, operator feedback, the DX7's 4-rate / 4-level
// envelopes in its log domain, rate and level key scaling, velocity sensitivity, pitch envelope, LFO with delay,
// pitch and amplitude modulation sensitivity), 16 voices. The operators run at twice the sample rate (less aliasing
// on bright patches) from a sine table, as the DX7 does. LFOs, S&H, step sequencers and a modulation matrix as the
// other Octavio engines. Written for the hardware synth (box); not in the plugins' engine list.
// The envelope and level arithmetic follow the DX7's (as measured and documented by the Dexed / MSFA project):
// levels in 1/256 octave steps (0.0235 dB), rates as qrate = rate * 41 / 64 + scaling, attack proportional to the
// distance to the top, an output level step of 0.75 dB, a full scale modulator = 2 cycles of phase.
#pragma once
#include "EngineBase.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

namespace oct {

namespace dx7k {
constexpr int NOP = 6, NV = 16, NS = 16, OS = 2;
constexpr double DXFS = 49096;   // the DX7's own sample rate: its envelope speeds are counted in its samples

// the 32 algorithms: which operators each one modulates (bit k: operator k + 1), the carriers, and the feedback loop
// (from the output of operator fbFrom into the input of operator fbTo; the same one for the plain self feedback)
struct Alg { uint8_t mod[NOP]; uint8_t carriers; int fbFrom, fbTo; };
inline const Alg& alg(int a) {
    static Alg A[32]; static bool made = false;
    if (!made) {
        // edges "6>5 5>4", carriers "13", feedback "6>6"
        static const char* E[32][3] = {
            { "6>5 5>4 4>3 2>1", "13", "6>6" }, { "6>5 5>4 4>3 2>1", "13", "2>2" },
            { "6>5 5>4 3>2 2>1", "14", "6>6" }, { "6>5 5>4 3>2 2>1", "14", "4>6" },
            { "6>5 4>3 2>1", "135", "6>6" }, { "6>5 4>3 2>1", "135", "5>6" },
            { "6>5 5>3 4>3 2>1", "13", "6>6" }, { "6>5 5>3 4>3 2>1", "13", "4>4" }, { "6>5 5>3 4>3 2>1", "13", "2>2" },
            { "3>2 2>1 5>4 6>4", "14", "3>3" }, { "3>2 2>1 5>4 6>4", "14", "6>6" },
            { "2>1 4>3 5>3 6>3", "13", "2>2" }, { "2>1 4>3 5>3 6>3", "13", "6>6" },
            { "6>4 5>4 4>3 2>1", "13", "6>6" }, { "6>4 5>4 4>3 2>1", "13", "2>2" },
            { "6>5 4>3 5>1 3>1 2>1", "1", "6>6" }, { "6>5 4>3 5>1 3>1 2>1", "1", "2>2" },
            { "6>5 5>4 4>1 3>1 2>1", "1", "3>3" },
            { "3>2 2>1 6>5 6>4", "145", "6>6" },
            { "3>2 3>1 6>4 5>4", "124", "3>3" }, { "3>2 3>1 6>5 6>4", "1245", "3>3" },
            { "2>1 6>5 6>4 6>3", "1345", "6>6" }, { "3>2 6>5 6>4", "1245", "6>6" },
            { "6>5 6>4 6>3", "12345", "6>6" }, { "6>5 6>4", "12345", "6>6" },
            { "3>2 6>4 5>4", "124", "6>6" }, { "3>2 6>4 5>4", "124", "3>3" },
            { "2>1 5>4 4>3", "136", "5>5" }, { "4>3 6>5", "1235", "6>6" }, { "5>4 4>3", "1236", "5>5" },
            { "6>5", "12345", "6>6" }, { "", "123456", "6>6" },
        };
        for (int a = 0; a < 32; ++a) {
            Alg& x = A[a]; for (auto& m : x.mod) m = 0; x.carriers = 0;
            for (const char* s = E[a][0]; *s; ++s) if (s[1] == '>') { x.mod[s[0] - '1'] |= (uint8_t) (1 << (s[2] - '1')); s += 2; }
            for (const char* s = E[a][1]; *s; ++s) x.carriers |= (uint8_t) (1 << (*s - '1'));
            x.fbFrom = E[a][2][0] - '1'; x.fbTo = E[a][2][2] - '1';
        }
        made = true;
    }
    return A[a & 31];
}

// a sine table (one cycle, 4096 points and its guard point), read with linear interpolation
inline const float* sineTable() { static float T[4097]; static bool made = false; if (!made) { for (int i = 0; i <= 4096; ++i) T[i] = (float) std::sin(TP * i / 4096); made = true; } return T; }
inline double sine(double ph) {   // ph in cycles, any value
    const float* T = sineTable(); double x = ph - std::floor(ph); x *= 4096; const int i = (int) x; const double f = x - i; return T[i] + (T[i + 1] - T[i]) * f;
}
inline int scaleOut(int l) {   // the DX7's output level curve (0-99 -> 0-127, steeper at the bottom)
    static const int LO[20] = { 0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46 };
    l = std::clamp(l, 0, 99); return l >= 20 ? 28 + l : LO[l];
}

// the parameter table, made once (the ids are kept alive in a deque)
struct Table {
    std::deque<std::string> str; std::vector<PDef> d; std::vector<int> dstp; std::string dstNames;
    const char* keep(const std::string& s) { str.push_back(s); return str.back().c_str(); }
    void add(const std::string& id, const std::string& name, char type, float mn, float mx, float df, int nch = 0, const char* ch = "") { d.push_back({ keep(id), keep(name), type, mn, mx, df, nch, ch }); }
    int find(const std::string& id) const { for (size_t i = 0; i < d.size(); ++i) if (id == d[i].id) return (int) i; return -1; }
};
// the starting sound: a DX electric piano (algorithm 5)
struct OpDef { int coarse, fine, det, lvl, vel, rs, kls, r[4], l[4]; };
static const OpDef EP[NOP] = {
    { 1, 0, 3, 99, 2, 3, 0, { 96, 25, 25, 67 }, { 99, 75, 0, 0 } },
    { 14, 0, 0, 58, 7, 3, 20, { 95, 50, 35, 78 }, { 99, 75, 0, 0 } },
    { 1, 0, 0, 99, 2, 3, 0, { 95, 20, 20, 50 }, { 99, 95, 0, 0 } },
    { 1, 0, 0, 89, 6, 3, 10, { 95, 29, 20, 50 }, { 99, 95, 0, 0 } },
    { 1, 0, -2, 99, 0, 3, 0, { 95, 20, 20, 50 }, { 99, 95, 0, 0 } },
    { 1, 0, 2, 79, 6, 3, 10, { 95, 29, 20, 50 }, { 99, 95, 0, 0 } },
};
inline const Table& table() {
    static Table T; if (!T.d.empty()) return T;
    static const char* ALGS = "1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32";
    static const char* COARSE = "0.50|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31";
    static const char* SRC = "---|LFO 1|LFO 2|LFO 3|S&H|SEQ 1|SEQ 2|SEQ 3|SEQ 4|SEQ 5|SEQ 6|VELOCITY|NOTE|LFO 4|S&H 2";
    T.add("alg", "ALGORITHM", 'c', 0, 31, 4, 32, ALGS); T.add("fb", "FEEDBACK", 'f', 0, 7, 6, 0, "#");
    T.add("bright", "BRIGHTNESS", 'f', -1, 1, 0); T.add("lvl", "LEVEL", 'f', 0, 1, .8f);
    T.add("tune", "TUNE", 'f', -1, 1, 0); T.add("porta", "PORTAMENTO", 'f', 0, 1, 0); T.add("mono", "MONO", 'b', 0, 1, 0);
    T.add("lspd", "LFO SPEED", 'f', 0, 99, 34, 0, "#"); T.add("ldel", "LFO DELAY", 'f', 0, 99, 33, 0, "#"); T.add("lpmd", "PITCH MOD DEPTH", 'f', 0, 99, 0, 0, "#");
    T.add("lamd", "AMP MOD DEPTH", 'f', 0, 99, 0, 0, "#"); T.add("lwave", "LFO WAVE", 'c', 0, 5, 0, 6, "TRIANGLE|SAW DOWN|SAW UP|SQUARE|SINE|S&H");
    T.add("lsync", "LFO KEY SYNC", 'b', 0, 1, 1); T.add("pms", "PITCH MOD SENS", 'c', 0, 7, 3, 8, "0|1|2|3|4|5|6|7");
    for (int k = 0; k < 4; ++k) T.add("pr" + std::to_string(k + 1), "PITCH RATE " + std::to_string(k + 1), 'f', 0, 99, 99, 0, "#");
    for (int k = 0; k < 4; ++k) T.add("pl" + std::to_string(k + 1), "PITCH LEVEL " + std::to_string(k + 1), 'f', 0, 99, 50, 0, "#");
    for (int o = 0; o < NOP; ++o) {
        const std::string x = "o" + std::to_string(o + 1); const OpDef& k = EP[o];
        T.add(x + "coarse", "RATIO", 'c', 0, 31, (float) k.coarse, 32, COARSE); T.add(x + "fine", "FINE", 'f', 0, 99, (float) k.fine, 0, "#");
        T.add(x + "det", "DETUNE", 'f', -7, 7, (float) k.det, 0, "#"); T.add(x + "lvl", "OUTPUT LEVEL", 'f', 0, 99, (float) k.lvl, 0, "#");
        T.add(x + "vel", "VELOCITY", 'f', 0, 7, (float) k.vel, 0, "#"); T.add(x + "rs", "RATE SCALING", 'f', 0, 7, (float) k.rs, 0, "#");
        T.add(x + "kls", "KEY LEVEL SCALING", 'f', 0, 99, (float) k.kls, 0, "#"); T.add(x + "ams", "AMP MOD SENS", 'c', 0, 3, 0, 4, "0|1|2|3");
        for (int j = 0; j < 4; ++j) T.add(x + "r" + std::to_string(j + 1), "RATE " + std::to_string(j + 1), 'f', 0, 99, (float) k.r[j], 0, "#");
        for (int j = 0; j < 4; ++j) T.add(x + "l" + std::to_string(j + 1), "LEVEL " + std::to_string(j + 1), 'f', 0, 99, (float) k.l[j], 0, "#");
        T.add(x + "on", "ON", 'b', 0, 1, 1);
    }
    for (int i = 0; i < 4; ++i) { T.add("l" + std::to_string(i) + "rate", "LFO " + std::to_string(i + 1), 'f', 0, 1, .35f + .1f * i); T.add("l" + std::to_string(i) + "shape", "SHAPE", 'c', 0, 4, (float) i, 5, "SINE|TRI|SAW|SQUARE|RANDOM"); }
    T.add("shrate", "S&H RATE", 'f', 0, 1, .55f); T.add("shsm", "SMOOTH", 'f', 0, 1, 0); T.add("sh2rate", "S&H 2 RATE", 'f', 0, 1, .4f); T.add("sh2sm", "SMOOTH", 'f', 0, 1, .3f);
    // the modulation destinations
    std::vector<std::pair<std::string, std::string>> dst { { "", "---" }, { "tune", "TUNE" }, { "fb", "FEEDBACK" }, { "bright", "BRIGHTNESS" } };
    for (int o = 0; o < NOP; ++o) dst.push_back({ "o" + std::to_string(o + 1) + "lvl", "OP " + std::to_string(o + 1) + " LEVEL" });
    for (auto x : { std::pair<const char*, const char*> { "lspd", "LFO SPEED" }, { "lpmd", "PITCH MOD" }, { "lamd", "AMP MOD" }, { "lvl", "VOLUME" },
                    { "l0rate", "LFO 1 RATE" }, { "l1rate", "LFO 2 RATE" }, { "l2rate", "LFO 3 RATE" }, { "shrate", "S&H RATE" }, { "l3rate", "LFO 4 RATE" }, { "sh2rate", "S&H 2 RATE" } })
        dst.push_back({ x.first, x.second });
    for (size_t j = 0; j < dst.size(); ++j) { if (j) T.dstNames += "|"; T.dstNames += dst[j].second; }
    const float nd = (float) dst.size() - 1; const char* DNM = T.keep(T.dstNames);
    for (int q = 0; q < 6; ++q) {
        for (int s = 0; s < NS; ++s) T.add("s" + std::to_string(q) + "_" + std::to_string(s), "", 'f', -1, 1, 0);
        T.add("s" + std::to_string(q) + "dst", "DESTINATION", 'c', 0, nd, 0, (int) dst.size(), DNM); T.add("s" + std::to_string(q) + "amt", "AMOUNT", 'f', -1, 1, .5f);
    }
    for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); T.add(x + "src", "SOURCE", 'c', 0, 14, 0, 15, SRC); T.add(x + "dst", "DEST", 'c', 0, nd, 0, (int) dst.size(), DNM); T.add(x + "amt", "AMOUNT", 'f', -1, 1, 0); }
    T.add("bpm", "TEMPO", 'f', 40, 200, 120); T.add("master", "VOLUME", 'f', 0, 1, .7f);
    for (auto& x : dst) T.dstp.push_back(x.first.empty() ? -1 : T.find(x.first));
    return T;
}

// the DX7 envelope, in its log domain: lvl in octaves above silence (16 at full scale), the four stages, held at the
// third until the key goes up
struct Env {
    double lvl = 0, tgt = 0, inc = 0; int ix = 4; bool rising = false, down = false, hold = false;
    double R[4] {}, L[4] {};   // per-sample increments (octaves), target levels (octaves)
    void stage(int i) {
        ix = i; hold = false; if (ix > 3) return;
        tgt = L[ix]; inc = R[ix]; rising = tgt > lvl;
        if (rising && lvl < 1716 / 256.0) lvl = 1716 / 256.0;   // the attack starts from about -55 dB, as the DX7's
    }
    void on() { down = true; stage(0); }
    void off() { down = false; stage(3); }
    void refresh() { if (ix <= 3 && !hold) { tgt = L[ix]; inc = R[ix]; rising = tgt > lvl; } }   // a knob moved
    void next() { if (ix == 2 && down) hold = true; else stage(ix + 1); }   // held at level 3 until the key goes up
    double tick() {
        if (ix > 3 || hold) return lvl;
        if (rising) { lvl += std::floor(17 - lvl) * inc; if (lvl >= tgt) { lvl = tgt; next(); } }   // faster the further from the top
        else { lvl -= inc; if (lvl <= tgt) { lvl = tgt; next(); } }
        return lvl;
    }
    bool idle() const { return ix > 3 && lvl < 1.0; }
};

struct Voice {
    int note = -1, age = 0; bool on = false, held = false;
    double ph[NOP] {}, dph[NOP] {}, y[NOP] {}, fb[2] {}, vel = 1, curN = 60, tgtN = 60, det = 0, pegLvl = 0, pegTgt = 0; int pegIx = 4;
    Env env[NOP];
};
} // namespace dx7k

struct Dx7 : EngineBase {
    static constexpr int CTV = 16, NOP = dx7k::NOP, NV = dx7k::NV, NS = dx7k::NS;
    struct OpI { int coarse, fine, det, lvl, vel, rs, kls, ams, r[4], l[4], on; } OP[NOP] {};
    dx7k::Voice v[NV];
    int iAlg, iFb, iBright, iLvl, iTune, iPorta, iMono, iLspd, iLdel, iLpmd, iLamd, iLwave, iLsync, iPms, iPr[4], iPl[4];
    int LR[4] {}, LS[4] {}, SQ[6][NS] {}, SD[6] {}, SA[6] {}, MS[8] {}, MD[8] {}, MA[8] {}, iBpm = 0, iMaster = 0, iShr = 0, iShs = 0, iSh2r = 0, iSh2s = 0;
    int ndst = 0, stp = 0, age = 0;
    float src[15] {}, lp[4] {}, lr[4] {};
    double shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0, ms = 0, mt = .7, lastVel = 0, lastNote = 0;
    double lfoPh = 0, lfoVal = 0, lfoSH = 0, lfoDelay = 0, lfoDelT = 0; long mc = 0;
    double pmSemis = 0, amOct = 0, glide = 1;
    std::array<double, 64> dec[2] {}; int decP = 0;   // the 2x decimator's history (one per channel)
    std::vector<float> fir;

    Dx7() {
        const auto& T = dx7k::table(); setTable(T.d.data(), T.dstp.data(), (int) T.d.size()); ndst = (int) T.dstp.size();
        iAlg = id("alg"); iFb = id("fb"); iBright = id("bright"); iLvl = id("lvl"); iTune = id("tune"); iPorta = id("porta"); iMono = id("mono");
        iLspd = id("lspd"); iLdel = id("ldel"); iLpmd = id("lpmd"); iLamd = id("lamd"); iLwave = id("lwave"); iLsync = id("lsync"); iPms = id("pms");
        for (int k = 0; k < 4; ++k) { iPr[k] = id("pr" + std::to_string(k + 1)); iPl[k] = id("pl" + std::to_string(k + 1)); }
        for (int o = 0; o < NOP; ++o) {
            const std::string x = "o" + std::to_string(o + 1); auto& O = OP[o];
            O.coarse = id(x + "coarse"); O.fine = id(x + "fine"); O.det = id(x + "det"); O.lvl = id(x + "lvl"); O.vel = id(x + "vel"); O.rs = id(x + "rs");
            O.kls = id(x + "kls"); O.ams = id(x + "ams"); O.on = id(x + "on");
            for (int j = 0; j < 4; ++j) { O.r[j] = id(x + "r" + std::to_string(j + 1)); O.l[j] = id(x + "l" + std::to_string(j + 1)); }
        }
        for (int i = 0; i < 4; ++i) { LR[i] = id("l" + std::to_string(i) + "rate"); LS[i] = id("l" + std::to_string(i) + "shape"); }
        for (int q = 0; q < 6; ++q) { for (int s = 0; s < NS; ++s) SQ[q][s] = id("s" + std::to_string(q) + "_" + std::to_string(s)); SD[q] = id("s" + std::to_string(q) + "dst"); SA[q] = id("s" + std::to_string(q) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); MS[m] = id(x + "src"); MD[m] = id(x + "dst"); MA[m] = id(x + "amt"); }
        iBpm = id("bpm"); iMaster = id("master"); iShr = id("shrate"); iShs = id("shsm"); iSh2r = id("sh2rate"); iSh2s = id("sh2sm");
        // the 2x decimator: a 64-tap windowed sinc, cut a little under the output's Nyquist
        fir.resize(64); double s = 0;
        for (int i = 0; i < 64; ++i) { const double m = i - 31.5, fc = .44 / 2; double h = std::sin(TP * fc * m) / (PI * m); h *= .42 - .5 * std::cos(TP * i / 63) + .08 * std::cos(2 * TP * i / 63); fir[(size_t) i] = (float) h; s += h; }
        for (auto& h : fir) h = (float) (h / s);
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 314159265;
        for (auto& w : v) w = dx7k::Voice();
        for (auto& x : src) x = 0; const float L0[4] = { 0, .33f, .66f, .5f }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        shp = sht = shv = sh2p = sh2t = sh2v = 0; clock = 0; ms = 0; mt = .7; mc = 0; lfoPh = 0; lfoVal = 0; lfoDelT = 0; age = 0;
        for (auto& d : dec) d.fill(0); decP = 0;
    }
    int step() const override { return stp; }
    int meters(float* m) override { int n = 0; for (auto& w : v) n += w.on; m[0] = (float) n; return 1; }

    // ---------------- notes ----------------
    double rateInc(int o, double rate, int note) const {   // a stage's speed: octaves per (output) sample
        int q = (int) rate * 41 / 64; q += std::min(31, std::max(0, note / 3 - 7)) * (int) std::lround(val(OP[o].rs)) / 8; q = std::min(q, 63);
        return (4 + (q & 3)) * std::ldexp(1.0, 2 + (q >> 2)) / 16777216.0 * dx7k::DXFS / fs;
    }
    double outLevel(int o, int note, double vel) const {   // the operator's level on this note and velocity (1/256 oct units)
        const double l = cl(val(OP[o].lvl) + (o < NOP ? brightOf(o) : 0), 0, 99);
        double lf = dx7k::scaleOut((int) l); lf += (dx7k::scaleOut((int) l + 1) - lf) * (l - std::floor(l));   // between the steps: no zipper
        double out = lf * 32;
        // velocity: up to about -24 dB at sensitivity 7 for a soft note
        const double vs = val(OP[o].vel), vv = vel <= 0 ? -239 : std::max(-239.0, 16 + 40 * std::log2(vel));
        out += vs * vv / 8 * 16;
        // key level scaling: quieter going up from middle C (up to -12 dB an octave)
        out -= val(OP[o].kls) / 99 * 12 / .0235 * std::max(0.0, (note - 60) / 12.0) * .5;
        return out;
    }
    double brightOf(int o) const {   // BRIGHTNESS: the modulators' levels up or down (the carriers keep theirs)
        const auto& A = dx7k::alg(ri(iAlg)); if (A.carriers & (1 << o)) return 0; return val(iBright) * 16;
    }
    void setEnv(dx7k::Voice& w, int o) {
        auto& E = w.env[o]; const double out = outLevel(o, w.note, w.vel);
        for (int j = 0; j < 4; ++j) {
            double a = (dx7k::scaleOut((int) val(OP[o].l[j])) >> 1) * 64 + out - 4256; if (a < 16) a = 16;
            E.L[j] = a / 256; E.R[j] = rateInc(o, val(OP[o].r[j]), w.note);
        }
    }
    void pitch(dx7k::Voice& w) {   // the operators' phase steps (at 2x), from the note, tuning, pitch EG, LFO, detune
        const double base = w.curN + val(iTune) + bend + w.pegLvl + pmSemis + w.det + vOsc[0];
        for (int o = 0; o < NOP; ++o) w.dph[o] = 440 * std::exp2((base - 69 + val(OP[o].det) * .01) / 12) * ratio(o) / (fs * dx7k::OS);
    }
    static double pegSemis(double l) { return (l - 50) / 50 * 48; }   // the pitch envelope's levels: +-4 octaves
    double pegSpeed(double r) const { return .25 * std::pow(2.0, r / 8.5) / fs; }   // semitones per sample
    void noteOn(int n, double vel) override {
        lastVel = vel; lastNote = n;
        const bool mono = p[(size_t) iMono] > .5;
        dx7k::Voice* w = nullptr;
        if (mono) { w = &v[0]; for (int k = 1; k < NV; ++k) if (v[k].on) { v[k].on = false; } }
        else {
            for (auto& x : v) if (x.note == n && x.on) { w = &x; break; }                    // the same note again: that voice
            if (!w) for (auto& x : v) if (!x.on) { w = &x; break; }                          // a free one
            if (!w) { int best = 0; for (int k = 1; k < NV; ++k) if (v[k].age < v[best].age) best = k; w = &v[best]; }   // the oldest
        }
        const bool legato = mono && w->on && w->held;
        w->note = n; w->age = ++age; w->on = true; w->held = true; w->vel = vel;
        w->tgtN = n; if (!legato && (val(iPorta) <= 0 || w->curN <= 0)) w->curN = n;
        w->det = (rnd() * 2 - 1) * vintage * .08;   // VINTAGE: each voice a little out of tune with the others
        if (!legato) {
            for (int o = 0; o < NOP; ++o) { setEnv(*w, o); w->env[o].on(); w->ph[o] = 0; w->y[o] = 0; }
            w->fb[0] = w->fb[1] = 0;
            w->pegLvl = pegSemis(val(iPl[3])); w->pegIx = 0; w->pegTgt = pegSemis(val(iPl[0]));
        }
        pitch(*w);
        if (p[(size_t) iLsync] > .5) { lfoPh = 0; lfoDelT = 0; }
    }
    void noteOff(int n) override {
        for (auto& w : v) if (w.on && w.held && w.note == n) { w.held = false; for (auto& e : w.env) e.off(); if (w.pegIx < 3) { w.pegIx = 3; w.pegTgt = pegSemis(val(iPl[3])); } }
    }
    void allOff() override { for (auto& w : v) { w.held = false; for (auto& e : w.env) e.off(); } }

    // ---------------- control rate ----------------
    static double lfo(double ph, int shape, double r) { switch (shape) { case 0: return std::sin(TP * ph); case 1: return 1 - 4 * std::fabs(ph - .5); case 2: return 2 * ph - 1; case 3: return ph < .5 ? 1 : -1; default: return r; } }
    void addModD(int si, int di, double amt) { if (si <= 0 || di <= 0 || amt == 0 || di >= ndst) return; const int pi = DSTP[di]; if (pi < 0) return; off[(size_t) pi] = (float) (off[(size_t) pi] + src[si] * amt * (D[pi].mx - D[pi].mn) * .5); }
    void ctrl() {
        const double dt = CTV / fs; float* s = src;
        clock = advance(clock, val(iBpm), dt); stp = ((int) std::floor(clock)) & 15; for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) SQ[k][stp]];
        for (int i = 0; i < 4; ++i) { lp[i] = (float) lfoAdv(i, lp[i], val(LR[i]), dt); if (lp[i] >= 1) { lp[i] = (float) (lp[i] - 1); lr[i] = (float) (rnd() * 2 - 1); } s[i < 3 ? 1 + i : 13] = (float) lfo(lp[i], ri(LS[i]), lr[i]); }
        shp += lfoHz(val(iShr)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = p[(size_t) iShs]; shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(iSh2r)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = p[(size_t) iSh2s]; sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[14] = (float) sh2v;
        s[11] = (float) (lastVel * 2 - 1); s[12] = (float) ((lastNote - 60) / 24); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addModD(ri(MS[m]), ri(MD[m]), p[(size_t) MA[m]]);
        for (int k = 0; k < 6; ++k) addModD(5 + k, ri(SD[k]), p[(size_t) SA[k]]);
        mt = val(iMaster);
        // the DX7's own LFO: speed, delay (a fade in), wave, then how much it moves the pitch and the amplitude
        const double hz = .062 * std::pow(2.0, val(iLspd) / 11.2);
        lfoPh += hz * dt; bool wrap = false; if (lfoPh >= 1) { lfoPh -= 1; wrap = true; }
        if (wrap) lfoSH = rnd() * 2 - 1;
        switch (ri(iLwave)) { case 0: lfoVal = 1 - 4 * std::fabs(lfoPh - .5); break; case 1: lfoVal = 1 - 2 * lfoPh; break; case 2: lfoVal = 2 * lfoPh - 1; break;
                              case 3: lfoVal = lfoPh < .5 ? 1 : -1; break; case 4: lfoVal = std::sin(TP * lfoPh); break; default: lfoVal = lfoSH; }
        lfoDelT += dt; const double del = val(iLdel) / 99 * 5, fade = del <= 0 ? 1 : cl((lfoDelT - del * .5) / (del * .5), 0, 1);
        static const double PMS[8] = { 0, .08, .16, .3, .55, 1, 2.2, 6 };   // semitones at full depth
        pmSemis = lfoVal * fade * val(iLpmd) / 99 * PMS[std::clamp(ri(iPms), 0, 7)];
        amOct = (lfoVal * .5 + .5) * fade * val(iLamd) / 99 * 8;            // up to 8 octaves (48 dB) down at full depth
        const double pt = val(iPorta); glide = pt <= 0 ? 1 : 1 - std::exp(-dt / (.005 + pt * pt * 1.5));
        for (auto& w : v) {
            if (!w.on) continue;
            w.curN += glide * (w.tgtN - w.curN);
            for (int o = 0; o < NOP; ++o) {   // the levels follow the knobs while the note sounds (key scaling and velocity kept)
                auto& E = w.env[o]; const double out = outLevel(o, w.note, w.vel);
                for (int j = 0; j < 4; ++j) { double a = (dx7k::scaleOut((int) val(OP[o].l[j])) >> 1) * 64 + out - 4256; if (a < 16) a = 16; E.L[j] = a / 256; E.R[j] = rateInc(o, val(OP[o].r[j]), w.note); }
                E.refresh();
            }
            // the operators' frequencies (phase steps at 2x), for the next CTV samples
            pitch(w);
            // the pitch envelope
            if (w.pegIx <= 3) {
                const double sp = pegSpeed(val(iPr[w.pegIx])) * CTV;
                if (std::fabs(w.pegTgt - w.pegLvl) <= sp) { w.pegLvl = w.pegTgt; if (w.pegIx < 2 || (!w.held && w.pegIx < 3)) { ++w.pegIx; w.pegTgt = pegSemis(val(iPl[w.pegIx])); } else if (w.pegIx == 3) w.pegIx = 4; }
                else w.pegLvl += w.pegLvl < w.pegTgt ? sp : -sp;
            }
        }
    }

    // ---------------- audio ----------------
    double ratio(int o) const { const int c = ri(OP[o].coarse); return (c == 0 ? .5 : c) * (1 + val(OP[o].fine) / 100); }
    void process(float* L, float* R, int n) override {
        const auto& A = dx7k::alg(ri(iAlg)); const double fbAmt = val(iFb) <= 0 ? 0 : std::pow(2.0, val(iFb) - 8);
        int nc = 0; for (int o = 0; o < NOP; ++o) nc += (A.carriers >> o) & 1;
        const double cScale = 1 / (1 + .45 * (nc - 1));
        bool opOn[NOP]; double amS[NOP]; static const double AMS[4] = { 0, .3, .6, 1 };
        for (int o = 0; o < NOP; ++o) { opOn[o] = p[(size_t) OP[o].on] > .5; amS[o] = AMS[std::clamp(ri(OP[o].ams), 0, 3)]; }
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            double acc[dx7k::OS] {};
            for (auto& w : v) {
                if (!w.on) continue;
                double g[NOP];
                for (int o = 0; o < NOP; ++o) { const double l = w.env[o].tick(); g[o] = opOn[o] ? std::exp2(l - amOct * amS[o] - 14) : 0; }
                bool any = false; for (int o = 0; o < NOP; ++o) if ((A.carriers >> o) & 1) any = any || !w.env[o].idle();
                if (!any && !w.held) { w.on = false; continue; }
                const double* dph = w.dph;
                for (int q = 0; q < dx7k::OS; ++q) {
                    double in[NOP] {}, out = 0;
                    in[A.fbTo] += (w.fb[0] + w.fb[1]) * .5 * fbAmt;   // the feedback: the mean of the last two outputs, as the DX7
                    for (int o = NOP - 1; o >= 0; --o) {
                        w.ph[o] += dph[o]; if (w.ph[o] >= 1) w.ph[o] -= std::floor(w.ph[o]);
                        const double y = dx7k::sine(w.ph[o] + in[o]) * g[o]; w.y[o] = y;
                        if (o == A.fbFrom) { w.fb[1] = w.fb[0]; w.fb[0] = y; }
                        const uint8_t m = A.mod[o]; if (m) for (int k = 0; k < o; ++k) if (m & (1 << k)) in[k] += y;
                        if ((A.carriers >> o) & 1) out += y;
                    }
                    acc[q] += out * .5 * cScale;
                }
            }
            // 2x -> 1x
            for (int q = 0; q < dx7k::OS; ++q) { dec[0][(size_t) decP] = acc[q]; decP = (decP + 1) & 63; }
            double y = 0; for (int k = 0; k < 64; ++k) y += fir[(size_t) k] * dec[0][(size_t) ((decP + k) & 63)];
            ms += .004 * (mt * val(iLvl) - ms);
            const double o = std::tanh(y * ms * 1.1);
            L[i] = R[i] = (float) o;
        }
    }
};

} // namespace oct
