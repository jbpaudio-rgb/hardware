// DFAM: 8 tracks of a Moog DFAM (Drummer From Another Mother) style percussion voice, laid out like the DR8:
// each track has 2 VCOs (triangle / square, 1-2 FM, hard sync), noise, a 4-pole ladder filter (low or high pass),
// the three decay envelopes of the DFAM (VCO pitch, VCF, VCA with a fast or slow attack), and a 16-step pattern whose
// steps also have a pitch, as the DFAM's pitch sequencer. LFOs, S&H, modulation matrix and step sequencers as the
// other Octavio engines. Written for the hardware synth (box); not in the plugins' engine list.
#pragma once
#include "LadderTune.h"
#include "EngineBase.h"
#include <array>
#include <deque>
#include <string>
#include <vector>

namespace oct {

namespace dfamk {
constexpr int NT = 8, NS = 16;
inline double th(double x) { if (x > 3) return 1; if (x < -3) return -1; const double x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
inline double blep(double t, double dt) { return oct::blepAny(t, dt); }
// the parameter table, made once (the ids are kept alive in a deque)
struct Table {
    std::deque<std::string> str; std::vector<PDef> d; std::vector<int> dstp; std::string dstNames;
    const char* keep(const std::string& s) { str.push_back(s); return str.back().c_str(); }
    void add(const std::string& id, const std::string& name, char type, float mn, float mx, float df, int nch = 0, const char* ch = "") { d.push_back({ keep(id), keep(name), type, mn, mx, df, nch, ch }); }
    int find(const std::string& id) const { for (size_t i = 0; i < d.size(); ++i) if (id == d[i].id) return (int) i; return -1; }
};
// a starting kit: kick, snare, hat, tom, zap, metal, a bass line, a blip
struct Def { float f1, w1, l1, f2, w2, l2, fm, sync, noise, cut, res, fmode, feg, fdec, nmod, veg, vdec, vatt, adec, spa, lvl; const char* pat; float pitch[NS]; };
static const Def KIT[NT] = {
    { -24, 0, 1, -12, 0, 0, 0, 0, 0, .72f, .1f, 0, .2f, .3f, 0, .6f, .12f, 0, .38f, 0, .9f, "2000100020001000", {} },
    { -5, 0, .45f, 2, 0, .2f, 0, 0, .9f, .58f, .25f, 1, .3f, .2f, .2f, .3f, .08f, 0, .2f, 0, .75f, "0000200000002010", {} },
    { 12, 1, 0, 19, 1, 0, 0, 0, 1, .8f, .3f, 1, .1f, .1f, .3f, 0, .1f, 0, .08f, 0, 1, "0101010201010102", {} },
    { -10, 1, .8f, -3, 0, .3f, .2f, 0, .1f, .5f, .6f, 0, .35f, .25f, 0, .3f, .2f, 0, .26f, .3f, .7f, "0001000100100010", { 0, 0, 0, .2f, 0, 0, 0, -.2f, 0, 0, .4f, 0, 0, 0, .1f, 0 } },
    { 12, 1, .9f, 0, 0, 0, 0, 0, 0, .6f, .72f, 0, .55f, .15f, 0, 1, .1f, 0, .14f, .4f, .55f, "0000001000010000", { 0, 0, 0, 0, 0, 0, .3f, 0, 0, 0, 0, -.4f, 0, 0, 0, 0 } },
    { 5, 1, 0, 18, 1, 1, .7f, 0, 0, .72f, .42f, 0, .25f, .2f, .1f, .1f, .15f, 0, .2f, .5f, .5f, "0010000001000001", { 0, 0, .5f, 0, 0, 0, 0, 0, 0, -.3f, 0, 0, 0, 0, 0, .7f } },
    { -12, 1, .8f, -11.9f, 0, .6f, 0, 0, 0, .34f, .55f, 0, .6f, .3f, 0, 0, .1f, 1, .3f, .5f, .7f, "1011011010110110", { 0, 0, .5f, 0, .25f, .5f, 0, -.25f, 0, .5f, .58f, 0, .25f, 0, .5f, .75f } },
    { 24, 0, .3f, 31, 1, .8f, 0, 1, 0, .8f, .2f, 0, .3f, .1f, 0, .2f, .05f, 0, .08f, .3f, .45f, "0000010000000100", { 0, 0, 0, 0, 0, .5f, 0, 0, 0, 0, 0, 0, 0, -.5f, 0, 0 } },
};
inline const Table& table() {
    static Table T; if (!T.d.empty()) return T;
    static const char* TW = "TRI|SQUARE"; static const char* LH = "LP|HP"; static const char* FS = "FAST|SLOW";
    static const char* SRC = "---|LFO 1|LFO 2|LFO 3|S&H|SEQ 1|SEQ 2|SEQ 3|SEQ 4|SEQ 5|SEQ 6|BAR RAMP|STEP RANDOM|LFO 4|S&H 2";
    static const char* STEP = "OFF|ON|ACC";
    for (int t = 0; t < NT; ++t) {
        const std::string x = "t" + std::to_string(t); const Def& k = KIT[t];
        T.add(x + "f1", "VCO 1 FREQ", 'f', -36, 36, k.f1); T.add(x + "w1", "VCO 1 WAVE", 'c', 0, 1, k.w1, 2, TW); T.add(x + "l1", "VCO 1 LEVEL", 'f', 0, 1, k.l1);
        T.add(x + "f2", "VCO 2 FREQ", 'f', -36, 36, k.f2); T.add(x + "w2", "VCO 2 WAVE", 'c', 0, 1, k.w2, 2, TW); T.add(x + "l2", "VCO 2 LEVEL", 'f', 0, 1, k.l2);
        T.add(x + "fm", "1-2 FM", 'f', 0, 1, k.fm); T.add(x + "sync", "HARD SYNC", 'b', 0, 1, k.sync); T.add(x + "noise", "NOISE", 'f', 0, 1, k.noise);
        T.add(x + "cut", "CUTOFF", 'f', 0, 1, k.cut); T.add(x + "res", "RESONANCE", 'f', 0, 1, k.res); T.add(x + "fmode", "VCF MODE", 'c', 0, 1, k.fmode, 2, LH);
        T.add(x + "feg", "VCF EG AMOUNT", 'f', -1, 1, k.feg); T.add(x + "fdec", "VCF DECAY", 'f', 0, 1, k.fdec); T.add(x + "nmod", "NOISE > VCF", 'f', 0, 1, k.nmod);
        T.add(x + "veg", "VCO EG AMOUNT", 'f', -1, 1, k.veg); T.add(x + "vdec", "VCO DECAY", 'f', 0, 1, k.vdec);
        T.add(x + "vatt", "VCA ATTACK", 'c', 0, 1, k.vatt, 2, FS); T.add(x + "adec", "VCA DECAY", 'f', 0, 1, k.adec);
        T.add(x + "spa", "SEQ PITCH", 'f', 0, 1, k.spa); T.add(x + "vel", "VELOCITY", 'f', 0, 1, .7f);
        T.add(x + "tune", "TUNE", 'f', -24, 24, 0); T.add(x + "lvl", "LEVEL", 'f', 0, 1, k.lvl); T.add(x + "pan", "PAN", 'f', -1, 1, t < 2 ? 0.f : t % 2 ? .15f : -.15f); T.add(x + "mute", "MUTE", 'b', 0, 1, 0);
        for (int s = 0; s < NS; ++s) T.add("p" + std::to_string(t) + "_" + std::to_string(s), "STEP", 'c', 0, 2, (float) (k.pat[s] - '0'), 3, STEP);
        for (int s = 0; s < NS; ++s) T.add("n" + std::to_string(t) + "_" + std::to_string(s), "PITCH", 'f', -1, 1, k.pitch[s]);
    }
    T.add("play", "PLAY", 'b', 0, 1, 0); T.add("swing", "SWING", 'f', 0, 1, 0); T.add("accent", "ACCENT", 'f', 0, 1, .5f);
    const char* SH[4] = { "SINE", "TRI", "SAW", "SQUARE" };
    for (int i = 0; i < 4; ++i) { T.add("l" + std::to_string(i) + "rate", "LFO " + std::to_string(i + 1), 'f', 0, 1, .35f + .1f * i); T.add("l" + std::to_string(i) + "shape", "SHAPE", 'c', 0, 4, (float) i, 5, "SINE|TRI|SAW|SQUARE|RANDOM"); (void) SH; }
    T.add("shrate", "S&H RATE", 'f', 0, 1, .55f); T.add("shsm", "SMOOTH", 'f', 0, 1, 0); T.add("sh2rate", "S&H 2 RATE", 'f', 0, 1, .4f); T.add("sh2sm", "SMOOTH", 'f', 0, 1, .3f);
    // the modulation destinations: per track VCO 1, VCO 2, FM, cutoff, resonance, VCF EG, VCA decay, noise; swing, accent
    static const char* DN[8] = { "f1", "f2", "fm", "cut", "res", "feg", "adec", "noise" }; static const char* DL[8] = { "VCO 1", "VCO 2", "FM", "CUTOFF", "RES", "VCF EG", "VCA DEC", "NOISE" };
    std::vector<std::pair<std::string, std::string>> dst { { "", "---" } };
    for (int t = 0; t < NT; ++t) for (int j = 0; j < 8; ++j) dst.push_back({ "t" + std::to_string(t) + DN[j], "T" + std::to_string(t + 1) + " " + DL[j] });
    dst.push_back({ "swing", "SWING" }); dst.push_back({ "accent", "ACCENT" });
    for (size_t j = 0; j < dst.size(); ++j) { if (j) T.dstNames += "|"; T.dstNames += dst[j].second; T.dstp.push_back(dst[j].first.empty() ? -1 : T.find(dst[j].first)); }
    const float nd = (float) dst.size() - 1; const char* DNM = T.keep(T.dstNames);
    for (int q = 0; q < 6; ++q) {
        for (int s = 0; s < NS; ++s) T.add("s" + std::to_string(q) + "_" + std::to_string(s), "", 'f', -1, 1, 0);
        T.add("s" + std::to_string(q) + "dst", "DESTINATION", 'c', 0, nd, 0, (int) dst.size(), DNM); T.add("s" + std::to_string(q) + "amt", "AMOUNT", 'f', -1, 1, .5f);
    }
    for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); T.add(x + "src", "SOURCE", 'c', 0, 14, 0, 15, SRC); T.add(x + "dst", "DEST", 'c', 0, nd, 0, (int) dst.size(), DNM); T.add(x + "amt", "AMOUNT", 'f', -1, 1, 0); }
    T.add("bpm", "TEMPO", 'f', 40, 200, 120); T.add("master", "VOLUME", 'f', 0, 1, .7f);
    // the destinations were looked up before the later controls existed: look them up again
    T.dstp.clear(); for (auto& x : dst) T.dstp.push_back(x.first.empty() ? -1 : T.find(x.first));
    return T;
}

// one DFAM voice
struct Voice {
    double fs = 48000, ph1 = 0, ph2 = .37, y[4] {}, zs[4] {}, ev = 0, ef = 0, ea = 0, att = 0, kv = 1, kf = 1, ka = 1, vel = 1, pitch = 0; bool on = false, rise = false;
    uint32_t rs = 77;
    double noise() { rs = rs * 1664525u + 1013904223u; return rs / 2147483648.0 - 1; }
    static double decS(double k) { return .004 * std::pow(500.0, k); }   // 4 ms .. 2 s
};
} // namespace dfamk

struct Dfam : EngineBase {
    static constexpr int CTV = 16, NT = dfamk::NT, NS = dfamk::NS;
    struct TrI { int f1, w1, l1, f2, w2, l2, fm, sync, noise, cut, res, fmode, feg, fdec, nmod, veg, vdec, vatt, adec, spa, vel, tune, lvl, pan, mute, pat[NS], pit[NS]; } TR[NT] {};
    dfamk::Voice v[NT];
    int LR[4] {}, LS[4] {}, SQ[6][NS] {}, SD[6] {}, SA[6] {}, MS[8] {}, MD[8] {}, MA[8] {}, iPlay = 0, iSwing = 0, iAcc = 0, iBpm = 0, iMaster = 0, iShr = 0, iShs = 0, iSh2r = 0, iSh2s = 0;
    int ndst = 0;
    float lvS[NT] {}, pnS[NT] {}, src[15] {}, lp[4] {}, lr[4] {};
    double shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0, nextT = 0, srnd = 0, ms = 0, mt = .7, bpmv = 120, sw = 0, acc = .5, lastClock = 0;
    long next = 0, mc = 0; bool playing = false; int dstep = -1, stp = 0; unsigned hitMask = 0;

    Dfam() {
        const auto& T = dfamk::table(); setTable(T.d.data(), T.dstp.data(), (int) T.d.size()); ndst = (int) T.dstp.size();
        for (int t = 0; t < NT; ++t) {
            const std::string x = "t" + std::to_string(t); auto& R = TR[t];
            R.f1 = id(x + "f1"); R.w1 = id(x + "w1"); R.l1 = id(x + "l1"); R.f2 = id(x + "f2"); R.w2 = id(x + "w2"); R.l2 = id(x + "l2"); R.fm = id(x + "fm"); R.sync = id(x + "sync");
            R.noise = id(x + "noise"); R.cut = id(x + "cut"); R.res = id(x + "res"); R.fmode = id(x + "fmode"); R.feg = id(x + "feg"); R.fdec = id(x + "fdec"); R.nmod = id(x + "nmod");
            R.veg = id(x + "veg"); R.vdec = id(x + "vdec"); R.vatt = id(x + "vatt"); R.adec = id(x + "adec"); R.spa = id(x + "spa"); R.vel = id(x + "vel"); R.tune = id(x + "tune");
            R.lvl = id(x + "lvl"); R.pan = id(x + "pan"); R.mute = id(x + "mute");
            for (int s = 0; s < NS; ++s) { R.pat[s] = id("p" + std::to_string(t) + "_" + std::to_string(s)); R.pit[s] = id("n" + std::to_string(t) + "_" + std::to_string(s)); }
        }
        for (int i = 0; i < 4; ++i) { LR[i] = id("l" + std::to_string(i) + "rate"); LS[i] = id("l" + std::to_string(i) + "shape"); }
        for (int q = 0; q < 6; ++q) { for (int s = 0; s < NS; ++s) SQ[q][s] = id("s" + std::to_string(q) + "_" + std::to_string(s)); SD[q] = id("s" + std::to_string(q) + "dst"); SA[q] = id("s" + std::to_string(q) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); MS[m] = id(x + "src"); MD[m] = id(x + "dst"); MA[m] = id(x + "amt"); }
        iPlay = id("play"); iSwing = id("swing"); iAcc = id("accent"); iBpm = id("bpm"); iMaster = id("master"); iShr = id("shrate"); iShs = id("shsm"); iSh2r = id("sh2rate"); iSh2s = id("sh2sm");
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 271828183;
        for (int t = 0; t < NT; ++t) { v[t] = dfamk::Voice(); v[t].fs = fs; v[t].rs = 1000u + 7919u * (uint32_t) t; lvS[t] = 0; pnS[t] = 0; }
        for (auto& x : src) x = 0; const float L0[4] = { 0, .33f, .66f, .5f }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        shp = sht = shv = sh2p = sh2t = sh2v = 0; clock = 0; next = 0; nextT = 0; playing = false; dstep = -1; stp = 0; srnd = 0; hitMask = 0; ms = 0; mt = .7; mc = 0; bpmv = 120; sw = 0; acc = .5; lastClock = 0;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = (float) dstep; m[1] = (float) hitMask; hitMask = 0; return 2; }
    void trigger(int t, double vel, double pitch) {
        const auto& R = TR[t]; if (p[(size_t) R.mute] > .5) return;
        auto& w = v[t]; w.on = true; w.rise = true; w.att = 0; w.ev = 1; w.ef = 1; w.vel = vel; w.pitch = pitch; hitMask |= 1u << t; markHit(t);
    }
    void noteOn(int n, double vel) override { trigger(((n - 36) % NT + NT) % NT, .9 * vel, 0); }
    void pad(int t) override { if (t >= 0 && t < NT) trigger(t, .9, 0); }
    void noteOff(int) override {}
    void allOff() override { for (auto& w : v) w.ea *= .1; }
    void fire(int st) {
        dstep = st; srnd = rnd() * 2 - 1;
        for (int t = 0; t < NT; ++t) { const int s = ri(TR[t].pat[st]); if (s > 0) trigger(t, s == 2 ? .78 * (1 + acc * .6) : .78, val(TR[t].pit[st])); }
    }
    static double lfo(double ph, int shape, double r) { switch (shape) { case 0: return std::sin(TP * ph); case 1: return 1 - 4 * std::fabs(ph - .5); case 2: return 2 * ph - 1; case 3: return ph < .5 ? 1 : -1; default: return r; } }
    void addModD(int si, int di, double amt) { if (si <= 0 || di <= 0 || amt == 0 || di >= ndst) return; const int pi = DSTP[di]; if (pi < 0) return; off[(size_t) pi] = (float) (off[(size_t) pi] + src[si] * amt * (D[pi].mx - D[pi].mn) * .5); }
    void ctrl() {
        const double dt = CTV / fs; float* s = src;
        stp = ((int) std::floor(clock)) & 15; for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) SQ[k][stp]];
        for (int i = 0; i < 4; ++i) { lp[i] = (float) lfoAdv(i, lp[i], val(LR[i]), dt); if (lp[i] >= 1) { lp[i] = (float) (lp[i] - 1); lr[i] = (float) (rnd() * 2 - 1); } s[i < 3 ? 1 + i : 13] = (float) lfo(lp[i], ri(LS[i]), lr[i]); }
        shp += lfoHz(val(iShr)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = p[(size_t) iShs]; shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(iSh2r)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = p[(size_t) iSh2s]; sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[14] = (float) sh2v;
        s[11] = (float) ((std::fmod(clock, 16.0) / 16) * 2 - 1); s[12] = (float) srnd; s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addModD(ri(MS[m]), ri(MD[m]), p[(size_t) MA[m]]);
        for (int k = 0; k < 6; ++k) addModD(5 + k, ri(SD[k]), p[(size_t) SA[k]]);
        bpmv = val(iBpm); sw = val(iSwing) * .5; acc = val(iAcc); mt = val(iMaster);
        for (int t = 0; t < NT; ++t) {   // the envelopes' rates
            auto& w = v[t]; const auto& R = TR[t];
            w.kv = std::exp(-1 / (dfamk::Voice::decS(val(R.vdec)) * fs)); w.kf = std::exp(-1 / (dfamk::Voice::decS(val(R.fdec)) * fs)); w.ka = std::exp(-1 / (dfamk::Voice::decS(val(R.adec)) * fs));
        }
    }
    double voice(int t) {
        auto& w = v[t]; const auto& R = TR[t];
        // envelopes: VCO and VCF decays, the VCA's attack (fast or slow) then decay
        w.ev *= w.kv; w.ef *= w.kf;
        if (w.rise) { w.att += 1 / ((ri(R.vatt) ? .025 : .0006) * fs); if (w.att >= 1) { w.att = 1; w.rise = false; } w.ea = w.att; } else w.ea *= w.ka;
        if (!w.rise && w.ea < 1e-5) { w.on = false; return 0; }
        const double semis = val(R.tune) + w.pitch * val(R.spa) * 24 + val(R.veg) * 48 * w.ev;
        const double f1 = cl(130.8128 * std::pow(2.0, (val(R.f1) + semis + vOsc[0]) / 12), .5, fs * .45), f2b = cl(130.8128 * std::pow(2.0, (val(R.f2) + semis + vOsc[1]) / 12), .5, fs * .45);
        // VCO 1, then VCO 2 (frequency modulated by VCO 1, hard synced to it)
        const double d1 = f1 / fs; w.ph1 += d1; bool wrap = false; if (w.ph1 >= 1) { w.ph1 -= 1; wrap = true; }
        double o1; if (ri(R.w1)) { double t2 = w.ph1 + .5; if (t2 >= 1) t2 -= 1; o1 = (w.ph1 < .5 ? 1 : -1) + dfamk::blep(w.ph1, d1) - dfamk::blep(t2, d1); } else o1 = 1 - 4 * std::fabs(w.ph1 - .5);
        const double d2 = cl(f2b * (1 + val(R.fm) * 3 * o1), -fs * .45, fs * .45) / fs; w.ph2 += d2; if (w.ph2 >= 1) w.ph2 -= 1; if (w.ph2 < 0) w.ph2 += 1;
        if (wrap && p[(size_t) R.sync] > .5) w.ph2 = w.ph1 * std::fabs(d2) / std::max(1e-9, d1);
        double o2; const double ad2 = std::fabs(d2); if (ri(R.w2)) { double t2 = w.ph2 + .5; if (t2 >= 1) t2 -= 1; o2 = (w.ph2 < .5 ? 1 : -1) + dfamk::blep(w.ph2, ad2) - dfamk::blep(t2, ad2); } else o2 = 1 - 4 * std::fabs(w.ph2 - .5);
        const double n = w.noise();
        const double x = o1 * val(R.l1) + o2 * val(R.l2) + n * val(R.noise);
        // the ladder: 4 poles, resonance from the 4th, low pass (y4) or high pass (the classic mix of the stages)
        const double fc = cl(20 * std::pow(2.0, val(R.cut) * 10 + val(R.feg) * 6 * w.ef + val(R.nmod) * 3 * n), 20, fs * .42);
        double kc; const double g = ladderG(fc, fs * 2, 4, &kc), k = val(R.res) * 3.9 * kc;
        double in = 0;
        if (classicSound()) for (int h = 0; h < 2; ++h) {   // twice a sample: the input held, the ladder at 2x
            in = dfamk::th(x * .6 - k * w.y[3]);
            w.y[0] += g * (in - w.y[0]); w.y[1] += g * (w.y[0] - w.y[1]); w.y[2] += g * (w.y[1] - w.y[2]); w.y[3] += g * (w.y[2] - w.y[3]);
        }
        else { const double zg = zdfG(fc, fs); zdfLadder<4>(w.zs, w.y, x * .6, zg, val(R.res) * 3.9, [](double u) { return dfamk::th(u); }, &in); }   // exact at 1x: no need for 2x
        const double f = ri(R.fmode) ? in - 4 * w.y[0] + 6 * w.y[1] - 4 * w.y[2] + w.y[3] : w.y[3];
        const double va = val(R.vel); return dfamk::th(f * 1.6) * w.ea * (1 - va + va * w.vel);
    }
    void process(float* L, float* R, int n) override {
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const bool play = p[(size_t) iPlay] > .5;
            if (hostClock) {
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
            for (int t = 0; t < NT; ++t) {
                const auto& Rt = TR[t]; lvS[t] = (float) (lvS[t] + .002 * (val(Rt.lvl) - lvS[t])); pnS[t] = (float) (pnS[t] + .002 * (val(Rt.pan) - pnS[t]));
                if (!v[t].on) continue;
                const double y = voice(t) * lvS[t]; const double pa = (pnS[t] + 1) * .25 * PI; sl += y * std::cos(pa); sr += y * std::sin(pa);
            }
            ms += .004 * (mt - ms); L[i] = (float) std::tanh(sl * ms * 2.6); R[i] = (float) std::tanh(sr * ms * 2.6);
        }
    }
};

} // namespace oct
