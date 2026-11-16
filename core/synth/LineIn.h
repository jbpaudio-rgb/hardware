// LINE IN: the box's line input (the HAT's stereo PCM1863) played as an engine. A part set to LINE IN plays what comes
// into the rear LINE IN jack instead of a synth: gain, stereo mode and width, drive, a filter (low pass, high pass, band
// pass, notch) with a low cut, and a gate: OFF (always heard), NOTE (heard while a key / the sequencer holds a note,
// through an ADSR) or LEVEL (a noise gate). LFOs, S&H, step sequencers and a modulation matrix as the other engines,
// so the part's knobs, MOD and SEQ pages work on the input too. Written for the hardware synth (box).
// The host gives the input block in inL / inR before each process() (read at hitOff + i, as the box runs a block in
// pieces when knobs glide); none = silence.
#pragma once
#include "EngineBase.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace oct {

namespace linek {
constexpr int NS = 16;
struct Table {
    std::deque<std::string> str; std::vector<PDef> d; std::vector<int> dstp; std::string dstNames;
    const char* keep(const std::string& s) { str.push_back(s); return str.back().c_str(); }
    void add(const std::string& id, const std::string& name, char type, float mn, float mx, float df, int nch = 0, const char* ch = "") { d.push_back({ keep(id), keep(name), type, mn, mx, df, nch, ch }); }
    int find(const std::string& id) const { for (size_t i = 0; i < d.size(); ++i) if (id == d[i].id) return (int) i; return -1; }
};
inline const Table& table() {
    static Table T; if (!T.d.empty()) return T;
    static const char* SRC = "---|LFO 1|LFO 2|LFO 3|S&H|SEQ 1|SEQ 2|SEQ 3|SEQ 4|SEQ 5|SEQ 6|VELOCITY|NOTE|LFO 4|S&H 2";
    T.add("gain", "GAIN", 'f', -24, 24, 0, 0, "dB"); T.add("mode", "INPUT", 'c', 0, 4, 0, 5, "STEREO|MONO|LEFT|RIGHT|SWAP");
    T.add("width", "WIDTH", 'f', 0, 2, 1); T.add("bal", "BALANCE", 'f', -1, 1, 0); T.add("drive", "DRIVE", 'f', 0, 1, 0);
    T.add("lvl", "LEVEL", 'f', 0, 1, 1); T.add("inv", "PHASE INVERT", 'b', 0, 1, 0);
    T.add("ftype", "FILTER", 'c', 0, 4, 0, 5, "OFF|LOW PASS|HIGH PASS|BAND PASS|NOTCH"); T.add("cut", "CUTOFF", 'f', 0, 1, .7f);
    T.add("res", "RESONANCE", 'f', 0, 1, .15f); T.add("fenv", "ENV AMOUNT", 'f', -1, 1, 0); T.add("locut", "LOW CUT", 'f', 0, 1, 0);
    T.add("gmode", "GATE", 'c', 0, 2, 0, 3, "OFF|NOTE|LEVEL"); T.add("thr", "THRESHOLD", 'f', -70, 0, -50, 0, "dB");
    T.add("att", "ATTACK", 'f', 0, 1, .05f); T.add("dec", "DECAY", 'f', 0, 1, .4f); T.add("sus", "SUSTAIN", 'f', 0, 1, 1);
    T.add("rel", "RELEASE", 'f', 0, 1, .3f); T.add("vsens", "VELOCITY", 'f', 0, 1, 0);
    for (int i = 0; i < 4; ++i) { T.add("l" + std::to_string(i) + "rate", "LFO " + std::to_string(i + 1), 'f', 0, 1, .35f + .1f * i); T.add("l" + std::to_string(i) + "shape", "SHAPE", 'c', 0, 4, (float) i, 5, "SINE|TRI|SAW|SQUARE|RANDOM"); }
    T.add("shrate", "S&H RATE", 'f', 0, 1, .55f); T.add("shsm", "SMOOTH", 'f', 0, 1, 0); T.add("sh2rate", "S&H 2 RATE", 'f', 0, 1, .4f); T.add("sh2sm", "SMOOTH", 'f', 0, 1, .3f);
    std::vector<std::pair<std::string, std::string>> dst { { "", "---" } };
    for (auto x : { std::pair<const char*, const char*> { "gain", "GAIN" }, { "cut", "CUTOFF" }, { "res", "RESONANCE" }, { "width", "WIDTH" }, { "bal", "BALANCE" },
                    { "drive", "DRIVE" }, { "lvl", "VOLUME" }, { "locut", "LOW CUT" },
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
} // namespace linek

struct LineIn : EngineBase {
    static constexpr int CTV = 16, NS = linek::NS;
    const float* inL = nullptr; const float* inR = nullptr;   // this block's input (the host sets it)
    int iGain, iMode, iWidth, iBal, iDrive, iLvl, iInv, iFt, iCut, iRes, iFenv, iLocut, iGm, iThr, iAtt, iDec, iSus, iRel, iVs;
    int LR[4] {}, LS[4] {}, SQ[6][NS] {}, SD[6] {}, SA[6] {}, MS[8] {}, MD[8] {}, MA[8] {}, iBpm = 0, iMaster = 0, iShr = 0, iShs = 0, iSh2r = 0, iSh2s = 0;
    int ndst = 0, stp = 0, held = 0, heldN[128] {};
    float src[15] {}, lp[4] {}, lr[4] {};
    double shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0, ms = 0, mt = .7, lastVel = 0, lastNote = 60; long mc = 0;
    // control rate values, smoothed per sample where they move the sound
    double gT = 1, gS = 1, wT = 1, bT = 0, dT = 0, lvT = .8, mix = 1;
    double env = 0, envVel = 1; int stage = 0;   // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release (NOTE gate); LEVEL: follower
    double fol = 0, gate = 0, aI = 0, dI = 0, rI = 0;
    double s1[2] {}, s2[2] {}, hpX[2] {}, hpY[2] {}, fg = 0, fk = 0, hpA = 0;   // the filter (TPT state variable) and the low cut
    float peak[2] {};

    LineIn() {
        const auto& T = linek::table(); setTable(T.d.data(), T.dstp.data(), (int) T.d.size()); ndst = (int) T.dstp.size();
        iGain = id("gain"); iMode = id("mode"); iWidth = id("width"); iBal = id("bal"); iDrive = id("drive"); iLvl = id("lvl"); iInv = id("inv");
        iFt = id("ftype"); iCut = id("cut"); iRes = id("res"); iFenv = id("fenv"); iLocut = id("locut");
        iGm = id("gmode"); iThr = id("thr"); iAtt = id("att"); iDec = id("dec"); iSus = id("sus"); iRel = id("rel"); iVs = id("vsens");
        for (int i = 0; i < 4; ++i) { LR[i] = id("l" + std::to_string(i) + "rate"); LS[i] = id("l" + std::to_string(i) + "shape"); }
        for (int q = 0; q < 6; ++q) { for (int s = 0; s < NS; ++s) SQ[q][s] = id("s" + std::to_string(q) + "_" + std::to_string(s)); SD[q] = id("s" + std::to_string(q) + "dst"); SA[q] = id("s" + std::to_string(q) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); MS[m] = id(x + "src"); MD[m] = id(x + "dst"); MA[m] = id(x + "amt"); }
        iBpm = id("bpm"); iMaster = id("master"); iShr = id("shrate"); iShs = id("shsm"); iSh2r = id("sh2rate"); iSh2s = id("sh2sm");
        prepare(48000);
    }
    void prepare(double sr) override {
        fs = sr; rs = 271828183;
        for (auto& x : src) x = 0; const float L0[4] = { 0, .33f, .66f, .5f }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; }
        shp = sht = shv = sh2p = sh2t = sh2v = 0; clock = 0; ms = 0; mt = .7; mc = 0;
        env = 0; stage = 0; fol = 0; gate = 0; held = 0; std::fill(heldN, heldN + 128, 0);
        for (int c = 0; c < 2; ++c) { s1[c] = s2[c] = hpX[c] = hpY[c] = 0; } peak[0] = peak[1] = 0; gS = gT;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = peak[0]; m[1] = peak[1]; peak[0] = peak[1] = 0; return 2; }

    void noteOn(int n, double vel) override {
        if (n < 0 || n > 127) return;
        if (!heldN[n]) { heldN[n] = 1; ++held; }
        lastVel = vel; lastNote = n; envVel = vel; stage = 1;   // retrigger from where it is
    }
    void noteOff(int n) override {
        if (n < 0 || n > 127 || !heldN[n]) return;
        heldN[n] = 0; if (--held <= 0) { held = 0; if (stage) stage = 4; }
    }
    void allOff() override { std::fill(heldN, heldN + 128, 0); held = 0; if (stage) stage = 4; }

    static double lfo(double ph, int shape, double r) { switch (shape) { case 0: return std::sin(TP * ph); case 1: return 1 - 4 * std::fabs(ph - .5); case 2: return 2 * ph - 1; case 3: return ph < .5 ? 1 : -1; default: return r; } }
    void addModD(int si, int di, double amt) { if (si <= 0 || di <= 0 || amt == 0 || di >= ndst) return; const int pi = DSTP[di]; if (pi < 0) return; off[(size_t) pi] = (float) (off[(size_t) pi] + src[si] * amt * (D[pi].mx - D[pi].mn) * .5); }
    static double envTime(double k) { return .001 + k * k * 4; }   // 1 ms .. 4 s
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
        gT = std::pow(10.0, val(iGain) / 20) * (ri(iInv) ? -1 : 1); wT = val(iWidth); bT = val(iBal); dT = val(iDrive); lvT = val(iLvl);
        // the envelope's per-sample steps (attack linear, decay and release exponential)
        aI = 1 / (envTime(val(iAtt)) * fs); dI = 1 - std::exp(-1 / (envTime(val(iDec)) * fs * .3)); rI = 1 - std::exp(-1 / (envTime(val(iRel)) * fs * .3));
        // the filter: cutoff 20 Hz .. 20 kHz, moved by the envelope (up to 6 octaves)
        const double env1 = ri(iGm) == 0 ? 1 : env;
        double fc = 20 * std::pow(1000.0, cl(val(iCut) + val(iFenv) * env1 * .6, 0, 1)); fc = std::min(fc, fs * .45);
        fg = std::tan(PI * fc / fs); fk = 2 - 1.97 * val(iRes);
        const double lc = val(iLocut); hpA = lc <= 0 ? 0 : 1 - std::exp(-TP * (20 * std::pow(25.0, lc)) / fs);   // low cut 20 .. 500 Hz (0: off)
    }
    void process(float* L, float* R, int n) override {
        const int mode = ri(iMode), ft = ri(iFt), gm = ri(iGm);
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const int at = hitOff + i;
            double l = inL ? inL[at] : 0, r = inR ? inR[at] : (inL ? inL[at] : 0);
            peak[0] = std::max(peak[0], (float) std::fabs(l)); peak[1] = std::max(peak[1], (float) std::fabs(r));
            switch (mode) { case 1: l = r = (l + r) * .5; break; case 2: r = l; break; case 3: l = r; break; case 4: std::swap(l, r); break; default: break; }
            gS += .002 * (gT - gS); l *= gS; r *= gS;
            if (hpA > 0) { for (int c = 0; c < 2; ++c) { double& x = c ? r : l; hpY[c] += hpA * (x - hpY[c]); x -= hpY[c]; } }
            if (ft > 0) for (int c = 0; c < 2; ++c) {   // TPT state variable filter
                double& x = c ? r : l;
                const double hp = (x - (fk + fg) * s1[c] - s2[c]) / (1 + fk * fg + fg * fg);
                const double bp = fg * hp + s1[c]; s1[c] = fg * hp + bp; const double lo = fg * bp + s2[c]; s2[c] = fg * bp + lo;
                x = ft == 1 ? lo : ft == 2 ? hp : ft == 3 ? bp * fk : x - fk * bp;
            }
            if (dT > 0) { const double g = 1 + dT * 9, mk = 1 / std::tanh(g * .5) * .5; l = std::tanh(l * g) * mk; r = std::tanh(r * g) * mk; }
            // the gate
            double a = 1;
            if (gm == 1) {
                switch (stage) {
                    case 1: env += aI; if (env >= 1) { env = 1; stage = 2; } break;
                    case 2: env += dI * (val(iSus) - env); if (std::fabs(env - val(iSus)) < 1e-4) stage = 3; break;
                    case 3: env = val(iSus); break;
                    case 4: env -= rI * env; if (env < 1e-5) { env = 0; stage = 0; } break;
                    default: env = 0;
                }
                const double vs = val(iVs); a = env * (1 - vs + vs * envVel);
            } else if (gm == 2) {
                const double x = std::max(std::fabs(l), std::fabs(r)); fol = std::max(x, fol * (1 - 1 / (.01 * fs)));
                const double open = fol > std::pow(10.0, val(iThr) / 20) ? 1 : 0;
                gate += (open > gate ? aI * 4 : rI) * (open - gate); env = gate; a = gate;
            }
            // width (mid / side), balance, level
            const double m = (l + r) * .5, sd = (l - r) * .5 * wT; l = m + sd; r = m - sd;
            const double bl = bT > 0 ? 1 - bT : 1, br = bT < 0 ? 1 + bT : 1;
            ms += .004 * (mt - ms); const double out = a * lvT * ms / .7;
            L[i] = (float) (l * bl * out); R[i] = (float) (r * br * out);
        }
    }
};

} // namespace oct
