// octavio-box in a web page: the synth (Box), the front panel's logic (Panel) and the screen (Screen) compiled to
// WebAssembly with wasi-sdk (box/web/build.sh). The same module is loaded twice:
//   - in an AudioWorklet, as the sound: ob_new(rate, 0), then ob_process() every 128 samples;
//   - on the page, as the front panel and the screen: ob_new(rate, 1). It keeps a copy of the synth's state (it never
//     makes sound): every change goes through it first, and ob_outbox() hands the page the messages to post to the
//     worklet, which applies the same ones. The levels go the other way (ob_meters -> ob_setMeters).
// The virtual front panel speaks the real panel's USB-MIDI protocol (facade-fw/PROTOCOLE.md) through ob_panelIn and
// gets the ring and LED messages back from ob_panelOut, exactly as the RP2040 would.
// Multis are kept in memory (the page stores them in the browser): ob_presetPut / ob_presetGet.
#include "../Box.h"
#include "../Panel.h"
#include "../ui/Canvas.h"
#include "../ui/Font.h"
#include "../ui/Screen.h"
#include <cstdlib>
#include <map>
#include <sstream>

using namespace box;
#define EXPORT(name) extern "C" __attribute__((export_name(#name)))

namespace {

struct Sim;
struct Host : PanelHost {
    Sim* s = nullptr;
    void send(int t, Msg m) override;
    void command(const std::string& line) override;
};

struct Sim {
    Box box; MidiIn midiIn { box }; bool ui;
    Lock lock; Host host; std::unique_ptr<Panel> panel; std::unique_ptr<ui::Screen> screen;
    std::unique_ptr<ui::Canvas> canvas; std::vector<uint8_t> rgba;
    std::vector<float> outbox; std::vector<uint8_t> panelOut; std::map<std::string, std::string> presets;
    float L[1024] {}, R[1024] {}, IL[1024] {}, IR[1024] {}, meters[15 + 16 * 5 + 4] {};   // ..., then the engines' sequencer steps (95-98)
    std::string text;   // strings handed to the page

    Sim(double fs, bool forUi) : box(fs), ui(forUi) {
        if (!ui) return;
        host.s = this;
        panel = std::make_unique<Panel>(box, host, [this](const std::vector<uint8_t>& m) { panelOut.insert(panelOut.end(), m.begin(), m.end()); }, "", lock);
        panel->connected = true;
        panel->clock = [this] { return now; }; box.recordHere = false;   // the sound's copy records (REC)
        panel->listDir = [this](const std::string& dir) {   // the "SD card": the files added on the page
            std::vector<Panel::Entry> v;
            if (dir.empty()) { v.push_back({ "SD CARD (files added on the page)", "sd", true }); return v; }
            v.push_back({ "..", "", true }); for (auto& f : sd) v.push_back({ f, f, false }); return v;
        };
        panel->listPresets = [this] { std::vector<std::string> v; for (auto& p : presets) v.push_back(p.first); return v; };
        screen = std::make_unique<ui::Screen>(box, *panel, host);
        canvas = std::make_unique<ui::Canvas>(ui::Screen::W, ui::Screen::H);
        rgba.resize((size_t) ui::Screen::W * ui::Screen::H * 4);
    }

    // a change: applied to this copy now, posted to the sound too
    void send(Msg m) {
        if ((int) m.k < 100) box.apply(m);
        const float f[8] = { (float) m.k, (float) m.t, (float) m.m[0], (float) m.m[1], (float) m.m[2], (float) m.i, m.v, (float) m.ms };
        outbox.insert(outbox.end(), f, f + 8);
    }
    void drainMidi() { Msg m; while (box.fromMidi.pop(m)) box.apply(m); }

    // the multi file format of the program (main.cpp: save / load): one command per line
    static void savePart(std::ostream& f, Box& b, int t, Panel* P = nullptr) {
        auto& T = b.tr[(size_t) t]; const int e = T.e.load(); auto& E = *T.eng[(size_t) e];
        f << "eng " << ENGS[e].id << "\n";
        for (int i = 0; i < E.np; ++i) { const float v = T.shown[(size_t) e][(size_t) i].load(); if (v != E.D[i].df && std::string(E.D[i].id) != "bpm" && std::string(E.D[i].id) != "play") f << "set " << E.D[i].id << " " << v << "\n"; }
        for (int i = 0; i < ::fxp::NP; ++i) { const float v = T.fxShown[(size_t) i].load(); if (v != ::fxp::PDEFS[i].df && std::string(::fxp::PDEFS[i].id) != "bpm") f << "fxset " << ::fxp::PDEFS[i].id << " " << v << "\n"; }
        for (int k = 0; k < Track::BM_N; ++k) f << "bmod " << k << " " << T.bm[(size_t) k].load() << "\n";
        for (int k = 0; k < 8; ++k) f << "lfosync " << k << " " << T.lfoSync[(size_t) k].load() << "\n";
        for (int k = 0; k < 6; ++k) f << "arp " << Box::ARP_KEYS[k] << " " << T.arp[(size_t) k].load() << "\n";
        for (int k = 0; k < 5; ++k) f << "nseq " << Box::NS_KEYS[k] << " " << T.ns[(size_t) k].load() << "\n";
        for (int k = 0; k < 16; ++k) f << "nstep " << k + 1 << " " << T.nsNote[(size_t) k].load() << " " << T.nsGate[(size_t) k].load() << "\n";
        f << "cc clear " << ENGS[e].id << "\n";
        for (int cc = 0; cc < 128; ++cc) { const int i = T.learnt[(size_t) e][(size_t) cc].load(); if (i >= 0 && i < E.np) f << "cc " << cc << " " << ENGS[e].id << " " << E.D[i].id << "\n"; }
        if (P) { bool any = false; for (int sl = 0; sl < 64; ++sl) any = any || !P->smpPath[t][sl].empty();
            if (any) { f << "sample clear\n"; for (int sl = 0; sl < 64; ++sl) if (!P->smpPath[t][sl].empty()) f << "sample " << sl + 1 << " " << P->smpPath[t][sl] << "\n"; } }
        if (P) for (int k = 0; k < 8; ++k) { const int i = P->custom[t][e][k]; f << "custom " << k + 1 << " " << ENGS[e].id << " " << (i >= 0 ? E.D[i].id : "-") << "\n"; }
    }
    std::string save() {
        std::ostringstream f; f << "# Octavio multi (4 parts)\nbpm " << box.bpm.load() << "\n";
        for (int t = 0; t < NT; ++t) {
            auto& T = box.tr[(size_t) t];
            f << "p " << t + 1 << "\nchan " << box.chan[(size_t) t].load() + 1 << "\nvol " << T.vol.load() << "\npan " << T.pan.load() << "\ntune " << T.tuneSt.load() << "\nvintage " << T.vint.load() << "\n" << (T.mute.load() ? "mute" : "unmute") << "\n";
            savePart(f, box, t, panel.get());
        }
        f << "p " << box.selected.load() + 1 << "\n"; return f.str();
    }
    void command(const std::string& line) {
        std::istringstream in(line); std::string c; if (!(in >> c) || c[0] == '#') return;
        const int sel = box.selected.load(); Msg m; m.t = (int8_t) sel;
        if (c == "p") { int t = 0; in >> t; if (t >= 1 && t <= NT) { box.selected.store(t - 1); m.k = (Kind) 100; m.i = t - 1; send(m); } }
        else if (c == "chan") { int ch = 0; in >> ch; if (ch >= 1 && ch <= 16) { m.k = PANIC; send(m); box.chan[(size_t) sel].store(ch - 1); m.k = (Kind) 101; m.i = ch - 1; send(m); } }
        else if (c == "eng") { std::string s; in >> s; const int e = engineIndex(s); if (e >= 0) { m.k = ENGINE; m.i = e; send(m); } }
        else if (c == "set") { std::string s; float v = 0; in >> s >> v; const int i = box.engine(sel).id(s); if (i >= 0) { m.k = PARAM; m.i = i; m.v = v; send(m); } }
        else if (c == "fxset") { std::string s; float v = 0; in >> s >> v; const int i = Box::fxIndex(s); if (i >= 0) { m.k = FXPARAM; m.i = i; m.v = v; send(m); } }
        else if (c == "tune") { float v = 0; in >> v; m.k = TUNE; m.v = v; send(m); }
        else if (c == "vintage") { float v = 0; in >> v; m.k = VINTAGE; m.v = v; send(m); }
        else if (c == "vol") { float v = .8f; in >> v; m.k = VOL; m.v = v; send(m); }
        else if (c == "pan") { float v = 0; in >> v; m.k = PAN; m.v = v; send(m); }
        else if (c == "mute" || c == "unmute") { m.k = MUTE; m.i = c == "mute"; send(m); }
        else if (c == "bpm") { float v = 120; in >> v; if (loading && (!box.sync.multiTempo.load() || box.sync.clock.load() != 0)) return; m.k = BPM; m.v = v; send(m); }
        else if (c == "sample" && panel) {   // the page hands the sound the file (ob_sampleReq)
            std::string a, f; in >> a; std::getline(in >> std::ws, f);
            auto one = [&](int sl, std::string x) {
                if (x.empty()) x = "kit";
                if (x != "kit" && x != "-" && std::find(sd.begin(), sd.end(), x) == sd.end()) { if (screen) screen->notify(x + " : not on the card", now); return; }
                panel->smpPath[sel][sl] = x == "kit" ? "" : x; smpReq.push_back({ sel * 64 + sl, x });
                if (x == "kit") panel->peaks[sel][sl].clear(); else if (x == "-") panel->peaks[sel][sl].assign(256, 0.f);
            };
            if (a == "clear") { for (int sl = 0; sl < 64; ++sl) if (!panel->smpPath[sel][sl].empty()) one(sl, "kit"); }
            else { const int sl = std::atoi(a.c_str()) - 1; if (sl >= 0 && sl < 64) one(sl, f); }
        }
        else if (c == "bmod") { int k = -1; float v = 0; in >> k >> v; if (k >= 0 && k < Track::BM_N) { m.k = BOXMOD; m.i = k; m.v = v; send(m); } }
    else if (c == "lfosync") { int k = -1, v = 0; in >> k >> v; if (k >= 0 && k < 8) { m.k = LFOSYNC; m.i = k; m.v = (float) v; send(m); } }   // LFO k on the tempo   // the box's LFOs / sequencers 5-8
        else if (c == "arp" || c == "nseq") {   // arp <on|mode|oct|rate|gate|latch> <n>; nseq <on|len|rate|gate|transpose|rec> <n>
            std::string k; int v = 0; in >> k >> v; const auto* K = c == "arp" ? Box::ARP_KEYS : Box::NS_KEYS;
            for (int q = 0; q < 6; ++q) if (k == K[q]) { m.k = c == "arp" ? ARP : NOTESEQ; m.i = q; m.v = (float) v; send(m); }
        }
        else if (c == "nstep") {   // nstep <1-16> <note> <gate 0 rest, 1 note, 2 tie>: a step of the note sequencer
            int k = 0, n = 60, g = 1; in >> k >> n >> g; if (k < 1 || k > 16) return;
            m.k = NOTESEQ; m.ms = k - 1; m.i = 100; m.v = (float) n; send(m); m.i = 101; m.v = (float) g; send(m);
        }
        else if (c == "cc") {
            std::string a, en, id; in >> a >> en >> id; const int e = engineIndex(en); if (e < 0) return;
            m.k = CCMAP; m.m[0] = (uint8_t) e;
            if (a == "clear") { m.i = -1; for (int cc = 0; cc < 128; ++cc) { m.ms = cc; send(m); } }
            else { m.ms = std::atoi(a.c_str()); m.i = box.tr[(size_t) sel].eng[(size_t) e]->id(id); if (m.i >= 0 && m.ms >= 0 && m.ms < 128) send(m); }
        }
        else if (c == "custom" && panel) {
            int k = 0; std::string en, id; in >> k >> en >> id; const int e = engineIndex(en);
            if (k >= 1 && k <= 8 && e >= 0) panel->custom[sel][e][k - 1] = id == "-" ? -1 : box.tr[(size_t) sel].eng[(size_t) e]->id(id);
        }
        else if (c == "sync") {   // the TEMPO & SYNCHRO page: here and in the sound's copy (it hears the MIDI clock too)
            std::string k; int v = 0; in >> k >> v; if (!box.setSync(k, v)) return;
            for (int q = 0; q < Box::NSYNC; ++q) if (k == Box::SYNC_KEYS[q]) { m.k = (Kind) 103; m.i = q; m.v = (float) v; send(m); }
        }
        else if (c == "play" || c == "stop") { m.k = PLAY; m.i = c == "play"; send(m); }
        else if (c == "save") {   // from the panel: "save <dir>/multi-00N.oct"
            std::string f; in >> f; const size_t p = f.find_last_of('/'); if (p != std::string::npos) f = f.substr(p + 1);
            presets[f] = save(); savedName = f;
            if (screen) { screen->notify("Saved: " + base(f), now); screen->setPresetName(base(f)); }
        }
        else if (c == "load") {
            std::string f; in >> f; const size_t p = f.find_last_of('/'); if (p != std::string::npos) f = f.substr(p + 1);
            auto it = presets.find(f); if (it == presets.end()) return;
            std::istringstream s(it->second); std::string l; ++loading; while (std::getline(s, l)) command(l); --loading;
            if (screen) { screen->notify("Loaded: " + base(f), now); screen->setPresetName(base(f)); }
        }
    }
    static std::string base(const std::string& f) { const size_t d = f.rfind('.'); return d == std::string::npos ? f : f.substr(0, d); }
    double now = 0; std::string savedName; int loading = 0;
    std::vector<std::string> sd;                          // the sound files the page has (its virtual SD card)
    std::vector<std::pair<int, std::string>> smpReq;      // samples the sound needs: slot, file ("kit", "-")
};

void Host::send(int t, Msg m) { m.t = (int8_t) t; s->send(m); }
void Host::command(const std::string& line) { s->command(line); }

Sim* S(int h) { return (Sim*) (uintptr_t) h; }

} // namespace

// ---------------- both ----------------
EXPORT(ob_new) int ob_new(double rate, int ui) { return (int) (uintptr_t) new Sim(rate, ui != 0); }
EXPORT(ob_alloc) void* ob_alloc(int n) { return std::malloc((size_t) n); }
EXPORT(ob_free) void ob_free(void* p) { std::free(p); }
// MIDI from the computer (Web MIDI or the page's keyboard): to both copies
// (Web MIDI counts as USB; at: the message's time in seconds, performance.now() / 1000)
EXPORT(ob_midi) void ob_midi(int h, int b0, int b1, int b2, int len, double at) {
    const uint8_t b[3] = { (uint8_t) b0, (uint8_t) b1, (uint8_t) b2 };
    S(h)->midiIn.bytes(b, (size_t) len, 2, at);
    if (S(h)->ui) S(h)->drainMidi();
}
// a message from the page's copy (8 numbers, see Sim::send)
EXPORT(ob_apply) void ob_apply(int h, float k, float t, float b0, float b1, float b2, float i, float v, float ms) {
    auto& s = *S(h); Msg m; m.t = (int8_t) t; m.m[0] = (uint8_t) b0; m.m[1] = (uint8_t) b1; m.m[2] = (uint8_t) b2; m.i = (int) i; m.v = v; m.ms = (int) ms;
    if ((int) k == 100) { s.box.selected.store(m.i); return; }
    if ((int) k == 101) { s.box.chan[(size_t) m.t].store(m.i); return; }
    if ((int) k == 102) { s.box.master.store(m.v); return; }   // the volume pot
    if ((int) k == 103) { if (m.i >= 0 && m.i < Box::NSYNC) s.box.setSync(Box::SYNC_KEYS[m.i], (int) m.v); return; }   // TEMPO & SYNCHRO
    m.k = (Kind) (int) k; while (!s.box.fromUi.push(m)) { Msg x; s.box.fromUi.pop(x); }
}

// ---------------- the sound (AudioWorklet) ----------------
EXPORT(ob_process) void ob_process(int h, int n) { auto& s = *S(h); s.box.process(s.L, s.R, n); }
// with the line input (the page's microphone / audio input, played by LINE IN parts): written into ob_inLeft / ob_inRight first
EXPORT(ob_processIn) void ob_processIn(int h, int n) { auto& s = *S(h); s.box.process(s.L, s.R, n, s.IL, s.IR); }
EXPORT(ob_inLeft) float* ob_inLeft(int h) { return S(h)->IL; }
EXPORT(ob_inRight) float* ob_inRight(int h) { return S(h)->IR; }
EXPORT(ob_left) float* ob_left(int h) { return S(h)->L; }
EXPORT(ob_right) float* ob_right(int h) { return S(h)->R; }
EXPORT(ob_meters) float* ob_meters(int h) {   // 4 parts, master, playing
    auto& s = *S(h); for (int t = 0; t < NT; ++t) s.meters[t] = s.box.tr[(size_t) t].meter.load();
    s.meters[4] = s.box.masterMeter.load(); s.meters[5] = s.box.playing.load() ? 1.f : 0.f;
    const uint64_t hb = s.box.padHits[(size_t) s.box.selected.load()].exchange(0); for (int q = 0; q < 4; ++q) s.meters[6 + q] = (float) ((hb >> (16 * q)) & 0xffff);
    for (int t = 0; t < NT; ++t) s.meters[10 + t] = (float) s.box.tr[(size_t) t].nsStep.load();   // where the note sequencers are
    int n = 0; Msg r; while (n < 16 && s.box.recOut.pop(r)) { float* q = s.meters + 15 + n * 5; q[0] = r.t; q[1] = (float) r.i; q[2] = r.v; q[3] = (float) r.ms; q[4] = 0; ++n; }   // the steps REC wrote
    s.meters[14] = (float) n;
    for (int t = 0; t < NT; ++t) s.meters[95 + t] = (float) s.box.tr[(size_t) t].engStep.load();   // where the engines' sequencers are
    return s.meters;
}

// the waveform and the tuner's signal: the sound's last n samples of each, side by side, for the page's copy
EXPORT(ob_scopeTail) float* ob_scopeTail(int h, int n) {
    auto& s = *S(h); static std::vector<float> v; v.resize((size_t) n * 2); const unsigned w = s.box.scopeW.load();
    for (int k = 0; k < n; ++k) { const unsigned i = (w - (unsigned) n + (unsigned) k) % Box::SCOPE; v[(size_t) k] = s.box.scope[i].load(); v[(size_t) (n + k)] = s.box.tune[i].load(); }
    return v.data();
}

// ---------------- the panel and the screen (page) ----------------
EXPORT(ob_scopePush) void ob_scopePush(int h, const float* f, int n) {
    auto& s = *S(h); const unsigned w = s.box.scopeW.load();
    for (int k = 0; k < n; ++k) { s.box.scope[(w + (unsigned) k) % Box::SCOPE].store(f[k]); s.box.tune[(w + (unsigned) k) % Box::SCOPE].store(f[n + k]); }
    s.box.scopeW.store(w + (unsigned) n);
}
EXPORT(ob_setRate) void ob_setRate(int h, double rate) { S(h)->box.fs = rate; }   // the sound's real rate (the tuner)
// the virtual SD card and the samples: the page adds a file's name, takes the requests, and gives the sound the data
EXPORT(ob_sdAdd) void ob_sdAdd(int h, const char* name) { auto& v = S(h)->sd; if (std::find(v.begin(), v.end(), name) == v.end()) { v.push_back(name); std::sort(v.begin(), v.end()); } }
EXPORT(ob_sampleReq) const char* ob_sampleReq(int h) {   // "part*64+slot|file", or "" when there is none
    auto& s = *S(h); if (s.smpReq.empty()) { s.text.clear(); return s.text.c_str(); }
    s.text = std::to_string(s.smpReq.front().first) + "|" + s.smpReq.front().second; s.smpReq.erase(s.smpReq.begin()); return s.text.c_str();
}
// (the sound) slot of part t gets n samples (R null: mono) at rate sr; n 0: the kit sound back (kit) or nothing (empty)
EXPORT(ob_sampleSet) void ob_sampleSet(int h, int t, int slot, const float* L, const float* R, int n, double sr, int empty) {
    auto& s = *S(h); if (s.box.spIdx < 0 || t < 0 || t >= NT) return;
    auto& E = static_cast<oct::Sp64&>(*s.box.tr[(size_t) t].eng[(size_t) s.box.spIdx]);
    if (n <= 0) { E.setSlot(slot, nullptr, empty != 0); return; }
    auto d = std::make_shared<oct::SmpData>(); d->L.assign(L, L + n); if (R) d->R.assign(R, R + n); d->sr = sr;
    E.setSlot(slot, d, true);
}
EXPORT(ob_fromSound) void ob_fromSound(int h, const float* m) {   // (the page's copy) the sequencers' steps and what REC wrote
    auto& s = *S(h); s.box.recordHere = false;
    for (int t = 0; t < NT; ++t) { s.box.tr[(size_t) t].nsStep = (int) m[10 + t]; s.box.tr[(size_t) t].engStep = (int) m[95 + t]; }
    for (int k = 0; k < (int) m[14]; ++k) { const float* q = m + 15 + k * 5; Msg r; r.k = NOTESEQ; r.t = (int8_t) q[0]; r.i = (int) q[1]; r.v = q[2]; r.ms = (int) q[3]; s.box.apply(r); }
}
EXPORT(ob_setPeaks) void ob_setPeaks(int h, int ps, const float* L, int n) {   // (the page's copy) a slot's waveform, from its sample
    auto& s = *S(h); if (!s.panel || ps < 0 || ps >= NT * 64) return; Panel::peaksOf(L, (size_t) n, s.panel->peaks[ps / 64][ps % 64]);
}
EXPORT(ob_padHits) void ob_padHits(int h, float a, float b, float c, float d) {   // (the page's copy) the pads the sound played
    auto& s = *S(h); const float v[4] = { a, b, c, d }; uint64_t bits = 0; for (int q = 0; q < 4; ++q) bits |= (uint64_t) (uint32_t) v[q] << (16 * q);
    s.box.padHits[(size_t) s.box.selected.load()].fetch_or(bits);
}
EXPORT(ob_command) void ob_command(int h, const char* line) { S(h)->command(line); }
EXPORT(ob_classic) void ob_classic(int on) { oct::classicSound() = on != 0; }   // the A/B switch (in the sound's copy)
EXPORT(ob_syncLines) const char* ob_syncLines(int h) { auto& s = *S(h); s.text = s.box.syncLines(); return s.text.c_str(); }
EXPORT(ob_fonts) int ob_fonts(int h, const unsigned char* reg, int nr, const unsigned char* bold, int nb) {
    return S(h)->screen->reg.loadMemory(reg, (size_t) nr) && S(h)->screen->bold.loadMemory(bold, (size_t) nb);
}
EXPORT(ob_setMeters) void ob_setMeters(int h, float a, float b, float c, float d, float master) {
    auto& s = *S(h); const float v[4] = { a, b, c, d };
    for (int t = 0; t < NT; ++t) s.box.tr[(size_t) t].meter.store(v[t]);
    s.box.masterMeter.store(master);
}
// the screen, as RGBA bytes for a canvas ImageData; now in seconds
EXPORT(ob_render) uint8_t* ob_render(int h, double now) {
    auto& s = *S(h); s.now = now; s.drainMidi();
    s.panel->refresh();
    s.screen->render(*s.canvas, now);
    const auto& p = s.canvas->px; uint8_t* o = s.rgba.data();
    for (size_t i = 0; i < p.size(); ++i) { o[i * 4] = (uint8_t) (p[i] >> 16); o[i * 4 + 1] = (uint8_t) (p[i] >> 8); o[i * 4 + 2] = (uint8_t) p[i]; o[i * 4 + 3] = 255; }
    return o;
}
EXPORT(ob_touch) void ob_touch(int h, int what, float x, float y, double now) { S(h)->now = now; S(h)->screen->touch(what, x, y, now); }
// the front panel's USB-MIDI, both ways
EXPORT(ob_panelIn) void ob_panelIn(int h, int b0, int b1, int b2, double now) {
    auto& s = *S(h); s.now = now; const uint8_t b[3] = { (uint8_t) b0, (uint8_t) b1, (uint8_t) b2 };
    if (b0 == 0xBF && b1 == 7) { s.box.master.store(b2 / 127.f * 1.2f); Msg m; m.k = (Kind) 102; m.v = s.box.master.load(); s.send(m); return; }
    s.panel->midi(b, 3);
}
EXPORT(ob_panelOut) uint8_t* ob_panelOut(int h, int* n) {   // the bytes the panel was sent since the last call
    auto& s = *S(h); s.text.assign(s.panelOut.begin(), s.panelOut.end()); s.panelOut.clear();
    *n = (int) s.text.size(); return (uint8_t*) s.text.data();
}
EXPORT(ob_resetLeds) void ob_resetLeds(int h) { S(h)->panel->forget(); }
// the messages for the sound, 8 numbers each
EXPORT(ob_outbox) float* ob_outbox(int h, int* n) {
    auto& s = *S(h); static std::vector<float> v; v.swap(s.outbox); s.outbox.clear(); *n = (int) v.size() / 8; return v.data();
}
// multis kept by the page
EXPORT(ob_presetPut) void ob_presetPut(int h, const char* name, const char* text) { S(h)->presets[name] = text; }
EXPORT(ob_presetName) const char* ob_presetName(int h, int k) {   // k-th name, or "" past the end
    auto& s = *S(h); int i = 0; for (auto& p : s.presets) if (i++ == k) { s.text = p.first; return s.text.c_str(); }
    s.text.clear(); return s.text.c_str();
}
EXPORT(ob_presetGet) const char* ob_presetGet(int h, const char* name) { auto& s = *S(h); auto it = s.presets.find(name); s.text = it == s.presets.end() ? "" : it->second; return s.text.c_str(); }
EXPORT(ob_presetDel) void ob_presetDel(int h, const char* name) { S(h)->presets.erase(name); }
EXPORT(ob_lastSaved) const char* ob_lastSaved(int h) { auto& s = *S(h); s.text = s.savedName; s.savedName.clear(); return s.text.c_str(); }
EXPORT(ob_selected) int ob_selected(int h) { return S(h)->box.selected.load(); }
EXPORT(ob_partChannel) int ob_partChannel(int h, int t) { return S(h)->box.chan[(size_t) t].load(); }
