// The box's engines: the plugins' 11 (core/synth, synthmap) and the box's own (DFAM, DX7), in one table with the same shape
// as synthmap::ENG (id, name, the controls on the CC list and their names).
#pragma once
#include "../core/synth/Engines.h"
#include "../core/synth/SynthCtlMap.h"
#include "../core/synth/Dfam.h"
#include "../core/synth/Dx7.h"
#include "../core/synth/LineIn.h"
#include <cstring>
#include <deque>

namespace box {

namespace dfamcc {   // the DFAM's controls on the CC list: per track VCO 1, VCO 2, cutoff, VCF decay, VCA decay, level; then swing, accent
inline const char* const* ids() {
    static std::vector<std::string> s; static std::vector<const char*> v; if (!v.empty()) return v.data();
    for (int t = 0; t < 8; ++t) for (const char* k : { "f1", "f2", "cut", "fdec", "adec", "lvl" }) s.push_back("t" + std::to_string(t) + k);
    s.push_back("swing"); s.push_back("accent"); for (auto& x : s) v.push_back(x.c_str()); return v.data();
}
inline const char* const* names() {
    static std::vector<std::string> s; static std::vector<const char*> v; if (!v.empty()) return v.data();
    for (int t = 0; t < 8; ++t) for (const char* k : { "VCO 1", "VCO 2", "CUTOFF", "VCF DECAY", "VCA DECAY", "LEVEL" }) s.push_back("T" + std::to_string(t + 1) + " " + k);
    s.push_back("SWING"); s.push_back("ACCENT"); for (auto& x : s) v.push_back(x.c_str()); return v.data();
}
}

namespace dx7cc {   // the DX7's controls on the CC list: algorithm, feedback, brightness, the 6 output levels, LFO, level
inline const char* const* ids() {
    static std::vector<std::string> s; static std::vector<const char*> v; if (!v.empty()) return v.data();
    for (const char* k : { "alg", "fb", "bright", "o1lvl", "o2lvl", "o3lvl", "o4lvl", "o5lvl", "o6lvl", "lspd", "lpmd", "lamd", "porta", "lvl" }) s.push_back(k);
    for (auto& x : s) v.push_back(x.c_str()); return v.data();
}
inline const char* const* names() {
    static const char* N[] = { "ALGORITHM", "FEEDBACK", "BRIGHTNESS", "OP 1 LEVEL", "OP 2 LEVEL", "OP 3 LEVEL", "OP 4 LEVEL", "OP 5 LEVEL", "OP 6 LEVEL", "LFO SPEED", "PITCH MOD", "AMP MOD", "PORTAMENTO", "LEVEL" };
    return N;
}
}

namespace linecc {   // LINE IN's controls on the CC list
inline const char* const* ids() { static const char* I[] = { "gain", "width", "bal", "drive", "cut", "res", "fenv", "locut", "thr", "lvl" }; return I; }
inline const char* const* names() { static const char* N[] = { "GAIN", "WIDTH", "BALANCE", "DRIVE", "CUTOFF", "RESONANCE", "ENV AMOUNT", "LOW CUT", "THRESHOLD", "LEVEL" }; return N; }
}

// the DR8 on the box: the plugin's DR8 with an envelope more on each track (attack, hold, release, velocity), shaping
// the whole hit; the hold at its top = the hit as the DR8 plays it
struct BoxDr8 : oct::Dr8 {
    int ATT[8] {}, HLD[8] {}, REL[8] {}, VS[8] {}; double env[8] {}, et[8] {};
    static const std::vector<::PDef>& table() {
        static std::vector<::PDef> T; if (!T.empty()) return T;
        T.assign(dr8::PDEFS, dr8::PDEFS + dr8::NP);
        static std::deque<std::string> ids;
        for (int t = 0; t < 8; ++t) {
            auto add = [&](const char* k, const char* name, float df) { ids.push_back("t" + std::to_string(t) + k); T.push_back({ ids.back().c_str(), name, 'f', 0.f, 1.f, df, 0, "" }); };
            add("att", "ATTACK", 0); add("hold", "HOLD", 1); add("rel", "RELEASE", .3f); add("vsens", "VELOCITY", 1);
        }
        return T;
    }
    BoxDr8() {
        const auto& T = table(); setTable(T.data(), dr8::DSTP, (int) T.size());
        for (int t = 0; t < 8; ++t) { const std::string x = "t" + std::to_string(t); ATT[t] = id(x + "att"); HLD[t] = id(x + "hold"); REL[t] = id(x + "rel"); VS[t] = id(x + "vsens"); }
    }
    double shape(int tr) {   // the track's envelope, from the hit (the voice's t)
        const double t = v[tr].t, a = .0005 + g(ATT[tr]) * g(ATT[tr]) * .2, h = g(HLD[tr]) >= .999 ? 1e9 : .005 + g(HLD[tr]) * g(HLD[tr]) * 1.5, r = .005 + g(REL[tr]) * g(REL[tr]) * 2;
        double e = t < a ? t / a : 1; if (t > a + h) e *= std::exp(-(t - a - h) / r * 3);
        const double vs = g(VS[tr]); return e * (1 - vs + vs * std::min(1.0, v[tr].vel / .78));
    }
    void process(float* L, float* R, int n) override {
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            const bool play = p[dr8::play] > .5;
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
            for (int tr = 0; tr < 8; ++tr) {
                lvS[tr] = (float) (lvS[tr] + .002 * (lvT[tr] - lvS[tr])); pnS[tr] = (float) (pnS[tr] + .002 * (pnT[tr] - pnS[tr])); auto& vv = v[tr]; if (!vv.on) continue;
                const double sh = shape(tr); const double y = vv.sample() * lvS[tr] * sh; const double pa = (pnS[tr] + 1) * .25 * oct::PI; sl += y * std::cos(pa); sr += y * std::sin(pa);
            }
            ms += .004 * (mt - ms); L[i] = (float) std::tanh(sl * ms * 1.2); R[i] = (float) std::tanh(sr * ms * 1.2);
        }
    }
};

constexpr int NENG = synthmap::NENG + 3;
inline const synthmap::Eng* engs() {
    static synthmap::Eng E[NENG]; static bool made = false;
    if (!made) { for (int e = 0; e < synthmap::NENG; ++e) E[e] = synthmap::ENG[e]; E[synthmap::NENG] = { "dfam", "DFAM", 50, dfamcc::ids(), dfamcc::names() };
                 E[synthmap::NENG + 1] = { "dx7", "DX7", 14, dx7cc::ids(), dx7cc::names() };
                 E[synthmap::NENG + 2] = { "line", "LINE IN", 10, linecc::ids(), linecc::names() }; made = true; }
    return E;
}
#define ENGS (::box::engs())
inline oct::EngineBase* makeBoxEngine(const char* id) { if (!std::strcmp(id, "dfam")) return new oct::Dfam(); if (!std::strcmp(id, "dx7")) return new oct::Dx7(); if (!std::strcmp(id, "line")) return new oct::LineIn(); if (!std::strcmp(id, "dr8")) return new BoxDr8(); return oct::makeEngine(id); }

} // namespace box
