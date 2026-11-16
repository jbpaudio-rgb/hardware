// What every Octavio synth engine shares: its parameter table, the web code's random generator, value lookups,
// the step clock (internal or following the host), pitch bend and the note interface.
#pragma once
#include "../DspBlocks.h"
#include "../ParamDef.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace oct {


struct EngineBase {
    double fs = 48000;
    const PDef* D = nullptr; const int* DSTP = nullptr; int np = 0;
    std::vector<float> p, off;
    uint32_t rs = 1234567;
    double bend = 0;                 // semitones, added where the web build adds MIDI pitch bend
    bool hostClock = false; double hostPpq = 0; int curI = 0;
    // LFO SYNC (the box): LFO i follows the beat (hostPpq, hostBpm, set by the host each block even without hostClock),
    // its rate knob then picks a note value. lfoAdv() gives the LFO's next phase; past 1 = it wrapped (as the free one)
    bool lfoSync[4] {}; double hostBpm = 120;
    // VINTAGE (the box's ENGINE page, 0-1): how far each voice strays from the others, and vOsc: each oscillator's own
    // slow drift, in semitones (set by the box every block; 0 in the plugins)
    double vintage = 0, vOsc[4] {};
    static constexpr int NSYNCDIV = 11;
    static constexpr double SYNC_MULT[NSYNCDIV] = { 1 / 16., 1 / 8., 1 / 4., 1 / 2., 1, 1.5, 2, 3, 4, 6, 8 };   // cycles per beat
    static constexpr const char* SYNC_NAMES[NSYNCDIV] = { "4 MES", "2 MES", "1 MES", "1/2", "1/4", "1/4 T", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" };
    static int syncDiv(double k) { return std::clamp((int) std::lround(k * (NSYNCDIV - 1)), 0, NSYNCDIV - 1); }
    double lfoAdv(int i, double ph, double k, double dt) const {
        if (i < 0 || i > 3 || !lfoSync[i]) return ph + lfoHz(k) * dt;
        const double t = (hostPpq + curI / fs * hostBpm / 60) * SYNC_MULT[syncDiv(k)]; double f = t - std::floor(t);
        return f + 0.5 < ph ? f + 1 : f;   // it came round: past 1, so the engine's wrap (and its sample & hold) still happen
    }   // host transport: the beat position at the start of the block, the sample in the block
    // where the drum tracks / pads 1-8 were hit in the last block (sample, -1 none): the box's SIDECHAIN keys on them.
    // The box clears it before each block and sets hitOff when it runs a block in pieces.
    int hitAt[8] { -1, -1, -1, -1, -1, -1, -1, -1 }; int hitOff = 0;
    void markHit(int k) { if (k >= 0 && k < 8 && hitAt[k] < 0) hitAt[k] = hitOff + curI; }
    virtual ~EngineBase() = default;

    void setTable(const PDef* d, const int* dstp, int n) {
        D = d; DSTP = dstp; np = n; p.resize((size_t) n); off.assign((size_t) n, 0.f); ext.assign((size_t) n, 0.f);
        for (int i = 0; i < n; ++i) p[(size_t) i] = d[i].df;
    }
    int id(const std::string& name) const {   // index of a control by its web id (used when an engine sets up)
        for (int i = 0; i < np; ++i) if (name == D[i].id) return i;
        return -1;
    }
    double rnd() { rs = rs * 1664525u + 1013904223u; return rs / 4294967296.0; }
    double g(int i) const { return p[(size_t) i]; }
    double val(int i) const { return cl((double) p[(size_t) i] + off[(size_t) i] + ext[(size_t) i], D[i].mn, D[i].mx); }
    std::vector<float> ext;   // modulation from outside the engine (the hardware box's LFOs and sequencers 5-8), 0 otherwise
    int ri(int i) const { return iround(p[(size_t) i]); }
    // the step clock: 4 steps per beat
    double advance(double clock, double bpm, double dt) const { return hostClock ? (hostPpq + curI / fs * bpm / 60) * 4 : clock + bpm / 60 * 4 * dt; }
    void addMod(const float* s, int si, int di, double amt) {
        if (si <= 0 || di <= 0 || amt == 0) return; const int pi = DSTP[di];
        off[(size_t) pi] = (float) (off[(size_t) pi] + s[si] * amt * (D[pi].mx - D[pi].mn) * .5);
    }

    virtual void prepare(double sampleRate) = 0;           // rebuilds the sound state (keeps the controls)
    virtual void process(float* L, float* R, int n) = 0;
    virtual void noteOn(int n, double vel) = 0;
    virtual void noteOff(int n) = 0;
    virtual void allOff() = 0;
    virtual void pad(int) {}   // a drum pad pressed in the panel (DR8)
    virtual void padOn(int k, double) { pad(k); }   // a pad held in the panel (SP64: with its velocity, and released)
    virtual void padOff(int) {}
    virtual int step() const = 0;
    virtual int meters(float* m) = 0;   // what the panel's meters show (up to 16 values; the layout is the engine's own)
};

} // namespace oct
