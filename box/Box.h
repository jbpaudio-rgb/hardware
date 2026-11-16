// Octavio Box: the groovebox engine of the hardware synth.
// A 4-part multitimbral synth: each part has one of the 11 Octavio engines (core/synth), its own FX rack
// (core/FxEngine.h) and its own MIDI channel; a mixer, MIDI in (notes, CCs, pitch bend, sustain, MIDI clock)
// and a tempo shared by every part. (In the code a part is a "track".)
// No audio or MIDI device code here: main.cpp feeds it. Nothing in process() allocates or locks.
#pragma once
#if defined(__x86_64__) || defined(__SSE2__)
#include <xmmintrin.h>
#endif
#include "PartPool.h"
#include "BoxEngines.h"
#include "../core/FxEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace box {

// a queue with one writer thread and one reader thread, no locks
template <typename T, unsigned N> struct Ring {
    std::array<T, N> b {};
    std::atomic<unsigned> w { 0 }, r { 0 };
    bool push(const T& x) {
        const unsigned wi = w.load(std::memory_order_relaxed);
        if (wi - r.load(std::memory_order_acquire) >= N) return false;
        b[wi % N] = x; w.store(wi + 1, std::memory_order_release); return true;
    }
    bool pop(T& x) {
        const unsigned ri = r.load(std::memory_order_relaxed);
        if (ri == w.load(std::memory_order_acquire)) return false;
        x = b[ri % N]; r.store(ri + 1, std::memory_order_release); return true;
    }
};

enum Kind : uint8_t { MIDI, PARAM, FXPARAM, ENGINE, VOL, PAN, MUTE, NOTE, BPM, PLAY, PANIC, FXMOVE, LEARN, CCMAP, PADHIT, ARP, NOTESEQ, BOXMOD, TUNE, VINTAGE, LFOSYNC };
// LFOSYNC: LFO i (0-3 the engine's, 4-7 the box's) follows the tempo (v 1) or runs free (v 0)
// TUNE: the part's tuning, v semitones (-12..12). VINTAGE: its analog drift, v 0-1
// BOXMOD: the box's own LFOs 5-8 and sequencers 5-8, setting i (Track::bm) = v
// ARP: arpeggiator setting i (Box::ARP_KEYS) = v. NOTESEQ: note sequencer setting i (Box::NS_KEYS), step ms for the per-step ones
// PADHIT: SAMPLER pad i hit with velocity v (v 0: let go); ms > 0: let go after ms
// FXMOVE: effect slot i moves to place (int) v. LEARN: control i of the part waits for a CC (v 0), forgets its CC (v 1), or
// stop waiting (v 2). CCMAP: engine m[0], CC ms (0-127) drives control i (i -1: the CC goes back to the default map)
struct Msg {
    Kind k = MIDI; int8_t t = 0;       // track 0-3
    uint8_t m[3] {};                    // MIDI bytes
    int i = 0; float v = 0; int ms = 0; // index / value / note length
};

constexpr int NT = 4, MAXBLOCK = 1024;
constexpr double BEND_RANGE = 2;        // semitones, as in the plugin

inline int engineIndex(const std::string& s) {   // by id ("mini") or by name ("SAMPLER"), any case
    for (int e = 0; e < NENG; ++e) {
        std::string a = ENGS[e].id, b = ENGS[e].name, x = s;
        for (auto* q : { &a, &b, &x }) for (auto& c : *q) c = (char) std::tolower((unsigned char) c);
        if (x == a || x == b) return e;
    }
    return -1;
}

struct Glide { int i; float to; };   // a knob move, reached over ~10 ms instead of in one step (no zipper)
struct Track {
    Glide gl[16]; int ngl = 0;
    std::array<std::unique_ptr<oct::EngineBase>, NENG> eng;
    std::array<std::vector<int>, NENG> ctl;                         // k-th CC of the app's CC list -> parameter index
    std::array<std::unique_ptr<std::atomic<float>[]>, NENG> shown;  // the parameters as the UI reads them
    std::unique_ptr<oct::FxRack> fx;
    std::unique_ptr<std::atomic<float>[]> fxShown;
    std::atomic<int> e { 1 };
    std::atomic<float> vol { .8f }, pan { 0 }, meter { 0 };
    std::atomic<bool> mute { false };
    // ENGINE page: TUNE (semitones) and VINTAGE (0: clean, 1: an old, warm, drifting machine)
    std::atomic<float> tuneSt { 0 }, vint { 0 };
    std::array<std::atomic<int>, 8> lfoSync {};   // MOD > LFO n > SYNC: the LFO on the tempo, its rate a note value
    std::atomic<int> engStep { 0 };               // where the engine's sequencers are (for the screen, which may be another copy)
    double pb = 0;                                  // the MIDI pitch bend, semitones (TUNE and the drift are added to it)
    std::array<std::vector<std::pair<int, bool>>, NENG> vIdx;   // vintage targets per engine: (param, true: drift / false: filter)
    double drP = 0, drPv = 0, drC = 0, drCv = 0, drO[3] {}, drOv[3] {};   // drO: each oscillator's own wander    // vintage: slow random walks for the pitch and the filter
    double dcXL = 0, dcYL = 0, dcXR = 0, dcYR = 0;  // the output's DC blocker
    std::array<std::array<std::atomic<int>, 128>, NENG> learnt;
    // the arpeggiator (ENGINE > ARPÉGIATEUR): on, mode, octaves, rate, gate, latch; and what it is playing (audio thread)
    std::array<std::atomic<int>, 6> arp {};
    bool noteHit = false;   // a note started on this part in this block (the SIDECHAIN effect keys on it)
    int aHeld[32] {}, aN = 0, aPhys = 0, aIdx = -1, aDir = 1, aNote = -1, aVel = 100; bool aLatch = false; int64_t aNext = 0, aOff = 0;
    // the note sequencer (ENGINE > SÉQUENCE): 16 steps of note and gate (0 rest, 1 note, 2 tie), length, rate, gate %,
    // on, transpose; REC writes the notes played into it
    std::array<std::atomic<int>, 16> nsNote {}, nsGate {};
    std::array<std::atomic<int>, 6> ns {};   // NS_KEYS: on, len, rate, gatepct, transpose, rec
    std::atomic<int> nsStep { -1 }, nsRecPos { 0 };
    // the box's own LFOs 5-8 and sequencers 5-8 (the engines have 4 of each): LFO n (0-3) at 4n: rate, shape, destination,
    // amount; sequencer q (0-3) at 16 + 18q: destination, amount, its 16 steps. Destinations as the engine's matrix.
    static constexpr int BM_LFO = 0, BM_SEQ = 16, BM_N = 16 + 4 * 18;
    std::array<std::atomic<float>, BM_N> bm {};
    double bmPh[4] {}, bmRnd[4] {}; std::vector<int> bmTouched;
    int nsCur = -1; int64_t nsNext = 0, nsOff = 0; bool nsWas = false;   // MIDI learn: CC -> control, per engine (-1: the default map)
    bool sustain = false; std::array<bool, 128> held {};
    float L[MAXBLOCK] {}, R[MAXBLOCK] {}, fL[MAXBLOCK] {}, fR[MAXBLOCK] {};
    float gL = 0, gR = 0;   // the mixer gains, smoothed
};

struct Box {
    double fs;
    std::array<Track, NT> tr;
    Ring<Msg, 1024> fromMidi, fromUi;          // one queue per writer thread
    std::atomic<int> selected { 0 };            // the part the panel edits; it also plays the MIDI channels no part listens to
    std::array<std::atomic<int>, NT> chan {};   // each part's MIDI channel 0-15 (parts on the same channel play together: layers)
    std::atomic<float> bpm { 120 };
    std::atomic<bool> playing { false };
    std::atomic<float> masterMeter { 0 };
    std::atomic<float> master { .9f };     // master volume (the panel's volume knob)
    struct Off { int t = -1, n = 0; int64_t due = 0; bool pad = false; };
    std::array<Off, 64> offs {};
    int64_t now = 0;
    int slotParam[8] {};   // where slot0-slot7 sit in the FX parameters
    std::atomic<int> learnT { -1 }, learnI { -1 }, learnedCc { -1 };
    // SAMPLER samples: handed to the audio thread through smpIn, the replaced ones come back through smpOut (freed by drain)
    struct SmpSwap { int t = 0, slot = 0; std::shared_ptr<const oct::SmpData>* p = nullptr; bool user = true; };
    Ring<SmpSwap, 256> smpIn, smpOut;
    int spIdx = -1;                                    // the SAMPLER's engine number
    int lineIdx = -1;                                  // LINE IN's engine number
    const float* inL = nullptr; const float* inR = nullptr;   // the line input's block (process() sets it; none: silence)
    std::array<std::atomic<uint64_t>, NT> padHits {};  // SAMPLER pads hit since the screen last looked (64 bits)
    void drainSamples() { SmpSwap x; while (smpOut.pop(x)) delete x.p; }
    // a sample for SAMPLER slot s of part t (d null + user: empty; user false: the built-in kit sound)
    bool sendSample(int t, int slot, std::shared_ptr<const oct::SmpData> d, bool user) {
        drainSamples(); if (spIdx < 0 || slot < 0 || slot >= oct::Sp64::NPAD) return false;
        if (!user && slot < 16 && !d) { auto k = std::make_shared<oct::SmpData>(); k->L = oct::Sp64::kitGen(slot, fs); k->sr = fs; d = k; }
        SmpSwap x; x.t = t; x.slot = slot; x.p = new std::shared_ptr<const oct::SmpData>(std::move(d)); x.user = user;
        if (!smpIn.push(x)) { delete x.p; return false; }
        return true;
    }   // MIDI learn: the part and control waiting for a CC; the CC it got
    // the master output for the screen's oscilloscope: the last 4096 samples (mono), written by the audio thread
    static constexpr unsigned SCOPE = 4096;
    std::array<std::atomic<float>, SCOPE> scope {};
    std::atomic<unsigned> scopeW { 0 };
    std::array<std::atomic<float>, SCOPE> tune {};   // the selected part alone (after its FX, before its fader): the tuner
    // tempo and sync (the TEMPO & SYNCHRO page): where the tempo comes from, what is received and sent
    struct Sync {
        std::atomic<int> clock { 0 };          // 0 internal, 1 MIDI (DIN), 2 USB (USB MIDI devices and the virtual port), 3 Bluetooth (BLE MIDI)
        std::atomic<bool> rxStart { true };     // follow start / stop (from the clock's source; from both when internal)
        std::atomic<int> txClock { 0 };        // send MIDI clock, bits: 1 MIDI (DIN out), 2 USB, 4 Bluetooth
        std::atomic<bool> txStart { true };     // send start / stop with the clock
        std::atomic<bool> thru { true };        // DIN MIDI in echoed to DIN MIDI out
        std::atomic<bool> multiTempo { true };  // a multi loads its tempo
        std::atomic<double> lastClock { -100 }; // when the last clock tick came from the chosen source (seconds)
    } sync;
    static constexpr const char* ARP_KEYS[6] = { "on", "mode", "oct", "rate", "gate", "latch" };
    static constexpr const char* NS_KEYS[6] = { "on", "len", "rate", "gate", "transpose", "rec" };
    static constexpr int RATE_DIV[6] = { 1, 2, 4, 8, 3, 6 };   // steps per beat: 1/4 1/8 1/16 1/32 1/8T 1/16T
    int64_t stepLen(int rate) const { return (int64_t) (fs * 60.0 / std::max(20.f, bpm.load()) / RATE_DIV[std::clamp(rate, 0, 5)]); }
    // ---- the arpeggiator ----
    void arpStop(int t) { auto& T = tr[(size_t) t]; if (T.aNote >= 0) engine(t).noteOff(T.aNote); T.aNote = -1; T.aIdx = -1; }
    void arpAdd(int t, int n, int vel) {
        auto& T = tr[(size_t) t];
        if (T.arp[5].load() && T.aLatch) { T.aLatch = false; T.aN = 0; }   // latch: a new chord after letting go replaces the old one
        ++T.aPhys; for (int i = 0; i < T.aN; ++i) if (T.aHeld[i] == n) return;
        if (T.aN < 32) T.aHeld[T.aN++] = n; T.aVel = vel; if (T.aIdx < 0) T.aNext = now;
    }
    void arpRemove(int t, int n) {
        auto& T = tr[(size_t) t];
        T.aPhys = std::max(0, T.aPhys - 1);
        if (T.arp[5].load()) { if (!T.aPhys) T.aLatch = true; return; }   // latched: the chord stays until a new one
        for (int i = 0; i < T.aN; ++i) if (T.aHeld[i] == n) { for (int j = i; j + 1 < T.aN; ++j) T.aHeld[j] = T.aHeld[j + 1]; --T.aN; break; }
        if (!T.aN) arpStop(t);
    }
    void arpRun(int t) {   // at the start of a block: the next note when it is due
        auto& T = tr[(size_t) t]; if (!T.arp[0].load()) return;
        if (T.aNote >= 0 && now >= T.aOff) { engine(t).noteOff(T.aNote); T.aNote = -1; }
        if (!T.aN || now < T.aNext) return;
        int sorted[32]; const int n = T.aN; for (int i = 0; i < n; ++i) sorted[i] = T.aHeld[i];
        const int mode = T.arp[1].load(); if (mode != 4) std::sort(sorted, sorted + n);
        const int oct = std::clamp(T.arp[2].load(), 1, 4), total = n * oct;
        int& i = T.aIdx;
        if (mode == 0 || mode == 4) i = (i + 1) % total;
        else if (mode == 1) i = i < 0 ? total - 1 : (i - 1 + total) % total;
        else if (mode == 2) { if (total == 1) i = 0; else { i += T.aDir; if (i >= total) { i = total - 2; T.aDir = -1; } if (i < 0) { i = std::min(1, total - 1); T.aDir = 1; } } }
        else { rs = rs * 1664525u + 1013904223u; i = (int) (rs >> 8) % total; }
        const int note = std::clamp(sorted[i % n] + 12 * (i / n), 0, 127); const int64_t len = stepLen(T.arp[3].load());
        if (T.aNote >= 0) engine(t).noteOff(T.aNote);
        engine(t).noteOn(note, .15 + .85 * T.aVel / 127.0); T.aNote = note;
        T.aOff = now + std::max<int64_t>(64, len * std::clamp(T.arp[4].load(), 5, 100) / 100); T.aNext = (T.aNext > now - len ? T.aNext : now) + len;
    }
    uint32_t rs = 12345;
    double rnd() { rs = rs * 1664525u + 1013904223u; return rs / 2147483648.0 - 1; }
    // VINTAGE, once a block: the pitch wanders a few cents, the filter breathes, the engine's own drift / vintage
    // control is pushed up. At 0 nothing moves (the part's TUNE and the pitch bend still apply).
    static constexpr const char* VINT_CUT[] = { "cutoff", "freq", "ff", "lpf", "alpf", "blpf", "gcut" };
    void vintage(int t, int n) {
        auto& T = tr[(size_t) t]; auto& E = engine(t); const double v = T.vint.load(std::memory_order_relaxed);
        const double dt = n / fs;
        // two random walks: the pitch's ~4 s, the filter's ~2 s (velocity noise, leaky: a smooth wander that stays near 0)
        T.drPv += (rnd() * 3 - T.drPv * .6 - T.drP * 1.2) * dt; T.drP += T.drPv * dt; T.drP = std::clamp(T.drP, -1.5, 1.5);
        T.drCv += (rnd() * 5 - T.drCv * 1.2 - T.drC * 2.5) * dt; T.drC += T.drCv * dt; T.drC = std::clamp(T.drC, -1.5, 1.5);
        E.bend = T.pb + T.tuneSt.load(std::memory_order_relaxed) + v * .09 * T.drP;   // up to ~9 cents
        // each oscillator wanders on its own (~3 s), so they beat against each other a little; and each voice of the poly
        // engines strays by its own tolerances (EngineBase::vintage)
        for (int k = 0; k < 3; ++k) { T.drOv[k] += (rnd() * 3 - T.drOv[k] * .5 - T.drO[k] * 1.0) * dt; T.drO[k] = std::clamp(T.drO[k] + T.drOv[k] * dt, -1.5, 1.5); E.vOsc[k] = v * .06 * T.drO[k]; }
        E.vintage = v;
        if (v <= 0 || (int) E.ext.size() != E.np) return;
        for (const auto& [i, drift] : T.vIdx[(size_t) T.e.load(std::memory_order_relaxed)]) {
            const float sp = E.D[i].mx - E.D[i].mn; const float x = (float) (drift ? v * .7 * sp : v * .018 * sp * T.drC);
            E.ext[(size_t) i] += x; T.bmTouched.push_back(i);
        }
    }
    // 1: the engine's own drift / vintage control; 2: a filter cutoff; 0: neither
    static int vintageRole(const oct::EngineBase& E, int i) {
        const char* id = E.D[i].id;
        if (!std::strcmp(id, "drift") || !std::strcmp(id, "vintage")) return 1;
        if (E.D[i].type != 'f') return 0;
        for (const char* c : VINT_CUT) if (!std::strcmp(id, c)) return 2;
        return std::strlen(id) == 5 && id[0] == 't' && !std::strcmp(id + 2, "cut") ? 2 : 0;   // DFAM t0cut..t7cut
    }
    // the box's LFOs and sequencers 5-8: once a block, into the engine's ext offsets
    void boxMod(int t, int n) {
        auto& T = tr[(size_t) t]; auto& E = engine(t); if ((int) E.ext.size() != E.np) return;
        for (int i : T.bmTouched) if (i >= 0 && i < E.np) E.ext[(size_t) i] = 0;
        T.bmTouched.clear();
        const int dm = E.id("m0dst"); if (dm < 0 || !E.DSTP) return; const int nd = (int) E.D[dm].mx;
        auto add = [&](int di, double v, double amt) {
            if (di <= 0 || di > nd || amt == 0) return; const int pi = E.DSTP[di]; if (pi < 0) return;
            E.ext[(size_t) pi] += (float) (v * amt * (E.D[pi].mx - E.D[pi].mn) * .5); T.bmTouched.push_back(pi);
        };
        for (int k = 0; k < 4; ++k) {
            const float* b = nullptr; (void) b;
            const double rate = T.bm[(size_t) (4 * k)].load(); const int shape = (int) T.bm[(size_t) (4 * k + 1)].load();
            double& ph = T.bmPh[k];
            if (T.lfoSync[(size_t) (4 + k)].load(std::memory_order_relaxed)) {   // on the tempo: its phase from the beat
                const double tt = beat * oct::EngineBase::SYNC_MULT[oct::EngineBase::syncDiv(rate)]; double f = tt - std::floor(tt);
                ph = f + 0.5 < ph ? f + 1 : f;
            } else ph += oct::lfoHz(rate) * n / fs;
            if (ph >= 1) { ph -= std::floor(ph); rs = rs * 1664525u + 1013904223u; T.bmRnd[k] = rs / 2147483648.0 - 1; }
            const double v = shape == 0 ? std::sin(oct::TP * ph) : shape == 1 ? 1 - 4 * std::fabs(ph - .5) : shape == 2 ? 2 * ph - 1 : shape == 3 ? (ph < .5 ? 1 : -1) : T.bmRnd[k];
            add((int) T.bm[(size_t) (4 * k + 2)].load(), v, T.bm[(size_t) (4 * k + 3)].load());
        }
        const int st = ((E.step() % 16) + 16) % 16;
        for (int q = 0; q < 4; ++q) { const int o = 16 + 18 * q; add((int) T.bm[(size_t) o].load(), T.bm[(size_t) (o + 2 + st)].load(), T.bm[(size_t) (o + 1)].load()); }
    }
    bool recordHere = true;   // false in the browser's screen copy: the sound's copy records and sends the steps (recOut)
    Ring<Msg, 256> recOut;
    // ---- the note sequencer ----
    void nsStop(int t) { auto& T = tr[(size_t) t]; if (T.nsCur >= 0) engine(t).noteOff(T.nsCur); T.nsCur = -1; T.nsStep = -1; }
    void nsRun(int t) {
        auto& T = tr[(size_t) t]; const bool on = T.ns[0].load() && playing.load();
        if (!on) { if (T.nsWas) nsStop(t); T.nsWas = false; return; }
        if (!T.nsWas) { T.nsWas = true; T.nsNext = now; T.nsStep = -1; }
        if (T.nsCur >= 0 && now >= T.nsOff) { engine(t).noteOff(T.nsCur); T.nsCur = -1; }
        if (now < T.nsNext) return;
        const int len = std::clamp(T.ns[1].load(), 1, 16); const int k = (T.nsStep.load() + 1) % len; T.nsStep = k;
        const int64_t sl = stepLen(T.ns[2].load()); T.nsNext += sl; if (T.nsNext < now) T.nsNext = now + sl;
        const int g = T.nsGate[(size_t) k].load(), note = quantize(t, std::clamp(T.nsNote[(size_t) k].load() + T.ns[4].load(), 0, 127));
        if (g == 2 && T.nsCur >= 0) { T.nsOff = T.nsNext + sl; return; }   // tie: the note goes on
        if (T.nsCur >= 0) { engine(t).noteOff(T.nsCur); T.nsCur = -1; }
        if (g == 0) return;
        engine(t).noteOn(note, .8); T.nsCur = note; T.nsOff = now + std::max<int64_t>(64, sl * std::clamp(T.ns[3].load(), 5, 100) / 100);
        if (k + 1 < len && T.nsGate[(size_t) k + 1].load() == 2) T.nsOff = T.nsNext + sl;
    }
    void nsRecord(int t, int n) {   // REC: playing, on the step under the playhead (or the next one past its half); stopped, step by step
        auto& T = tr[(size_t) t]; const int len = std::clamp(T.ns[1].load(), 1, 16); int k;
        if (playing.load() && T.ns[0].load() && T.nsStep.load() >= 0) { k = T.nsStep.load(); if (T.nsNext - now < stepLen(T.ns[2].load()) / 2) k = (k + 1) % len; }
        else { k = T.nsRecPos.load() % len; T.nsRecPos = (k + 1) % len; }
        T.nsNote[(size_t) k] = std::clamp(n - T.ns[4].load(), 0, 127); T.nsGate[(size_t) k] = 1;
        Msg r; r.k = NOTESEQ; r.t = (int8_t) t; r.ms = k; r.i = 100; r.v = (float) T.nsNote[(size_t) k].load(); recOut.push(r); r.i = 101; r.v = 1; recOut.push(r);
    }
    static constexpr int NSYNC = 10;   // the tempo & sync settings, then the quantizer's (saved with them)
    static constexpr const char* SYNC_KEYS[NSYNC] = { "clock", "rxstart", "txclock", "txstart", "thru", "multitempo", "qroot", "qscale", "qmode", "qparts" };
    // ---------------- the quantizer (TEMPO & SYNCHRO > QUANTIZER): the note sequencers' notes pulled into a key ----------------
    static constexpr int NSCALE = 25;   // new scales go at the end: the multis keep the index
    static constexpr const char* SCALE_NAMES[NSCALE] = { "OFF", "MAJOR · IONIAN", "MINOR · AEOLIAN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "LOCRIAN",
        "HARM. MINOR", "MEL. MINOR", "MAJ. PENTA", "MIN. PENTA", "BLUES", "WHOLE TONE", "DIMINISHED",
        "PHRYGIAN DOM.", "LYDIAN DOM.", "ALTERED", "HUNGARIAN MIN.", "DOUBLE HARM.", "HIRAJOSHI", "IN SEN", "BEBOP DOM.",
        "MAJ. BLUES", "CHROMATIC" };
    static constexpr uint16_t SCALE_MASK[NSCALE] = {   // bit i: the degree i semitones above the key is in the scale
        0xFFF, 0xAB5, 0x5AD, 0x6AD, 0x5AB, 0xAD5, 0x6B5, 0x56B, 0x9AD, 0xAAD, 0x295, 0x4A9, 0x4E9, 0x555, 0x6DB,
        0x5B3, 0x6D5, 0x55B, 0x9CD, 0x9B3, 0x18D, 0x4A3, 0xEB5, 0x29D, 0xFFF };
    std::atomic<int> qRoot { 0 }, qScale { 0 }, qMode { 0 }, qParts { 15 };   // key (0 = C), scale (0 = off), 0 nearest / 1 down / 2 up, parts (bits)
    int quantize(int t, int n) const {
        const int sc = qScale.load(std::memory_order_relaxed); if (sc <= 0 || !((qParts.load(std::memory_order_relaxed) >> t) & 1)) return n;
        const int root = qRoot.load(std::memory_order_relaxed), mode = qMode.load(std::memory_order_relaxed); const uint16_t mk = SCALE_MASK[sc];
        auto in = [&](int x) { return (mk >> (((x - root) % 12 + 12) % 12)) & 1; };
        if (in(n)) return n;
        for (int d = 1; d < 12; ++d) {
            if (mode != 2 && in(n - d)) return std::clamp(n - d, 0, 127);   // nearest: a tie goes down
            if (mode != 1 && in(n + d)) return std::clamp(n + d, 0, 127);
        }
        return n;
    }
    bool setSync(const std::string& key, int v) {
        if (key == "clock") sync.clock.store(std::clamp(v, 0, 3));
        else if (key == "rxstart") sync.rxStart.store(v != 0);
        else if (key == "txclock") sync.txClock.store(std::clamp(v, 0, 7));
        else if (key == "txstart") sync.txStart.store(v != 0);
        else if (key == "thru") sync.thru.store(v != 0);
        else if (key == "multitempo") sync.multiTempo.store(v != 0);
        else if (key == "qroot") qRoot.store(((v % 12) + 12) % 12);
        else if (key == "qscale") qScale.store(std::clamp(v, 0, NSCALE - 1));
        else if (key == "qmode") qMode.store(std::clamp(v, 0, 2));
        else if (key == "qparts") qParts.store(std::clamp(v, 0, 15));
        else return false;
        return true;
    }
    int getSync(const std::string& key) const {
        if (key == "clock") return sync.clock.load(); if (key == "rxstart") return sync.rxStart.load(); if (key == "txclock") return sync.txClock.load();
        if (key == "txstart") return sync.txStart.load(); if (key == "thru") return sync.thru.load(); if (key == "multitempo") return sync.multiTempo.load();
        if (key == "qroot") return qRoot.load(); if (key == "qscale") return qScale.load(); if (key == "qmode") return qMode.load(); if (key == "qparts") return qParts.load();
        return 0;
    }
    std::string syncLines() const { std::string s; for (const char* k : SYNC_KEYS) s += std::string("sync ") + k + " " + std::to_string(getSync(k)) + "\n"; return s; }

    explicit Box(double sampleRate, const int startEngines[NT] = nullptr) : fs(sampleRate) {
        static const int def[NT] = { engineIndex("mini"), engineIndex("pro70"), engineIndex("vact"), engineIndex("dr8") };
        for (int t = 0; t < NT; ++t) {
            auto& T = tr[(size_t) t];
            for (int e = 0; e < NENG; ++e) {
                const auto& M = ENGS[e];
                T.eng[(size_t) e].reset(makeBoxEngine(M.id));
                auto& E = *T.eng[(size_t) e];
                E.prepare(fs);
                for (int k = 0; k < M.n; ++k) T.ctl[(size_t) e].push_back(E.id(M.ids[k]));
                for (int i = 0; i < E.np; ++i) if (held(E, i)) E.p[(size_t) i] = E.D[i].mn;
                for (int i = 0; i < E.np; ++i) { const int w = vintageRole(E, i); if (w) T.vIdx[(size_t) e].push_back({ i, w == 1 }); }
                T.bmTouched.reserve(256);
                T.shown[(size_t) e].reset(new std::atomic<float>[(size_t) E.np]);
                for (int i = 0; i < E.np; ++i) T.shown[(size_t) e][(size_t) i].store(E.p[(size_t) i]);
            }
            for (auto& a : T.learnt) for (auto& x : a) x.store(-1);
            T.arp[0] = 0; T.arp[1] = 0; T.arp[2] = 1; T.arp[3] = 2; T.arp[4] = 50; T.arp[5] = 0;
            for (int k = 0; k < 16; ++k) { T.nsNote[(size_t) k] = 48 + (k % 4 == 2 ? 7 : k % 8 == 4 ? 12 : 0); T.nsGate[(size_t) k] = k % 2 == 0 ? 1 : 0; }
            T.bmTouched.reserve(64);
            for (int n = 0; n < 4; ++n) { T.bm[(size_t) (4 * n)] = .35f + .1f * n; T.bm[(size_t) (4 * n + 1)] = (float) n; }
            for (int q = 0; q < 4; ++q) T.bm[(size_t) (16 + 18 * q + 1)] = .5f;
            T.ns[0] = 0; T.ns[1] = 16; T.ns[2] = 2; T.ns[3] = 60; T.ns[4] = 0; T.ns[5] = 0;
            T.fx.reset(new oct::FxRack()); T.fx->prepare(fs);
            for (int s = 0; s < 8; ++s) setFx(*T.fx, "slot" + std::to_string(s), 0);   // the FX chain starts empty
            setFx(*T.fx, "insrc", 2);                                                    // the rack hears the track (not its demo loop)
            T.fxShown.reset(new std::atomic<float>[::fxp::NP]);
            for (int i = 0; i < ::fxp::NP; ++i) T.fxShown[(size_t) i].store(T.fx->p[i]);
            T.e.store(startEngines ? startEngines[t] : def[t]);
        }
        for (int s = 0; s < 8; ++s) slotParam[s] = fxIndex("slot" + std::to_string(s));
        spIdx = engineIndex("sp64"); lineIdx = engineIndex("line");
        for (int t = 0; t < NT; ++t) chan[(size_t) t].store(t);
        applyBpm(bpm.load());
    }

    static int fxIndex(const std::string& id) { for (int i = 0; i < ::fxp::NP; ++i) if (id == ::fxp::PDEFS[i].id) return i; return -1; }
    static void setFx(oct::FxRack& F, const std::string& id, float v) { const int i = fxIndex(id); if (i >= 0) F.p[i] = v; }

    oct::EngineBase& engine(int t) { auto& T = tr[(size_t) t]; return *T.eng[(size_t) T.e.load()]; }

    // tiny numbers decaying in filters and reverbs (denormals) cost up to 100x the CPU: flushed to zero on this thread
    static void noDenormals() {
#if defined(__aarch64__)
        uint64_t c; asm volatile("mrs %0, fpcr" : "=r"(c)); if (!(c & (1ull << 24))) { c |= 1ull << 24; asm volatile("msr fpcr, %0" : : "r"(c)); }
#elif defined(__x86_64__) || defined(__SSE2__)
        _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
    }
    // ---------------- the audio thread ----------------
    // one part: its engine (gliding the knobs that move), its output stage, its FX into fL / fR. Parts share nothing
    // here, so the 4 can run on 4 threads (PartPool)
    void renderPart(int t, int n) {
        auto& T = tr[(size_t) t]; const int e = T.e.load(std::memory_order_relaxed);
        auto& E = *T.eng[(size_t) e];
        const double bpmNow = bpm.load(std::memory_order_relaxed);
        E.hostBpm = bpmNow; for (int i = 0; i < 4; ++i) E.lfoSync[i] = T.lfoSync[(size_t) i].load(std::memory_order_relaxed);
        E.hostPpq = beat;
        if (e == lineIdx) { auto& Li = static_cast<oct::LineIn&>(E); Li.inL = inL; Li.inR = inR; }
        if (!T.ngl) E.process(T.L, T.R, n);
        else for (int o = 0; o < n; o += 16) {   // knobs moving: glide them every 16 samples (the engines' control rate)
            const float k = 1.f - std::exp(-16.f / (float) (fs * .010));
            for (int g = 0; g < T.ngl; ++g) {
                float& pv = E.p[(size_t) T.gl[g].i]; const float d = T.gl[g].to - pv; const float sp = E.D[T.gl[g].i].mx - E.D[T.gl[g].i].mn;
                if (std::fabs(d) <= sp * 1e-4f) { pv = T.gl[g].to; T.gl[g--] = T.gl[--T.ngl]; } else pv += d * k;
            }
            E.hostPpq = beat + o / fs * bpmNow / 60;
            E.hitOff = o; E.process(T.L + o, T.R + o, std::min(16, n - o));
        }
        E.hitOff = 0;
        {   // the part's output stage: DC blocked (5 Hz); with VINTAGE, a soft saturation that warms it (none at 0)
            const double R5 = 1 - oct::TP * 5 / fs; const float v = T.vint.load(std::memory_order_relaxed);
            const float g = 1 + 2.5f * v, a = v;
            for (int i = 0; i < n; ++i) {
                double l = T.L[i], r = T.R[i];
                const double yl = l - T.dcXL + R5 * T.dcYL; T.dcXL = l; T.dcYL = yl; const double yr = r - T.dcXR + R5 * T.dcYR; T.dcXR = r; T.dcYR = yr;
                float ol = (float) yl, orr = (float) yr;
                if (a > 0) { ol += a * (std::tanh(g * ol) / g - ol); orr += a * (std::tanh(g * orr) / g - orr); }
                T.L[i] = ol; T.R[i] = orr;
            }
        }
        T.engStep.store(E.step(), std::memory_order_relaxed);
        if (e == spIdx) { float hm[16]; E.meters(hm); uint64_t bits = 0; for (int q = 0; q < 4; ++q) bits |= (uint64_t) (uint32_t) hm[q] << (16 * q); padHits[(size_t) t].fetch_or(bits, std::memory_order_relaxed); }
    }
    // the part's effects: a second pass, once every part's engine has run, so a SIDECHAIN effect can key on any part's
    // sound and hits of the same block (sideKey) without delay
    oct::SideKey sideKey;
    void renderFx(int t, int n) {
        auto& T = tr[(size_t) t];
        bool anyFx = false; for (int s = 0; s < 8; ++s) anyFx = anyFx || T.fx->p[slotParam[s]] > .5f;
        if (anyFx) { T.fx->side = &sideKey; T.fx->hostClock = true; T.fx->hostPpq = beat; T.fx->process(T.L, T.R, T.fL, T.fR, n); }   // an empty chain is skipped: no CPU, no colour
        else { std::memcpy(T.fL, T.L, sizeof(float) * (size_t) n); std::memcpy(T.fR, T.R, sizeof(float) * (size_t) n); }
    }
    PartPool pool; int rendN = 0;
    double beat = 0;   // beats since the transport started (free running when stopped): the synced LFOs' clock
    // in: the line input's samples for this block (LINE IN parts play them), or none
    void process(float* outL, float* outR, int n, const float* in0 = nullptr, const float* in1 = nullptr) {
        noDenormals();
        while (n > MAXBLOCK) { process(outL, outR, MAXBLOCK, in0, in1); outL += MAXBLOCK; outR += MAXBLOCK; if (in0) in0 += MAXBLOCK; if (in1) in1 += MAXBLOCK; n -= MAXBLOCK; }
        inL = in0; inR = in1;
        Msg m;
        for (auto& T : tr) { auto& E = *T.eng[(size_t) T.e.load(std::memory_order_relaxed)]; std::fill(E.hitAt, E.hitAt + 8, -1); E.hitOff = 0; E.curI = 0; T.noteHit = false; }   // SIDECHAIN keys
        while (fromMidi.pop(m)) apply(m);
        while (fromUi.pop(m)) apply(m);
        for (auto& o : offs) if (o.t >= 0 && o.due <= now) { if (o.pad) engine(o.t).padOff(o.n); else { const uint8_t b[3] = { 0x80, (uint8_t) o.n, 0 }; midi(o.t, b); } o.t = -1; }
        SmpSwap x;   // samples for the SAMPLER: swapped in here, the old one goes back to be freed off this thread
        while (smpIn.pop(x)) { if (spIdx >= 0) { auto& S = static_cast<oct::Sp64&>(*tr[(size_t) x.t].eng[(size_t) spIdx]); *x.p = S.setSlot(x.slot, std::move(*x.p), x.user); } smpOut.push(x); }

        std::memset(outL, 0, sizeof(float) * (size_t) n); std::memset(outR, 0, sizeof(float) * (size_t) n);
        for (int t = 0; t < NT; ++t) { arpRun(t); nsRun(t); boxMod(t, n); vintage(t, n); }
        rendN = n; pool.run(NT, [](void* b, int t) { static_cast<Box*>(b)->renderPart(t, ((Box*) b)->rendN); }, this, n / fs);   // the parts, in parallel when busy
        sideKey.n = n;   // what the SIDECHAIN effects key on: each part's sound, hits and notes in this block
        for (int t = 0; t < NT; ++t) { auto& T = tr[(size_t) t]; auto& E = *T.eng[(size_t) T.e.load(std::memory_order_relaxed)];
            sideKey.L[t] = T.L; sideKey.R[t] = T.R; sideKey.note[t] = T.noteHit; for (int k = 0; k < 8; ++k) sideKey.hit[t][k] = E.hitAt[k]; }
        pool.run(NT, [](void* b, int t) { static_cast<Box*>(b)->renderFx(t, ((Box*) b)->rendN); }, this, n / fs);
        for (int t = 0; t < NT; ++t) {
            auto& T = tr[(size_t) t]; const int e = T.e.load(std::memory_order_relaxed);
            auto& E = *T.eng[(size_t) e];
            const float p = T.pan.load(std::memory_order_relaxed), v = T.mute.load(std::memory_order_relaxed) ? 0.f : T.vol.load(std::memory_order_relaxed);
            const float a = (p + 1) * .25f * 3.14159265f, tl = v * std::cos(a) * 1.41421356f, trg = v * std::sin(a) * 1.41421356f;
            const float k = 1.f / (float) n; float pk = 0;
            for (int i = 0; i < n; ++i) {
                const float gl = T.gL + (tl - T.gL) * (float) (i + 1) * k, gr = T.gR + (trg - T.gR) * (float) (i + 1) * k;
                const float l = T.fL[i] * gl, r = T.fR[i] * gr;
                outL[i] += l; outR[i] += r; pk = std::max(pk, std::max(std::fabs(l), std::fabs(r)));
            }
            if (t == selected.load(std::memory_order_relaxed)) { const unsigned w = scopeW.load(std::memory_order_relaxed);
                for (int i = 0; i < n; ++i) tune[(w + (unsigned) i) % SCOPE].store((T.fL[i] + T.fR[i]) * .5f, std::memory_order_relaxed); }
            T.gL = tl; T.gR = trg; T.meter.store(std::max(pk, T.meter.load(std::memory_order_relaxed) * .9f), std::memory_order_relaxed);
            for (int i = 0; i < E.np; ++i) T.shown[(size_t) e][(size_t) i].store(E.p[(size_t) i], std::memory_order_relaxed);
        }
        beat += n / fs * bpm.load(std::memory_order_relaxed) / 60;
        float pk = 0; const float mg = master.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) {   // master volume, then a soft limit above 0.8 so the box never clips hard
            outL[i] = soft(outL[i] * mg); outR[i] = soft(outR[i] * mg);
            pk = std::max(pk, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
        }
        masterMeter.store(std::max(pk, masterMeter.load(std::memory_order_relaxed) * .9f), std::memory_order_relaxed);
        const unsigned w = scopeW.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) scope[(w + (unsigned) i) % SCOPE].store((outL[i] + outR[i]) * .5f, std::memory_order_relaxed);
        scopeW.store(w + (unsigned) n, std::memory_order_release);
        now += n;
    }

    static float soft(float x) {
        const float a = std::fabs(x); if (a <= .8f) return x;
        const float y = .8f + .2f * std::tanh((a - .8f) / .2f); return x < 0 ? -y : y;
    }

    void applyBpm(float b) {
        for (auto& T : tr) {
            for (auto& E : T.eng) { const int i = E->id("bpm"); if (i >= 0) E->p[(size_t) i] = oct::cl(b, E->D[i].mn, E->D[i].mx); }
            setFx(*T.fx, "bpm", b);
        }
    }

    void noteOff(int t, int n) {
        auto& T = tr[(size_t) t];
        if (T.sustain) T.held[(size_t) n] = true; else engine(t).noteOff(n);
    }

    // what the box keeps off, whatever a multi, a CC or the panel says: SEQ 5 and 6 (not on the box), and the J8's
    // arpeggio and the J8's and CS8's chorus (the box has its own arpeggiator, and the J8 chorus is in the FX)
    static bool held(const oct::EngineBase& E, int i) {
        const char* id = E.D[i].id;
        if (!std::strcmp(id, "s4dst") || !std::strcmp(id, "s5dst")) return true;
        if (!std::strcmp(id, "chorus") && (E.id("udet") >= 0 || E.id("ribrng") >= 0)) return true;   // J8, CS8
        return !std::strcmp(id, "arp") && E.id("arprng") >= 0;
    }
    void setParam(int t, int i, float v) {
        auto& E = engine(t); if (i < 0 || i >= E.np || held(E, i)) return;
        const auto& d = E.D[i]; const float to = oct::cl(v, d.mn, d.mx);
        auto& T = tr[(size_t) t]; T.shown[(size_t) T.e.load()][(size_t) i].store(to);
        int k = 0; while (k < T.ngl && T.gl[k].i != i) ++k;
        if (d.type != 'f' || (k == T.ngl && k == 16) || oct::classicSound()) { E.p[(size_t) i] = to; if (k < T.ngl) T.gl[k] = T.gl[--T.ngl]; return; }
        T.gl[k] = { i, to }; if (k == T.ngl) ++T.ngl;
    }

    void apply(const Msg& m) {
        const int t = m.t < 0 || m.t >= NT ? 0 : m.t; auto& T = tr[(size_t) t];
        switch (m.k) {
        case PARAM: setParam(t, m.i, m.v); break;
        case FXPARAM: if (m.i >= 0 && m.i < ::fxp::NP) { const auto& d = ::fxp::PDEFS[m.i]; T.fx->p[m.i] = oct::cl(m.v, d.mn, d.mx); T.fxShown[(size_t) m.i].store(T.fx->p[m.i]); } break;
        case ENGINE: if (m.i >= 0 && m.i < NENG && m.i != T.e.load()) { std::fill(engine(t).ext.begin(), engine(t).ext.end(), 0.f); T.bmTouched.clear(); for (int g = 0; g < T.ngl; ++g) engine(t).p[(size_t) T.gl[g].i] = T.gl[g].to; T.ngl = 0; engine(t).allOff(); T.held.fill(false); T.e.store(m.i); } break;
        case VOL: T.vol.store(m.v); break;
        case PAN: T.pan.store(m.v); break;
        case LFOSYNC: if (m.i >= 0 && m.i < 8) T.lfoSync[(size_t) m.i].store(m.v > .5f); break;
        case TUNE: T.tuneSt.store(std::clamp(m.v, -12.f, 12.f)); break;
        case VINTAGE: T.vint.store(std::clamp(m.v, 0.f, 1.f)); break;
        case MUTE: T.mute.store(m.i != 0); break;
        case NOTE: {   // as a MIDI note, so the arpeggiator and REC take it too
            const uint8_t b[3] = { 0x90, (uint8_t) std::clamp(m.i, 0, 127), (uint8_t) std::clamp((int) std::lround(m.v * 127), 1, 127) }; midi(t, b); }
            if (m.ms > 0) for (auto& o : offs) if (o.t < 0) { o = { t, m.i, now + (int64_t) (m.ms * fs / 1000) }; break; }
            break;
        case BPM: bpm.store(m.v); applyBpm(m.v); break;
        case PLAY:
            if (m.i && !playing.load()) beat = 0;   // the synced LFOs start on the beat with the transport
            playing.store(m.i != 0);
            for (auto& U : tr) for (auto& E : U.eng) { const int i = E->id("play"); if (i >= 0) E->p[(size_t) i] = m.i ? 1.f : 0.f; }
            break;
        case PANIC: for (auto& U : tr) { for (auto& E : U.eng) E->allOff(); U.held.fill(false); U.sustain = false; } break;
        case MIDI: midi(t, m.m); break;
        case FXMOVE: moveFx(T, m.i, (int) m.v); break;
        case LEARN:
            if (m.v == 0) { learnT.store(t); learnI.store(m.i); learnedCc.store(-1); }
            else if (m.v == 1) { auto& L = T.learnt[(size_t) T.e.load()]; for (auto& x : L) if (x.load() == m.i) x.store(-1); learnT.store(-1); }
            else learnT.store(-1);
            break;
        case PADHIT:
            if (m.v > 0) { engine(t).padOn(m.i, m.v); if (m.ms > 0) for (auto& o : offs) if (o.t < 0) { o = { t, m.i, now + (int64_t) (m.ms * fs / 1000), true }; break; } }
            else engine(t).padOff(m.i);
            break;
        case ARP: if (m.i >= 0 && m.i < 6) { T.arp[(size_t) m.i] = (int) m.v; if (m.i == 0 && !m.v) arpStop(t); if (m.i == 5 && !m.v) { T.aLatch = false; T.aN = 0; } } break;
        case NOTESEQ:
            if (m.i == 100 && m.ms >= 0 && m.ms < 16) T.nsNote[(size_t) m.ms] = std::clamp((int) m.v, 0, 127);
            else if (m.i == 101 && m.ms >= 0 && m.ms < 16) T.nsGate[(size_t) m.ms] = std::clamp((int) m.v, 0, 2);
            else if (m.i >= 0 && m.i < 6) { T.ns[(size_t) m.i] = (int) m.v; if (m.i == 0 && !m.v) nsStop(t); if (m.i == 5) T.nsRecPos = 0; }
            break;
        case BOXMOD: if (m.i >= 0 && m.i < Track::BM_N) T.bm[(size_t) m.i] = m.v; break;
        case CCMAP: if (m.m[0] < NENG && m.ms >= 0 && m.ms < 128) T.learnt[m.m[0]][(size_t) m.ms].store(m.i); break;
        }
    }

    // an effect slot moved in the chain (as dragging a box in the FX plugin): its settings, its type and its running
    // state go with it; the others close up
    void moveFx(Track& T, int from, int to) {
        if (from < 0 || from > 7 || to < 0 || to > 7 || from == to) return;
        int a[8], n = 0; for (int k = 0; k < 8; ++k) if (k != from) a[n++] = k;
        for (int k = 7; k > to; --k) a[k] = a[k - 1]; a[to] = from;   // new place k gets old slot a[k]
        auto& F = *T.fx; static float op[::fxp::NP], ov[::fxp::NP];
        std::memcpy(op, F.p, sizeof op); std::memcpy(ov, F.v, sizeof ov);
        for (int k = 0; k < 8; ++k) {
            const int d = ::fxp::SLOT_BASE + k * ::fxp::SLOT_STRIDE, s = ::fxp::SLOT_BASE + a[k] * ::fxp::SLOT_STRIDE;
            std::memcpy(&F.p[d], &op[s], sizeof(float) * ::fxp::SLOT_STRIDE); std::memcpy(&F.v[d], &ov[s], sizeof(float) * ::fxp::SLOT_STRIDE);
            F.p[slotParam[k]] = op[slotParam[a[k]]]; F.v[slotParam[k]] = ov[slotParam[a[k]]];
        }
        F.permute(a);
        for (int i = 0; i < ::fxp::NP; ++i) T.fxShown[(size_t) i].store(F.p[i]);
    }

    // MIDI as in the plugin: notes (velocity .15 to 1), sustain, pitch bend, all notes off, one CC per control
    void midi(int t, const uint8_t* b) {
        auto& T = tr[(size_t) t]; auto& E = engine(t); const int st = b[0] & 0xF0;
        if (st == 0x90 && b[2] > 0) { T.noteHit = true; }
        if (st == 0x90 && b[2] > 0) {
            if (T.ns[5].load() && recordHere) nsRecord(t, b[1]);
            if (T.arp[0].load()) { arpAdd(t, b[1], b[2]); return; }
            T.held[(size_t) b[1]] = false; E.noteOn(b[1], .15 + .85 * b[2] / 127.0);
        }
        else if (st == 0x80 || st == 0x90) { if (T.arp[0].load()) { arpRemove(t, b[1]); return; } noteOff(t, b[1]); }
        else if (st == 0xE0) { T.pb = (((b[2] << 7) | b[1]) - 8192) / 8192.0 * BEND_RANGE; E.bend = T.pb + T.tuneSt.load(std::memory_order_relaxed); }
        else if (st == 0xB0) {
            const int cc = b[1];
            if (cc == 64) {
                T.sustain = b[2] >= 64;
                if (!T.sustain) for (int n = 0; n < 128; ++n) if (T.held[(size_t) n]) { T.held[(size_t) n] = false; E.noteOff(n); }
                return;
            }
            if (cc == 120 || cc == 123) { E.allOff(); T.held.fill(false); return; }
            auto& L = T.learnt[(size_t) T.e.load()];
            if (learnT.load() == t && learnI.load() >= 0) {   // MIDI learn: this CC now drives the control waiting for one
                const int li = learnI.load(); for (auto& x : L) if (x.load() == li) x.store(-1);
                L[(size_t) cc].store(li); learnedCc.store(cc); learnT.store(-1);
            }
            if (const int li = L[(size_t) cc].load(); li >= 0 && li < E.np) {
                const auto& d = E.D[li]; const float x = b[2] / 127.f;
                setParam(t, li, d.type == 'f' ? d.mn + x * (d.mx - d.mn) : std::floor(x * (d.mx - d.mn) + .5f) + d.mn); return;
            }
            int k = -1; for (int j = 0; j < synthmap::NPOOL; ++j) if (synthmap::CC_POOL[j] == cc) { k = j; break; }
            const auto& map = T.ctl[(size_t) T.e.load()];
            if (k < 0 || k >= (int) map.size() || map[(size_t) k] < 0) return;
            const auto& d = E.D[map[(size_t) k]]; const float x = b[2] / 127.f;
            setParam(t, map[(size_t) k], d.type == 'f' ? d.mn + x * (d.mx - d.mn) : std::floor(x * (d.mx - d.mn) + .5f) + d.mn);
        }
    }
};

// MIDI bytes from a device: each part plays its own MIDI channel (1-4 by default; parts on one channel are layered),
// a channel no part listens to plays the selected part,
// MIDI clock sets the tempo and start / stop run the sequencers, as the TEMPO & SYNCHRO page says (Box::sync).
// src: where the bytes came from (1 DIN, 2 USB, 3 Bluetooth); at: when, in seconds (any steady clock). Runs on the MIDI thread(s).
struct MidiIn {
    Box& box; double beatStart = -1; int clockN = 0;
    explicit MidiIn(Box& b) : box(b) {}
    void bytes(const uint8_t* b, size_t len, int src, double at) {
        if (len == 0) return;
        const int clk = box.sync.clock.load();
        if (b[0] == 0xF8) {   // 24 clocks per beat: the tempo over one beat
            if (clk != src) return;
            box.sync.lastClock.store(at);
            if (beatStart < 0 || at - beatStart > 2) { beatStart = at; clockN = 0; return; }
            if (++clockN >= 24) {
                const double beat = at - beatStart;
                if (beat > 0) { const float bpm = std::clamp((float) (60.0 / beat), 20.f, 300.f); if (std::fabs(bpm - box.bpm.load()) > .3f) { Msg m; m.k = BPM; m.v = std::round(bpm * 10) / 10; box.fromMidi.push(m); } }
                beatStart = at; clockN = 0;
            }
            return;
        }
        if (b[0] == 0xFA || b[0] == 0xFB || b[0] == 0xFC) {
            if (!box.sync.rxStart.load() || (clk != 0 && clk != src)) return;
            Msg m; m.k = PLAY; m.i = b[0] != 0xFC; box.fromMidi.push(m); if (b[0] == 0xFA) { beatStart = at; clockN = 0; } return;
        }
        if (b[0] >= 0xF0 || len < 2) return;
        const int ch = b[0] & 0x0F;
        Msg m; m.k = MIDI; m.m[0] = b[0]; m.m[1] = b[1]; m.m[2] = len > 2 ? b[2] : 0;
        bool any = false;
        for (int t = 0; t < NT; ++t) if (box.chan[(size_t) t].load() == ch) { m.t = (int8_t) t; box.fromMidi.push(m); any = true; }
        if (!any) { m.t = (int8_t) box.selected.load(); box.fromMidi.push(m); }
    }
};

} // namespace box
