// The synth's screen (Touch Display 2, 5", used in landscape: 1280 x 720).
// Sober, hardware style: black background, the four parts in their colours along the top, then the page chosen with the
// page buttons. The 8 controls of a page are laid out 2 x 4 like the 8 encoders to the right of the screen, each with a
// ring like the encoder's LED ring. A control that just moved lights up. Touch: tap a part to select it, drag a control
// up or down to change it, double-tap it to put it back to its default; on SEQ, draw the 16 steps with a finger.
// Top right: the output's waveform (tap it: a tuner for the selected part), the tempo (tap it: TEMPO & SYNC), play / stop.
// The screen only reads the synth (atomics) and acts through the front panel's logic (Panel), so the encoders, the
// buttons and the touch screen always do the same thing.
#pragma once
#include "../Box.h"
#include "../Panel.h"
#include "Canvas.h"
#include "Font.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <string>

namespace ui {

struct Screen {
    static constexpr int W = 1280, H = 720;
    box::Box& box; box::Panel& panel; box::PanelHost& host;
    const std::atomic<float>* cpu = nullptr;    // audio load in %, if known
    Font reg, bold;
    box::Lock& mx;   // the panel's lock (shared with the terminal): view, touch and drawing all hold it

    // colours
    static constexpr Px BG = rgb(0, 0, 0), CELL = rgb(14, 14, 16), CELL_HI = rgb(24, 24, 28), LINE = rgb(30, 30, 34);
    static constexpr Px TXT = rgb(242, 242, 242), TXT2 = rgb(138, 138, 142), TXT3 = rgb(70, 70, 74), TRACK = rgb(40, 40, 44);
    static constexpr Px PLAY = rgb(40, 210, 90), WARN = rgb(255, 70, 60);
    static Px partCol(int t) { const auto* c = box::Panel::COL[t]; return rgb(c[0] * 2, c[1] * 2, c[2] * 2); }   // the panel's LED colours, x2

    Screen(box::Box& b, box::Panel& p, box::PanelHost& h) : box(b), panel(p), host(h), mx(p.mx) {}
    bool loadFonts(const std::string& dir) {
        return reg.load(dir + "/DejaVuSansCondensed.ttf") && bold.load(dir + "/DejaVuSansCondensed-Bold.ttf");
    }
    // a short message at the bottom of the screen ("Multi loaded", ...)
    static std::string spaced(std::string s) { for (auto& c : s) if (c == '_') c = ' '; return s; }   // multi files have _ for spaces
    void notify(const std::string& m, double now) { box::Guard l(mx); msg = spaced(m); msgAt = now; }
    void setPresetName(const std::string& n) { box::Guard l(mx); presetName = spaced(n); }

    // ---------------- what the page shows ----------------
    enum Mode { UNI, LIST, BIPOLAR, EMPTY };
    struct Cell { std::string name, value; float norm = 0; Mode mode = EMPTY; bool off = false; std::string badge; bool hasSwitch = false; int part = -1; int cc = -1; float font = 0; };   // cc: the CC learnt for it; font: the value's size (0: the largest that fits)
    struct View {
        int keysOct = 4, keysRoot = 0; bool keys = false; bool sounds = false;   // sounds: PRESETS lists the engine's factory sounds   // the KEYS button: on, its octave, its key
        int page = box::Panel::VOICE, win = 0, nwin = 1, ncontrols = 0, first = 0, last = 0, pick = 0, part = 0, engine = 0; bool connected = false;
        Cell cell[8]; std::vector<std::string> presets; std::string title, group;
        int seq = -1, pat = -1, step = -1; float steps[16] {}; bool hasPitch = false; float pitches[16] {};   // hasPitch: the DFAM's step pitches   // pat: the DR8 track whose pattern is shown (steps 0 off, 1 on, 2 accent)
        int assign = -1, apick = 0; std::vector<std::pair<std::string, std::string>> picks;   // CUSTOM: choosing a knob's control (name, section)
        bool custom = false;
        int lfo = -1; bool sync = false;   // MOD > LFO n (0-7) and its SYNC
        bool nseq = false, rec = false; int nsLen = 16, nsSel = 0, nsPlay = -1; int nsNote[16] {}, nsGate[16] {};   // ENGINE > SEQUENCE; rec: REC on (SEQUENCE, SEQ)
        std::string kind; std::vector<float> bars; std::vector<char> barsBi; float xy[2] {}; std::string ribName;   // the group's kind (Panel::Group::kind)
        std::vector<float> wave; float wst = 0, wen = 1; bool wrev = false;
        bool pads = false, browse = false; int padSel = 0; std::string padNames[16], padSample, bdir; std::vector<std::pair<std::string, bool>> files; int bpick = 0;   // SAMPLER
        bool matrix = false; int mrows = 0, mcols = 0, mrow = 0, mcol = 0, mpin = 1; std::vector<int> pins; std::vector<std::string> srcs, dsts;   // the VC3's pin board
        struct Sw { std::string name, value, cap; bool on = false, two = false, pin = false; int pinv = 0; };   // pin: a VC3 patch pin (pinv: 1 white, 2 green)   // two: a two-way choice (shows its value)
        std::vector<Sw> sw;   // the group's on / off switches: buttons along the bottom
        std::vector<std::pair<std::string, int>> tabs; int tab = -1;   // the tab row at the bottom (LFO, SEQ, DR8 track)   // SEQ: the sequencer shown, the step playing, its 16 steps (-1..1)
    };

    static std::string upper(std::string s) { for (auto& c : s) c = (char) std::toupper((unsigned char) c); return s; }
    static std::string choice(const char* list, int k) {
        std::string c = list ? list : ""; size_t p = 0;
        while (k-- > 0 && p != std::string::npos) { p = c.find('|', p); if (p != std::string::npos) ++p; }
        if (p == std::string::npos) return "?";
        return c.substr(p, c.find('|', p) - p);
    }
    static Cell describe(const PDef& d, float v) {
        Cell c; c.name = upper(d.name);
        const float n = d.mx > d.mn ? (v - d.mn) / (d.mx - d.mn) : 0; c.norm = std::clamp(n, 0.f, 1.f);
        char b[32];
        if (d.type == 'c') { c.mode = LIST; c.value = upper(choice(d.choices, (int) std::lround(v - d.mn))); if (c.value == "VERMONA") c.value = "VERM"; }
        else if (d.type == 'b') { c.mode = LIST; c.value = v > .5f ? "ON" : "OFF"; c.off = v <= .5f; }
        else {
            c.mode = d.mn < 0 && d.mx > 0 ? BIPOLAR : UNI;
            const float span = d.mx - d.mn;
            if (d.choices && d.choices[0] == '#') std::snprintf(b, sizeof b, "%d", (int) std::lround(v));   // a whole-number control (the DX7's)
            else if (d.choices && !std::strcmp(d.choices, "dB")) std::snprintf(b, sizeof b, "%+.1f dB", v);   // LINE IN's gain and threshold
            else if (d.id && std::strstr(d.id, "f16att")) std::snprintf(b, sizeof b, "%d ms", (int) std::lround(v * 300));   // SIDECHAIN's times
            else if (d.id && std::strstr(d.id, "f16dec")) std::snprintf(b, sizeof b, "%d ms", (int) std::lround(10 * std::pow(200.0, v)));
            else if (d.mn == 0 && d.mx == 1) std::snprintf(b, sizeof b, "%d", (int) std::lround(v * 100));
            else if (span >= 100) std::snprintf(b, sizeof b, "%.0f", v);
            else if (span >= 10) std::snprintf(b, sizeof b, "%.1f", v);
            else std::snprintf(b, sizeof b, "%.2f", v);
            c.value = b;
        }
        return c;
    }

    float engineFont = 0;   // the size at which every engine's name fits in a ring (worked out once)
    double now = 0;   // the time of the picture being drawn (seconds, the same clock as MIDI's)
    bool clockAlive(double t) const { return t - box.sync.lastClock.load() < 1.0; }
    View view() {
        View V; const int t = box.selected.load(); auto& T = box.tr[(size_t) t]; const int e = T.e.load();
        V.part = t; V.engine = e;
        box::Guard l(panel.mx);
        V.page = panel.page; V.connected = panel.connected; V.pick = panel.presetPick;
        V.keys = panel.keys; V.keysOct = panel.keysOct; V.keysRoot = box.qScale.load() ? box.qRoot.load() : 0;
        using P = box::Panel;
        char b[32];
        auto& E0 = *T.eng[(size_t) e];
        auto paramCell = [&](int k, int i) {   // a control of the engine on cell k
            V.cell[k] = describe(E0.D[i], T.shown[(size_t) e][(size_t) i].load());
            V.cell[k].name = upper(panel.label(t, i)); V.cell[k].cc = panel.ccOf(t, i);
            const int sw = panel.switchOf(e, i);
            if (sw >= 0) { auto& c = V.cell[k]; c.hasSwitch = true; c.off = panel.levelOff(t, i); c.badge = c.off ? (box::Panel::isMute(E0.D[sw]) ? "MUTED" : "OFF") : "ON"; }
        };
        if (V.page == P::ENGINE) {   // the lit tab: by its place in the row (ENGINE, SEQUENCE, ARPEGGIATOR, CUSTOM), not its number
            V.tabs = panel.tabs(t, P::ENGINE); V.tab = 0;
            for (int q = 0; q < (int) V.tabs.size(); ++q) if (V.tabs[(size_t) q].second == panel.W(P::ENGINE)) V.tab = q;
        }
        if (panel.arpPage() || panel.nsPage()) {   // the arpeggiator, the note sequencer
            const bool arp = panel.arpPage();
            for (int k = 0; k < 8; ++k) { const auto o = arp ? panel.arpItem(t, k) : panel.nsItem(t, k); if (o.mode < 0 || o.name.empty()) continue;
                V.cell[k] = { o.name, o.value, o.norm, o.mode == 0 ? UNI : o.mode == 2 ? BIPOLAR : LIST }; V.cell[k].off = o.off; }
            if (arp) { V.title = "ARPEGGIATOR"; V.group = "part " + std::to_string(t + 1) + " · held notes played one by one, in time"; }
            else {
                V.title = "SEQUENCE"; V.nseq = true; V.group = "16 note steps · REC: play the keyboard to write";
                auto& T = box.tr[(size_t) t]; V.nsLen = T.ns[1].load(); V.nsSel = panel.nsSel; V.rec = T.ns[5].load(); V.nsPlay = T.nsStep.load();
                for (int k = 0; k < 16; ++k) {   // the notes as they will play: through the QUANTIZER when it acts on this part
                    const int tr = T.ns[4].load(), n = T.nsNote[(size_t) k].load(); V.nsNote[k] = std::clamp(box.quantize(t, n + tr) - tr, 0, 127); V.nsGate[k] = T.nsGate[(size_t) k].load(); }
            }
            return V;
        }
        if (V.page == P::ENGINE && panel.customOn()) {   // CUSTOM: the 8 controls the player chose
            V.custom = true; V.group = "CUSTOM";
            for (int k = 0; k < 8; ++k) { const int i = panel.slotOf(t, k); if (i >= 0 && i < E0.np) paramCell(k, i); }
            if (panel.assign >= 0) {
                V.assign = panel.assign; V.apick = panel.assignPick;
                for (const auto& q : panel.picks(t)) V.picks.push_back({ q.id < 0 ? "— NONE —" : upper(panel.label(t, q.id)), upper(q.sec) });
            }
            return V;
        }
        switch (V.page) {
        case P::ENGINE: {
            auto& c = V.cell;
            c[0] = { std::string("ENGINE · ") + P::family(e), ENGS[e].name, box::NENG > 1 ? (float) P::orderPos(e) / (box::NENG - 1) : 0, LIST };
            c[0].font = engineFont;   // every engine's name in the ring, all at the same size
            std::snprintf(b, sizeof b, "%d", (int) std::lround(T.vol.load() * 100)); c[1] = { "VOLUME", b, T.vol.load() / 1.5f, UNI };
            const float p = T.pan.load(); const int pp = (int) std::lround(std::fabs(p) * 50);
            if (pp == 0) std::snprintf(b, sizeof b, "C"); else std::snprintf(b, sizeof b, "%s %d", p < 0 ? "L" : "R", pp);
            c[2] = { "PAN", b, (p + 1) / 2, BIPOLAR };
            const int ch = box.chan[(size_t) t].load(); std::snprintf(b, sizeof b, "%d", ch + 1); c[3] = { "MIDI CHANNEL", b, ch / 15.f, LIST };
            std::snprintf(b, sizeof b, "%.1f", box.bpm.load()); c[4] = { "TEMPO", b, (box.bpm.load() - 40) / 160, UNI };
            if (panel.external()) { c[4].name = "TEMPO · " + std::string(P::CLOCK_NAMES[box.sync.clock.load()]); c[4].off = true; }
            { const float tu = T.tuneSt.load(); if (std::fabs(tu) < .005f) std::snprintf(b, sizeof b, "0"); else std::snprintf(b, sizeof b, "%+.2f", tu);
              c[5] = { "TUNE", b, (tu + 12) / 24, BIPOLAR }; }
            { const float vi = T.vint.load(); if (vi <= 0) std::snprintf(b, sizeof b, "CLEAN"); else std::snprintf(b, sizeof b, "%d", (int) std::lround(vi * 100));
              c[6] = { "VINTAGE", b, vi, UNI }; c[6].off = vi <= 0; }
            if (T.mute.load()) { c[1].value = "MUTED"; c[1].off = true; }
            break; }
        case P::PRESET:
            if (panel.presetKind == 0) {   // the engine's factory sounds
                V.sounds = true; V.pick = panel.soundPick;
                for (int i : P::sounds(e)) V.presets.push_back(box::FACTORY[i].name);
                V.pick = std::clamp(V.pick, 0, std::max(0, (int) V.presets.size() - 1));
                if (panel.loadedEng == e) V.group = panel.loadedSound;
            } else V.presets = panel.presets();
            if (!panel.naming) { V.tabs = panel.tabs(t, P::PRESET); V.tab = 1 + panel.presetKind; }   // MIXER | PRESETS · ENGINE | PRESETS · ALL TRACKS
            break;
        case P::SYNC: {
            V.tabs = panel.tabs(t, P::SYNC); V.tab = panel.syncTab;
            if (panel.quantOn()) {   // QUANTIZER: the note sequencers' notes pulled into a key
                auto& c = V.cell; const int sc = box.qScale.load(), qp = box.qParts.load();
                c[0] = { "KEY", P::KEY_NAMES[box.qRoot.load()], box.qRoot.load() / 11.f, LIST }; c[0].off = sc == 0;
                c[1] = { "SCALE", box::Box::SCALE_NAMES[sc], sc / (float) (box::Box::NSCALE - 1), LIST }; c[1].off = sc == 0;
                c[2] = { "ROUNDING", P::QMODE_NAMES[box.qMode.load()], box.qMode.load() / 2.f, LIST }; c[2].off = sc == 0;
                for (int p = 0; p < 4; ++p) { const bool on = (qp >> p) & 1;
                    c[4 + p] = { "SEQUENCE " + std::to_string(p + 1), on ? "ON" : "OFF", on ? 1.f : 0.f, LIST }; c[4 + p].off = !on || sc == 0; }
                V.title = "QUANTIZER";
                V.group = sc == 0 ? "scale OFF: the sequences play their notes as written · pick a scale to tune them"
                                  : std::string("ENGINE's sequences play in ") + P::KEY_NAMES[box.qRoot.load()] + " " + box::Box::SCALE_NAMES[sc] + " · tap a box to change it";
                break;
            }
            auto& c = V.cell; auto& S = box.sync; const bool ext = panel.external();
            auto onoff = [](bool on) { return std::string(on ? "ON" : "OFF"); };
            std::snprintf(b, sizeof b, "%.1f", box.bpm.load());
            c[0] = { ext ? "TEMPO IN" : "TEMPO", b, (box.bpm.load() - 40) / 160, UNI }; c[0].off = ext;
            c[1] = { "CLOCK", P::CLOCK_NAMES[S.clock.load()], S.clock.load() / 3.f, LIST };
            c[2] = { "START/STOP IN", onoff(S.rxStart.load()), S.rxStart.load() ? 1.f : 0.f, LIST }; c[2].off = !S.rxStart.load();
            c[3] = { "TAP TEMPO", ext ? "—" : panel.tapping() ? std::string(b) : "TAP", panel.tapping() ? 1.f : 0.f, LIST }; c[3].off = ext;
            c[4] = { "CLOCK OUT", P::TX_NAMES[S.txClock.load()], S.txClock.load() / 7.f, LIST }; c[4].off = S.txClock.load() == 0;
            c[5] = { "START/STOP OUT", onoff(S.txStart.load()), S.txStart.load() ? 1.f : 0.f, LIST }; c[5].off = !S.txStart.load() || S.txClock.load() == 0;
            c[6] = { "MIDI THRU", onoff(S.thru.load()), S.thru.load() ? 1.f : 0.f, LIST }; c[6].off = !S.thru.load();
            c[7] = { "MULTI'S TEMPO", onoff(S.multiTempo.load()), S.multiTempo.load() ? 1.f : 0.f, LIST }; c[7].off = !S.multiTempo.load();
            V.title = "TEMPO & SYNC";
            if (!ext) V.group = "internal clock · tap a box to change it · a page button to leave";
            else V.group = std::string("clock ") + P::CLOCK_NAMES[S.clock.load()] + (clockAlive(now) ? " : receiving" : " : nothing received yet");
            break; }
        case P::MIXER:
            for (int k = 0; k < 8; ++k) {
                const int p = k % 4; auto& Q = box.tr[(size_t) p]; auto& c = V.cell[k];
                if (k < 4) { std::snprintf(b, sizeof b, "%d", (int) std::lround(Q.vol.load() * 100)); c = { "VOLUME " + std::to_string(p + 1), b, Q.vol.load() / 1.5f, UNI };
                    c.hasSwitch = true; c.off = Q.mute.load(); c.badge = c.off ? "MUTED" : "ON"; }
                else { const float pn = Q.pan.load(); const int pp = (int) std::lround(std::fabs(pn) * 50);
                    if (pp == 0) std::snprintf(b, sizeof b, "C"); else std::snprintf(b, sizeof b, "%s %d", pn < 0 ? "L" : "R", pp);
                    c = { "PAN " + std::to_string(p + 1), b, (pn + 1) / 2, BIPOLAR }; }
                c.part = p;
            }
            V.group = "VOLUMES AND PANS OF THE 4 PARTS";
            V.tabs = panel.tabs(t, P::MIXER); V.tab = 0;   // MIXER | PRESETS
            break;
        default: {
            const auto& G = panel.groups(t, V.page); const P::Group* g = panel.group(t, V.page);
            V.group = g ? g->title : "";
            if (V.page == P::PAGE2) { const char* n = P::page2Name(e); V.title = panel.voiceSplit(t) ? "VOICE 2" : n ? n : "PAGE 2"; }
            V.ncontrols = panel.count(t, V.page); V.nwin = std::max(1, (int) G.size()); V.win = std::clamp(panel.W(V.page), 0, V.nwin - 1);
            for (int w = 0; w < (int) G.size(); ++w) for (int q = 0; q < 8; ++q) if (G[(size_t) w].id[q] != -1) {   // which controls this group shows
                if (w < V.win) ++V.first; else if (w == V.win) ++V.last;
            }
            V.last += V.first; ++V.first;
            V.tabs = panel.tabs(t, V.page);
            for (int q = 0; q < (int) V.tabs.size(); ++q) {
                const int tv = V.tabs[(size_t) q].second;
                if (tv <= box::Panel::PADTAB) { if (box::Panel::PADTAB - tv == panel.pad[t]) V.tab = q; }
                else if (tv <= V.win) V.tab = q;
            }
            if (!g) break;
            auto& E = *T.eng[(size_t) e];
            V.lfo = panel.lfoShown(t); if (V.lfo >= 0) V.sync = T.lfoSync[(size_t) V.lfo].load();
            for (int k = 0; k < 8; ++k) {
                const int i = g->id[k]; if (i == -1) continue;
                if (P::isVirtual(i)) {   // PAD, an LFO's destination / amount / LFO-S&H
                    P::VInfo o; if (!panel.vinfo(t, i, o)) continue;
                    auto& c = V.cell[k]; c.name = g->names[k].empty() ? upper(o.name) : g->names[k]; c.value = upper(o.value); c.norm = o.norm; c.off = o.off;
                    c.mode = o.mode == 1 ? LIST : o.mode == 2 ? BIPOLAR : UNI; continue;
                }
                V.cell[k] = describe(E.D[i], T.shown[(size_t) e][(size_t) i].load());
                V.cell[k].name = upper(panel.label(t, i)); V.cell[k].cc = panel.ccOf(t, i);
                const int sw = panel.switchOf(e, i);
                if (sw >= 0) {   // a level with its on / off switch (on the knob's push)
                    auto& c = V.cell[k]; c.hasSwitch = true; c.off = panel.levelOff(t, i);
                    c.badge = c.off ? (box::Panel::isMute(E.D[sw]) ? "MUTED" : "OFF") : "ON";
                }
                if (V.page == P::MOD && std::string(E.D[i].id).find("shape") != std::string::npos && (int) std::lround(T.shown[(size_t) e][(size_t) i].load()) == 4)
                    { V.cell[k].value = "S&H"; V.cell[k].off = true; }   // the shape knob rests while the LFO is a sample & hold
            }
            for (size_t q = 0; q < g->sw.size(); ++q) {
                const int i = g->sw[q]; const auto& d = E.D[i]; const float v = T.shown[(size_t) e][(size_t) i].load();
                View::Sw w; w.name = upper(d.name); if (const size_t gt = w.name.find(" > "); gt != std::string::npos) w.name = w.name.substr(gt + 3);
                w.pin = std::string(d.id).rfind("pin", 0) == 0; w.pinv = (int) std::lround(v);   // the short name: the section is written above
                w.cap = q < g->swCap.size() ? upper(g->swCap[q]) : ""; w.two = d.type == 'c';
                w.on = v > d.mn + .5f; if (w.two) w.value = upper(choice(d.choices, (int) std::lround(v - d.mn)));
                V.sw.push_back(w);
            }
            if (g->pads) {
                V.pads = true; V.title = V.browse ? "" : "PADS"; V.padSel = panel.pad[t]; const int bank = V.padSel / 16;
                for (int q = 0; q < 16; ++q) V.padNames[q] = upper(panel.slotName(t, panel.slotOfPad(t, bank * 16 + q)));
                V.padSample = upper(panel.slotName(t, panel.slotOfPad(t, V.padSel)));
                if (panel.browsing) {
                    V.browse = true; V.title = "LOAD A SOUND"; V.bpick = panel.bpick; V.bdir = panel.bdir.empty() ? "SOURCES" : panel.bdir;
                    for (const auto& x : panel.entries) V.files.push_back({ x.dir && x.name != ".." ? x.name + "/" : x.name, x.dir });
                }
            }
            if (g->matrix) {
                V.matrix = true; V.mrows = panel.pinRows(t); V.mcols = panel.pinCols(t); V.mrow = panel.mRow; V.mcol = panel.mCol; V.mpin = panel.mPin;
                for (int r = 0; r < V.mrows; ++r) { V.srcs.push_back(upper(panel.pinSide(t, r, 0, false)));
                    for (int c = 0; c < V.mcols; ++c) { const int i = panel.pinId(t, r, c); V.pins.push_back(i >= 0 ? (int) std::lround(T.shown[(size_t) e][(size_t) i].load()) : 0); } }
                for (int c = 0; c < V.mcols; ++c) V.dsts.push_back(upper(panel.pinSide(t, 0, c, true)));
            }
            V.kind = g->kind;
            for (int i : g->bars) { const auto& d = E.D[i]; const float v = T.shown[(size_t) e][(size_t) i].load(); const bool bi = d.mn < 0; V.barsBi.push_back(bi); V.bars.push_back(bi ? v / std::max(std::fabs(d.mn), d.mx) : (v - d.mn) / (d.mx - d.mn)); }
            if (V.kind == "xy" && g->id[0] >= 0 && g->id[1] >= 0) for (int q = 0; q < 2; ++q) { const auto& d = E.D[g->id[q]]; V.xy[q] = (T.shown[(size_t) e][(size_t) g->id[q]].load() - d.mn) / (d.mx - d.mn) * 2 - 1; }
            if (V.kind == "ribbon") { V.ribName = panel.ribTargets(t)[(size_t) std::clamp(panel.ribDst[t], 0, (int) panel.ribTargets(t).size() - 1)].first; }
            if (V.kind == "wave") {
                const int sl = panel.slotOfPad(t, panel.pad[t]); V.wave = panel.slotPeaks(t, sl);
                auto nv = [&](int i) { if (i < 0) return 0.f; const auto& d = E.D[i]; return (T.shown[(size_t) e][(size_t) i].load() - d.mn) / (d.mx - d.mn); };
                V.wst = nv(g->id[1]); V.wen = nv(g->id[2]); if (!g->sw.empty()) V.wrev = T.shown[(size_t) e][(size_t) g->sw[0]].load() > .5f;
            }
            if (V.lfo >= 0 && V.sync && g->id[0] != -1) {   // on the tempo: the rate is a note value
                V.cell[0].value = oct::EngineBase::SYNC_NAMES[oct::EngineBase::syncDiv(V.cell[0].norm)]; V.cell[0].mode = LIST; }
            if (g->seq >= 0) V.rec = panel.seqRec;
            if (g->seq >= 0 || g->pat >= 0) {
                V.seq = g->seq; V.pat = g->pat;
                for (int k = 0; k < 16; ++k) { if (g->pat < 0) { V.steps[k] = panel.stepVal(t, g->seq, k); continue; } const int si = panel.patId(t, g->pat, k); V.steps[k] = si >= 0 ? T.shown[(size_t) e][(size_t) si].load() : 0; }
                if (g->pat >= 0 && panel.pitchId(t, g->pat, 0) >= 0) { V.hasPitch = true; for (int k = 0; k < 16; ++k) V.pitches[k] = T.shown[(size_t) e][(size_t) panel.pitchId(t, g->pat, k)].load(); }
                if (box.playing.load()) V.step = ((T.engStep.load() % 16) + 16) % 16;   // from the sound (another copy in the browser)
            }
        } }
        return V;
    }

    static const char* pageName(int p) {
        static const char* N[] = { "ENGINE", "VOICE", "PAGE 2", "FILTER", "ENV", "MOD", "SEQ", "FX", "MIXER", "PRESETS", "SYNC" };
        return p >= 0 && p < box::Panel::NPAGE ? N[p] : "";
    }

    // ---------------- drawing ----------------
    // geometry
    // The page is drawn in its own coordinates and shifted up by DY on the screen (Canvas::ty): there is no row of
    // part blocks any more (the page's colour says which part; the 1-4 buttons change it). The title bar is then the
    // top of the screen, with the waveform, the tempo and play / stop in its right corner. FOOT and VH are where the
    // bottom of the screen falls in those coordinates, so the cells get the room the old row took.
    static constexpr float DY = 92, HEAD = 96, BAR = 168, GRID = 176, CW = 320, CH = 244, FOOT = 664 + DY, VH = H + DY;
    static constexpr float SWH = 64;   // the row of switch buttons (with their section names above)
    float ch = CH;                      // the cells' height now
    bool stepPage = false;              // a page with a row of steps (SEQ, the DFAM's pattern, SEQUENCE)
    void cellRect(int k, float& x0, float& y0, float& x1, float& y1) const {
        x0 = (k % 4) * CW + 8; y0 = GRID + (k / 4) * ch; x1 = x0 + CW - 16; y1 = y0 + ch - 12;
    }

    void render(Canvas& C, double t) {
        box::Guard l(mx); now = t;
        if (engineFont == 0) { engineFont = 36; for (int e = 0; e < box::NENG; ++e) while (engineFont > 16 && C.textWidth(bold, engineFont, ENGS[e].name) > 2 * 62 - 26) engineFont -= 1; }
        const View V = view();
        // every page the same: the cells leave room for two rows of buttons whether the page has them or not, so the
        // knobs are always the same size and the buttons always in the same place (the step pages keep their rows)
        stepPage = false;   // the step pages too: their row of steps sits where the second row of cells is
        ch = CELLH;
        swRects(V);
        tabsInBand = false;   // set again below for the pages of cells
        const int lt = box.learnT.load(), lcc = box.learnedCc.load();   // MIDI learn ended with a CC: say which
        if (learning && lt < 0 && lcc >= 0) { msg = "CC " + std::to_string(lcc) + " → " + learnName; msgAt = t; learning = false; }
        if (lt >= 0) learning = true;
        longPress(t);
        C.clear(BG); C.ty = -DY;
        header(C, V);
        if (V.matrix) { matrix(C, V, t); return; }
        if (V.pads) { pageBar(C, V); if (V.browse) browser(C, V); else padGrid(C, V, t); footer(C, V, t); return; }
        pageBar(C, V);
        if (V.assign >= 0) pickList(C, V);
        else if (V.page == box::Panel::PRESET && naming()) keyboard(C, V);
        else if (V.page == box::Panel::PRESET) { tabsInBand = !V.tabs.empty(); presetList(C, V); }
        else if (box::Panel::paged(V.page) && V.ncontrols == 0 && V.sw.empty())
            C.text(reg, 26, 40, 260, V.page == box::Panel::PAGE2 ? "This engine has no second page in the plugin." : "Nothing on this page for this engine.", TXT2);
        else { tabsInBand = !V.tabs.empty() && V.sw.empty() && !stepPage;
            grid(C, V, now); if (V.seq >= 0 || V.pat >= 0) steps(C, V); if (V.nseq) noteSteps(C, V); kindView(C, V); switches(C, V); }
        footer(C, V, now);
    }

    void header(Canvas& C, const View& V) {
        // no part blocks: the page's colour says which part, the 1-4 buttons change it
        // the output's waveform (or the tuner), the tempo (tap: TEMPO & SYNC), play / stop
        if (tuner) tunerView(C, V); else scope(C);
        const bool playing = box.playing.load(), ext = panel.external(), sync = V.page == box::Panel::SYNC;
        C.rrect(TX0, CY0, TX1, CY1, 10, rgb(20, 20, 23));
        if (sync) C.rframe(TX0, CY0, TX1, CY1, 10, 2, partCol(V.part));
        char b[32]; std::snprintf(b, sizeof b, "%.1f", box.bpm.load());
        const float tc = (TX0 + TX1) / 2;
        C.text(bold, 26, tc, CY0 + 30, b, ext ? TXT2 : TXT, 1);
        static const char* SHORT[4] = { "INTERNAL", "MIDI", "USB", "BLUETOOTH" };
        const int ck = box.sync.clock.load(); const bool lost = ext && !clockAlive(now);
        C.text(bold, 11, tc, CY1 - 7, C.fit(bold, 11, lost ? std::string(SHORT[ck]) + " ?" : SHORT[ck], TX1 - TX0 - 10), lost ? WARN : ext ? PLAY : TXT3, 1);
        const float cx = 1244, cy = (CY0 + CY1) / 2;
        C.disc(cx, cy, 26, playing ? PLAY : rgb(28, 28, 32));
        if (playing) C.rrect(cx - 9, cy - 9, cx + 9, cy + 9, 2, BG);
        else C.tri(cx - 7, cy - 12, cx - 7, cy + 12, cx + 13, cy, TXT);
    }

    // ---------------- the oscilloscope and the tuner (top right) ----------------
    // the corner of the title bar: the waveform (or the tuner), the tempo, play / stop
    static constexpr float SC0 = 872, SC1 = 1082, TX0 = 1090, TX1 = 1206, CY0 = HEAD + 6, CY1 = BAR - 10, RB = 856;   // RB: the title bar's other things end here
    bool tuner = false;
    void scope(Canvas& C) {
        const float x0 = SC0, x1 = SC1, y0 = CY0, y1 = CY1, mid = (y0 + y1) / 2, half = (y1 - y0) / 2 - 5;
        C.rrect(x0, y0, x1, y1, 10, rgb(10, 10, 12));
        C.rect(x0 + 8, mid - .5f, x1 - 8, mid + .5f, TRACK);
        constexpr unsigned N = 1024, M = box::Box::SCOPE;   // ~21 ms at 48 kHz
        const unsigned w = box.scopeW.load(std::memory_order_acquire);
        auto at = [&](unsigned i) { return box.scope[i % M].load(std::memory_order_relaxed); };
        unsigned s = w - N;   // starts on a rising zero crossing, so a steady sound stands still
        for (unsigned k = 0; k < 2048; ++k) { const unsigned i = w - N - k; if (at(i - 1) < 0 && at(i) >= 0) { s = i; break; } }
        const int cols = (int) (x1 - x0 - 16); float prev = at(s);
        for (int c = 0; c < cols; ++c) {
            const unsigned a = s + (unsigned) (c * (int) N / cols), e = s + (unsigned) ((c + 1) * (int) N / cols);
            float mn = prev, mx = prev;
            for (unsigned i = a; i < std::max(e, a + 1); ++i) { const float v = at(i); mn = std::min(mn, v); mx = std::max(mx, v); prev = v; }
            const float ya = mid - std::clamp(mx, -1.f, 1.f) * half, yb = mid - std::clamp(mn, -1.f, 1.f) * half;
            C.rect(x0 + 8 + (float) c, ya - .8f, x0 + 9.4f + (float) c, yb + .8f, rgb(205, 205, 210));
        }
    }
    // the tuner: the selected part alone (Box::tune), YIN on the last 1024 samples taken down to a quarter of the rate
    float tunerHz = 0; double tunerSeen = -10;
    float pitch() {
        constexpr int D = 4, L = 1024, WIN = 512; float x[L];
        const unsigned w = box.scopeW.load(std::memory_order_acquire), M = box::Box::SCOPE;
        double e = 0;
        for (int k = 0; k < L; ++k) { float a = 0; for (int j = 0; j < D; ++j) a += box.tune[(w - (unsigned) ((L - k) * D) + (unsigned) j) % M].load(std::memory_order_relaxed); x[k] = a / D; e += x[k] * x[k]; }
        if (e / L < 1e-6) return 0;   // silence
        const double fs = box.fs / D; const int lo = std::max(2, (int) (fs / 1200)), hi = std::min(L - WIN - 1, (int) (fs / 25));
        static float d[L]; d[0] = 1; double sum = 0; int best = -1;
        for (int tau = 1; tau <= hi; ++tau) {
            double q = 0; for (int j = 0; j < WIN; ++j) { const float u = x[j] - x[j + tau]; q += u * u; }
            sum += q; d[tau] = sum > 0 ? (float) (q * tau / sum) : 1.f;
        }
        for (int tau = lo; tau < hi; ++tau) if (d[tau] < .15f) { while (tau + 1 < hi && d[tau + 1] < d[tau]) ++tau; best = tau; break; }
        if (best < 0) return 0;
        const float a = d[best - 1], b = d[best], c = d[best + 1], den = a - 2 * b + c;
        const float t = best + (std::fabs(den) > 1e-9f ? .5f * (a - c) / den : 0.f);
        return (float) (fs / t);
    }
    void tunerView(Canvas& C, const View& V) {   // compact, in the corner: the note on the left, the needle on the right
        const float x0 = SC0, x1 = SC1, y0 = CY0, y1 = CY1; const Px pc = partCol(V.part);
        C.rrect(x0, y0, x1, y1, 10, rgb(10, 10, 12));
        const float f = pitch();
        if (f > 0) { tunerHz = tunerHz > 0 && std::fabs(std::log2(f / tunerHz)) < .03f ? tunerHz + (f - tunerHz) * .35f : f; tunerSeen = now; }
        const bool live = now - tunerSeen < .4;
        const float cx = x0 + 140, bx0 = cx - 56, bx1 = cx + 56, by = y1 - 14;
        C.rect(bx0, by - .5f, bx1, by + .5f, TRACK);
        for (int k = -2; k <= 2; ++k) { const float x = cx + k * 28; C.rect(x - .5f, by - (k ? 3 : 6), x + .5f, by + (k ? 3 : 6), k ? TXT3 : TXT2); }
        C.text(reg, 10, cx, y0 + 14, "TUNER " + std::to_string(V.part + 1), TXT3, 1);
        if (!live || tunerHz <= 0) { C.text(bold, 24, x0 + 12, y1 - 12, "—", TXT3); return; }
        static const char* NOTE[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const float m = 69 + 12 * std::log2(tunerHz / 440.f); const int n = (int) std::lround(m); const float cents = (m - (float) n) * 100;
        const bool ok = std::fabs(cents) < 3;
        const std::string name = std::string(NOTE[((n % 12) + 12) % 12]) + std::to_string(n / 12 - 1);
        C.text(bold, 24, x0 + 12, y0 + 28, name, ok ? PLAY : TXT);
        char b[32]; std::snprintf(b, sizeof b, "%+d c", (int) std::lround(cents)); C.text(reg, 11, x0 + 12, y1 - 6, b, TXT2);
        const float px = cx + std::clamp(cents, -50.f, 50.f) * 1.12f;
        C.rrect(px - 2.5f, by - 10, px + 2.5f, by + 10, 2, ok ? PLAY : pc);
    }

    void pageBar(Canvas& C, const View& V) {
        const Px pc = partCol(V.part);
        const std::string title = V.title.empty() ? pageName(V.page) : V.title;
        const float tw = C.text(bold, 44, 24, 150, title, TXT);
        char b[96];
        // the engine, then the plugin section(s) the knobs are on now
        std::snprintf(b, sizeof b, "%s%s%s", ENGS[V.engine].name, V.group.empty() ? "" : "  ·  ", V.group.c_str());
        const bool paged = box::Panel::paged(V.page) && V.ncontrols > 0 && !V.pads;
        char cnt[32] = "";
        if (paged && (V.page == box::Panel::SEQ || V.page == box::Panel::MOD || V.page == box::Panel::FX || V.pat >= 0)) std::snprintf(cnt, sizeof cnt, "%d / %d", V.win + 1, V.nwin);
        else if (paged) std::snprintf(cnt, sizeof cnt, "%d–%d / %d", V.first, V.last, V.ncontrols);
        const bool dots = paged && V.nwin > 1 && V.nwin <= 16;   // the groups of 8 as dots only (big enough to see)
        float right = dots ? RB - V.nwin * DOTS - 8 : paged && V.nwin > 16 ? RB - C.textWidth(reg, 22, cnt) - 16 : RB;
        if (V.nseq || V.seq >= 0 || V.lfo >= 0) { float r0, r1; recRect(paged, V.nwin, r0, r1); right = std::min(right, r0 - 12); }   // and before REC / SYNC
        if (V.page == box::Panel::SYNC && cpu) right = std::min(right, RB - 120.f);   // room for the CPU load
        if (V.keys) {   // the KEYS button is on: a little keyboard and the octave (C4...); knob 9 moves it
            char kb[16]; std::snprintf(kb, sizeof kb, "%s%d", box::Panel::KEY_NAMES[V.keysRoot], V.keysOct);
            const float iw = 44, ih = 26, w = 16 + iw + 12 + C.textWidth(bold, 28, kb) + 18, x1 = right - 4, x0 = x1 - w;
            const float y0 = CY0 + 4, y1 = CY1 - 4, iy = (y0 + y1) / 2 - ih / 2, ix = x0 + 16;
            C.rrect(x0, y0, x1, y1, 12, pc);
            C.rect(ix, iy, ix + iw, iy + ih, BG);                                   // the keyboard: 5 white keys, 3 black
            const float kw = (iw - 4) / 5;
            for (int q = 0; q < 5; ++q) C.rect(ix + 2 + q * kw + (q ? 1 : 0), iy + 2, ix + 2 + (q + 1) * kw - 1, iy + ih - 2, pc);
            for (int q : { 1, 2, 4 }) C.rect(ix + 2 + q * kw - 3.5f, iy + 2, ix + 2 + q * kw + 3.5f, iy + ih * .6f, BG);
            C.text(bold, 28, ix + iw + 12, (y0 + y1) / 2 + 10, kb, BG);
            right = x0 - 12;
        }
        C.text(bold, 22, 24 + tw + 22, 148, C.fit(bold, 22, V.page == box::Panel::MIXER || V.page == box::Panel::SYNC ? V.group : std::string(b), right - (24 + tw + 22)), pc);
        if (V.nseq || V.seq >= 0) recPill(C, V, paged);
        else if (V.lfo >= 0) syncPill(C, V, paged);
        if (V.page == box::Panel::SYNC && cpu) { char c[32]; std::snprintf(c, sizeof c, "CPU %d %%", (int) std::lround(cpu->load())); C.text(reg, 20, RB, 146, c, TXT3, 2); }
        if (V.group == "CHAIN") { C.text(reg, 18, RB, 146, "box + knob 9: the effect · drag: the order", TXT2, 2); C.rect(0, BAR - 1, W, BAR, LINE); return; }
        if (paged) {
            if (dots) for (int k = 0; k < V.nwin; ++k)   // one dot per group of 8 (knob 9 scrolls), the one shown in the part's colour
                C.disc(RB - DOTS / 2 - (V.nwin - 1 - k) * DOTS, (HEAD + BAR) / 2, k == V.win ? 8.f : 6.5f, k == V.win ? pc : TXT3);
            else if (V.nwin > 16) C.text(reg, 22, RB, 146, cnt, TXT2, 2);   // too many for dots: the count
        }
        C.rect(0, BAR - 1, W, BAR, LINE);
    }

    // REC: a button in the page bar (SEQUENCE: notes played go into the steps; SEQ: the AMOUNT knob writes the steps)
    static constexpr float DOTS = 24;   // the page dots' spacing
    void recRect(bool paged, int nwin, float& x0, float& x1) const { x1 = paged && nwin > 1 ? RB - nwin * DOTS - 20 : RB; x0 = x1 - 120; }   // left of the dots
    void recPill(Canvas& C, const View& V, bool paged) {
        float x0, x1; recRect(paged, V.nwin, x0, x1); const Px red = rgb(235, 60, 60);
        if (V.rec) C.rrect(x0, 112, x1, 152, 20, red); else C.rframe(x0, 112, x1, 152, 20, 2, red);
        C.disc(x0 + 24, 132, 7, V.rec ? TXT : red); C.text(bold, 18, x0 + 40, 139, "REC", V.rec ? TXT : red);
    }
    void syncPill(Canvas& C, const View& V, bool paged) {   // MOD > LFO n: SYNC, where REC is on the sequencer pages
        float x0, x1; recRect(paged, V.nwin, x0, x1); const Px pc = partCol(V.part);
        if (V.sync) C.rrect(x0, 112, x1, 152, 20, pc); else C.rframe(x0, 112, x1, 152, 20, 2, pc);
        C.text(bold, 18, (x0 + x1) / 2, 139, "SYNC", V.sync ? BG : pc, 1);
    }
    bool recTouch(float x, float y) {
        if (y < 106 || y > 158) return false;
        const View V = view(); if (!V.nseq && V.seq < 0 && V.lfo < 0) return false;
        float x0, x1; recRect(!V.nseq, V.nwin, x0, x1); if (x < x0 || x > x1) return false;
        const int t = box.selected.load(); box::Guard lp(panel.mx);
        if (V.lfo >= 0) { panel.lfoSyncToggle(t, V.lfo); return true; }
        if (V.nseq) panel.nsSet(t, 5, !box.tr[(size_t) t].ns[5].load());
        else { panel.seqRec = !panel.seqRec; const auto* g = panel.group(t, box::Panel::SEQ); if (g && g->seq >= 0) { const int k = ((box.tr[(size_t) t].engStep.load() % 16) + 16) % 16; panel.seqRecVal[g->seq] = panel.stepVal(t, g->seq, k); } }
        return true;
    }
    // ENGINE > SEQUENCE: the 16 steps (bottom row): a tap = rest -> note -> tie, drag up / down = the note
    void noteSteps(Canvas& C, const View& V) {
        const Px pc = partCol(V.part); float x0, y0, x1, y1; lowArea(x0, y0, x1, y1); C.rrect(x0, y0, x1, y1, 14, CELL);
        for (int k = 0; k < 16; ++k) {
            const float sx0 = stepX(k) + 5, sx1 = stepX(k + 1) - 5, sy0 = y0 + 14, sy1 = y1 - 30; const int g = V.nsGate[k]; const bool in = k < V.nsLen;
            const bool now = k == V.nsPlay;   // the step playing: its column lit, the step itself bright (as on SEQ)
            if (now) C.rrect(stepX(k) + 1, y0 + 4, stepX(k + 1) - 1, y1 - 4, 10, rgb(44, 44, 50));
            if (!in) { C.rframe(sx0, sy0, sx1, sy1, 8, 1, rgb(30, 30, 34)); continue; }
            if (g == 0) C.rframe(sx0, sy0, sx1, sy1, 8, 2, now ? TXT : rgb(46, 46, 52));
            else C.rrect(sx0, sy0, sx1, sy1, 8, now ? TXT : g == 2 ? scale(pc, .55f) : scale(pc, .85f));
            if (g == 2) C.rect(sx0 - 10, (sy0 + sy1) / 2 - 3, sx0, (sy0 + sy1) / 2 + 3, scale(pc, .55f));   // a tie: joined to the step before
            if (g) { const float h = (sy1 - sy0 - 50) * (V.nsNote[k] % 24) / 23.f; C.rect(sx0 + 8, sy1 - 34 - h - 2, sx1 - 8, sy1 - 34 - h + 2, BG); }
            C.text(bold, 16, (sx0 + sx1) / 2, sy1 - 12, box::Panel::noteName(V.nsNote[k]), g ? BG : TXT3, 1);
            if (k == V.nsSel) C.rframe(sx0 - 3, sy0 - 3, sx1 + 3, sy1 + 3, 10, 2, TXT);
            C.text(reg, 13, (sx0 + sx1) / 2, y1 - 2, std::to_string(k + 1), TXT3, 1);
        }
    }
    int noteStepAt(float x) const { return std::clamp((int) ((x - SX0) / (SX1 - SX0) * 16), 0, 15); }

    // per cell: the value it showed last frame, to light it up when it moves
    std::string lastKey[8]; std::string lastVal[8]; double changed[8] {};

    void grid(Canvas& C, const View& V, double now) {
        for (int k = 0; k < 8; ++k) {
            const Cell& c = V.cell[k]; const Px pc = partCol(c.part >= 0 ? c.part : V.part); float x0, y0, x1, y1; cellRect(k, x0, y0, x1, y1);
            const std::string key = std::to_string(V.part) + "/" + std::to_string(V.engine) + "/" + std::to_string(V.page) + "/" + std::to_string(V.win);
            if (key == lastKey[k] && c.value + c.badge != lastVal[k]) changed[k] = now;
            if (key != lastKey[k]) changed[k] = -10;
            lastKey[k] = key; lastVal[k] = c.value + c.badge;
            const float hot = c.mode == EMPTY ? 0.f : std::clamp(1.f - (float) (now - changed[k]) / 1.5f, 0.f, 1.f);
            if (drag.down && drag.cell == k) changed[k] = now;

            if (drag.down && drag.move && V.group == "CHAIN") {   // moving an effect: where it would go
                if (k == drag.target && k != drag.cell) C.rframe(x0 - 4, y0 - 4, x1 + 4, y1 + 4, 16, 3, TXT);
            }
            if (hidden(V, k) || (V.nseq && k >= 4)) continue;   // under the XY pad, the sliders, the ribbon, the waveform, the note steps
            if (c.mode == EMPTY && V.custom) {   // an empty CUSTOM knob
                C.rframe(x0, y0, x1, y1, 14, 1.5f, rgb(40, 40, 46));
                char nb[16]; std::snprintf(nb, sizeof nb, "KNOB %d", k + 1); C.text(bold, 18, x0 + 18, y0 + 34, nb, TXT3);
                C.text(bold, 44, (x0 + x1) / 2, (y0 + y1) / 2 + 14, "+", TXT3, 1);
                C.text(reg, 17, (x0 + x1) / 2, y1 - 22, "tap or push: pick", TXT3, 1);
                continue;
            }
            if (c.mode == EMPTY) { if (V.seq < 0 && V.pat < 0 && (V.sw.empty() || V.ncontrols)) C.rframe(x0, y0, x1, y1, 14, 1.5f, rgb(22, 22, 25)); continue; }
            C.rrect(x0, y0, x1, y1, 14, mix(CELL, CELL_HI, hot));
            if (hot > 0) C.rframe(x0, y0, x1, y1, 14, 2, pc, hot);
            float nameW = x1 - x0 - 36;
            if (c.hasSwitch) {   // the switch on the knob's push: a small ON / OFF tag
                const float bw = C.textWidth(bold, 15, c.badge) + 16, bx1 = x1 - 12, bx0 = bx1 - bw;
                if (c.off) C.rframe(bx0, y0 + 16, bx1, y0 + 40, 6, 1.5f, TXT3); else C.rrect(bx0, y0 + 16, bx1, y0 + 40, 6, pc);
                C.text(bold, 15, (bx0 + bx1) / 2, y0 + 33, c.badge, c.off ? TXT3 : BG, 1);
                nameW -= bw + 8;
            }
            C.text(bold, 21, x0 + 18, y0 + 34, C.fit(bold, 21, c.name, nameW), hot > .1f ? mix(TXT2, pc, hot) : TXT2);

            const float h = y1 - y0, cx = (x0 + x1) / 2, cy = y0 + (40 + (h - 28)) / 2, R = std::min(KNOB_R, (h - 68) / 2 - 6), th = 9, A = 2.356f;   // 270 degrees, open at the bottom
            C.arc(cx, cy, R, th, -A, A, TRACK);
            const Px vc = c.off ? scale(pc, .35f) : pc;
            if (c.mode == UNI) { if (c.norm > .003f) C.arc(cx, cy, R, th, -A, -A + 2 * A * c.norm, vc); }
            else if (c.mode == BIPOLAR) C.arc(cx, cy, R, th, 0, -A + 2 * A * c.norm, vc);
            else { const float a = -A + 2 * A * c.norm; C.arc(cx, cy, R, th + 2, a - .09f, a + .09f, vc); }
            if (V.group == "CHAIN" && k == panel.fxSel) {   // the slot whose effect knob 9 changes
                C.rframe(x0 - 4, y0 - 4, x1 + 4, y1 + 4, 16, 3, pc);
                C.text(bold, 15, cx, y1 - 14, "KNOB 9: EFFECT", pc, 1);
            }
            if (c.cc >= 0) C.text(bold, 14, x0 + 18, y1 - 14, "CC " + std::to_string(c.cc), pc);   // learnt
            // the value: in the ring if it fits, under it otherwise
            const Px tc = c.off ? TXT3 : TXT;
            if (c.font > 0) C.text(bold, c.font, cx, cy + c.font * .36f, c.value, tc, 1);
            else if (C.textWidth(bold, 36, c.value) <= 2 * R - 24) C.text(bold, 36, cx, cy + 13, c.value, tc, 1);
            else if (R < 62 && C.textWidth(bold, 30, c.value) <= 2 * R - 20) C.text(bold, 30, cx, cy + 11, c.value, tc, 1);
            else {
                const float sz = C.textWidth(bold, 24, c.value) <= x1 - x0 - 24 ? 24.f : 20.f;
                C.text(bold, sz, cx, y1 - 18, C.fit(bold, sz, c.value, x1 - x0 - 24), tc, 1);
            }
        }
    }

    // the group's on / off switches: rows of buttons under the knobs (one row up to 10, two beyond), in clusters named
    // after their section. A patch pin (VC3) is a button too: a tap goes off -> white -> green -> off.
    struct SwRect { float x0, x1, y0, y1; }; std::vector<SwRect> swr; std::vector<std::pair<SwRect, std::string>> swCaps;
    static constexpr float SWROW = 46;   // one row of buttons (40 + the gap)
    static constexpr float CELLH = (FOOT - GRID - (20 + 2 * SWROW) - 4) / 2;   // a cell's height, on every page
    static constexpr float KNOB_R = 62;  // every knob on the screen: this radius
    static int swRows(int n) { return n > 10 ? 2 : n ? 1 : 0; }
    static float swHeight(int n) { const int r = swRows(n); return r ? 20 + r * SWROW : 0; }
    static constexpr float SWBAND = 20 + 2 * SWROW;   // the band kept for the buttons on every page
    void swRects(const View& V) {
        swr.clear(); swCaps.clear(); const int n = (int) V.sw.size(); if (!n) return;
        const int rows = swRows(n), per = (n + rows - 1) / rows; const float top = FOOT - (stepPage ? swHeight(n) : SWBAND) + 18;   // first row always at the same height
        for (int r = 0; r < rows; ++r) {
            const int a = r * per, b = std::min(n, a + per); int clusters = 0;
            for (int q = a; q < b; ++q) if (q == a || V.sw[(size_t) q].cap != V.sw[(size_t) q - 1].cap) ++clusters;
            const float gap = 22, w = std::min(180.f, (1264 - gap * (clusters - 1)) / (b - a)); float x = 8;
            const float y0 = top + r * SWROW, y1 = y0 + (rows == 1 && !stepPage ? 2 * SWROW : SWROW) - 6;   // one row: the band's full height
            for (int q = a; q < b; ++q) {
                if (q == a || V.sw[(size_t) q].cap != V.sw[(size_t) q - 1].cap) { if (q != a) x += gap; swCaps.push_back({ { x, 0, y0, y1 }, V.sw[(size_t) q].cap }); }
                swr.push_back({ x + 2, x + w - 2, y0, y1 }); x += w;
            }
        }
    }
    static Px pinColor(int v) { return v == 2 ? rgb(70, 205, 100) : rgb(236, 236, 236); }
    void switches(Canvas& C, const View& V) {
        const Px pc = partCol(V.part);
        for (size_t c = 0; c < swCaps.size(); ++c) {
            const float x1 = c + 1 < swCaps.size() && swCaps[c + 1].first.y0 == swCaps[c].first.y0 ? swCaps[c + 1].first.x0 - 22 : 1272;
            if (c == 0 || swCaps[c].second != swCaps[c - 1].second) C.text(bold, 12, swCaps[c].first.x0 + 2, swCaps[c].first.y0 - 6, C.fit(bold, 12, swCaps[c].second, x1 - swCaps[c].first.x0 - 4), TXT3);
        }
        for (size_t q = 0; q < swr.size() && q < V.sw.size(); ++q) {
            const auto& w = V.sw[q]; const auto& r = swr[q]; const float cx = (r.x0 + r.x1) / 2, bw = r.x1 - r.x0 - 8, my = (r.y0 + r.y1) / 2 + 6;
            if (w.pin) {   // a patch pin: white or green when in
                if (w.pinv) { C.rrect(r.x0, r.y0, r.x1, r.y1, 8, pinColor(w.pinv)); C.text(bold, 14, cx, my, C.fit(bold, 14, w.name, bw), BG, 1); }
                else { C.rframe(r.x0, r.y0, r.x1, r.y1, 8, 1.5f, TXT3); C.text(bold, 14, cx, my, C.fit(bold, 14, w.name, bw), TXT2, 1); }
            } else if (w.two) {   // a two-way choice: its name small, its value
                C.rframe(r.x0, r.y0, r.x1, r.y1, 8, 1.5f, pc);
                C.text(reg, 11, cx, r.y0 + 15, C.fit(reg, 11, w.name, bw), TXT2, 1);
                C.text(bold, 15, cx, r.y1 - 8, C.fit(bold, 15, w.value, bw), TXT, 1);
            } else if (w.on) {
                C.rrect(r.x0, r.y0, r.x1, r.y1, 8, pc);
                C.text(bold, 15, cx, my, C.fit(bold, 15, w.name, bw), BG, 1);
            } else {
                C.rframe(r.x0, r.y0, r.x1, r.y1, 8, 1.5f, TXT3);
                C.text(bold, 15, cx, my, C.fit(bold, 15, w.name, bw), TXT2, 1);
            }
        }
    }
    int swAt(float x, float y) const {
        for (size_t q = 0; q < swr.size(); ++q) if (x >= swr[q].x0 && x < swr[q].x1 && y >= swr[q].y0 - 3 && y < swr[q].y1 + 3) return (int) q;
        return -1;
    }

    // ---------------- the group kinds: an XY pad, sliders, a ribbon, a sample's waveform ----------------
    static bool hidden(const View& V, int k) {
        if (V.kind == "xy") return k == 0 || k == 1 || k == 4 || k == 5;
        return (V.kind == "bars" || V.kind == "ribbon" || V.kind == "wave") && k >= 4;
    }
    void lowArea(float& x0, float& y0, float& x1, float& y1) const { x0 = 8; x1 = 1272; y0 = GRID + ch; y1 = GRID + 2 * ch - 12; }   // the bottom row
    void xyArea(float& x0, float& y0, float& x1, float& y1) const { x0 = 8; x1 = 2 * CW - 8; y0 = GRID; y1 = GRID + 2 * ch - 12; }
    void kindView(Canvas& C, const View& V) {
        const Px pc = partCol(V.part); float x0, y0, x1, y1;
        if (V.kind == "xy") {   // the joystick: a big pad, the stick where the finger is
            xyArea(x0, y0, x1, y1); C.rrect(x0, y0, x1, y1, 14, CELL);
            const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
            for (int q = 1; q < 8; ++q) { const float gx = x0 + (x1 - x0) * q / 8, gy = y0 + (y1 - y0) * q / 8; C.rect(gx - .5f, y0 + 10, gx + .5f, y1 - 10, q == 4 ? TRACK : LINE); C.rect(x0 + 10, gy - .5f, x1 - 10, gy + .5f, q == 4 ? TRACK : LINE); }
            const float px = cx + V.xy[0] * ((x1 - x0) / 2 - 24), py = cy - V.xy[1] * ((y1 - y0) / 2 - 24);
            C.rect(px - .75f, y0 + 10, px + .75f, y1 - 10, scale(pc, .5f)); C.rect(x0 + 10, py - .75f, x1 - 10, py + .75f, scale(pc, .5f));
            C.disc(px, py, 22, pc); C.disc(px, py, 9, BG);
            C.text(bold, 14, x0 + 16, y0 + 26, "JOYSTICK", TXT3); char b[48]; std::snprintf(b, sizeof b, "X %+.2f   Y %+.2f", V.xy[0], V.xy[1]); C.text(reg, 15, x1 - 16, y1 - 14, b, TXT2, 2);
            return;
        }
        lowArea(x0, y0, x1, y1);
        if (V.kind == "bars" && !V.bars.empty()) {   // sliders: drawn with a finger
            C.rrect(x0, y0, x1, y1, 14, CELL); const int n = (int) V.bars.size(); const float w = (x1 - x0 - 16) / n, top = y0 + 16, bot = y1 - 30;
            for (int k = 0; k < n; ++k) {
                const float bx0 = x0 + 8 + k * w + 6, bx1 = bx0 + w - 12, v = std::clamp(V.bars[(size_t) k], V.barsBi[(size_t) k] ? -1.f : 0.f, 1.f);
                C.rrect(bx0, top, bx1, bot, 6, rgb(22, 22, 26));
                if (V.barsBi[(size_t) k]) { const float mid = (top + bot) / 2, y = mid - v * (bot - top) / 2; C.rrect(bx0, std::min(y, mid), bx1, std::max(y, mid), 4, pc); }
                else { const float y = bot - v * (bot - top); C.rrect(bx0, y, bx1, bot, 4, pc); }
                C.text(reg, 14, (bx0 + bx1) / 2, y1 - 10, std::to_string(k + 1), TXT3, 1);
            }
        } else if (V.kind == "ribbon") {   // the CS8's ribbon: play it with a finger
            C.rrect(x0, y0, x1, y1, 14, CELL);
            const float ry0 = y0 + 44, ry1 = y1 - 24; C.rrect(x0 + 16, ry0, x1 - 16, ry1, 10, rgb(20, 20, 24));
            for (int q = 1; q < 24; ++q) { const float gx = x0 + 16 + (x1 - x0 - 32) * q / 24; C.rect(gx - .5f, ry0 + 8, gx + .5f, ry1 - 8, q == 12 ? TRACK : LINE); }
            C.text(bold, 16, x0 + 18, y0 + 30, "RIBBON → " + V.ribName, TXT2);
            C.text(reg, 14, x1 - 18, y0 + 30, V.ribName == "PITCH" ? "middle = the note played · let go: back to the middle" : "slide a finger", TXT3, 2);
            if (ribbonX >= 0) { const float px = x0 + 16 + ribbonX * (x1 - x0 - 32); C.rrect(px - 10, ry0 + 4, px + 10, ry1 - 4, 8, pc); }
        } else if (V.kind == "wave") {   // the pad's sample: start and end over it (drag a marker), reversed when REVERSE is on
            C.rrect(x0, y0, x1, y1, 14, CELL); const float wx0 = x0 + 16, wx1 = x1 - 16, mid = (y0 + y1) / 2, h = (y1 - y0) / 2 - 18;
            const int n = (int) V.wave.size(); float mx = 1e-6f; for (float a : V.wave) mx = std::max(mx, a);
            for (int k = 0; k < n; ++k) {
                const float a = V.wave[(size_t) (V.wrev ? n - 1 - k : k)] / mx, fx = (float) k / n, x = wx0 + fx * (wx1 - wx0), in = fx >= V.wst && fx <= V.wen;
                C.rect(x, mid - a * h - .5f, x + (wx1 - wx0) / n + .2f, mid + a * h + .5f, in ? pc : rgb(60, 60, 66));
            }
            if (!n) C.text(reg, 18, (x0 + x1) / 2, mid + 6, "no sound in this pad", TXT3, 1);
            for (int q = 0; q < 2; ++q) { const float x = wx0 + (q ? V.wen : V.wst) * (wx1 - wx0); C.rect(x - 1, y0 + 10, x + 1, y1 - 10, TXT); C.text(bold, 12, x + (q ? -4 : 4), y0 + 22, q ? "END" : "START", TXT, q ? 2 : 0); }
            if (V.wrev) C.text(bold, 13, x1 - 18, y1 - 10, "REVERSE", pc, 2);
        }
    }
    float ribbonX = -1;   // where the finger is on the ribbon (-1: not on it)
    // a touch on one of them (what: 0 down, 1 move, 2 up); true if it took it
    bool kindTouch(int what, float x, float y) {
        const int t = box.selected.load(); int pg; { box::Guard lp(panel.mx); pg = panel.page; }
        const View V = view(); float x0, y0, x1, y1;
        if (what == 0) drag.kind = false;
        if (what != 0 && !drag.kind) return false;
        box::Guard lp(panel.mx); const auto* g = panel.group(t, pg); if (!g) return false; auto& E = *box.tr[(size_t) t].eng[(size_t) box.tr[(size_t) t].e.load()];
        auto setn = [&](int i, float n) { if (i < 0) return; const auto& d = E.D[i]; panel.setp(t, i, d.mn + std::clamp(n, 0.f, 1.f) * (d.mx - d.mn)); };
        if (V.kind == "xy") {
            xyArea(x0, y0, x1, y1); if (what == 0 && (x < x0 || x > x1 || y < y0 || y > y1)) return false;
            setn(g->id[0], (x - x0 - 24) / (x1 - x0 - 48)); setn(g->id[1], 1 - (y - y0 - 24) / (y1 - y0 - 48)); drag.kind = what != 2; return true;
        }
        lowArea(x0, y0, x1, y1); if (what == 0 && (x < x0 || x > x1 || y < y0 || y > y1)) return false;
        if (V.kind == "bars" && !g->bars.empty()) {
            const int n = (int) g->bars.size(), k = std::clamp((int) ((x - x0 - 8) / ((x1 - x0 - 16) / n)), 0, n - 1);
            const float top = y0 + 16, bot = y1 - 30; const int i = g->bars[(size_t) k]; const auto& d = E.D[i];
            const float f = std::clamp((bot - y) / (bot - top), 0.f, 1.f);
            if (d.mn < 0) panel.setp(t, i, (f * 2 - 1) * std::max(std::fabs(d.mn), d.mx)); else setn(i, f);
            drag.kind = what != 2; return true;
        }
        if (V.kind == "ribbon") {
            const float f = std::clamp((x - x0 - 16) / (x1 - x0 - 32), 0.f, 1.f);
            ribbonX = what == 2 ? -1 : f; panel.ribbon(t, f, what != 2); drag.kind = what != 2; return true;
        }
        if (V.kind == "wave") {
            const float f = std::clamp((x - x0 - 16) / (x1 - x0 - 32), 0.f, 1.f);
            if (what == 0) drag.marker = std::fabs(f - V.wst) <= std::fabs(f - V.wen) ? 1 : 2;
            setn(g->id[(size_t) drag.marker], f); drag.kind = what != 2; return true;
        }
        return false;
    }

    // SEQ: the sequencer's 16 steps, drawn and edited with a finger (bottom row); a step from -1 to 1 around the middle
    static constexpr float SX0 = 8, SX1 = 1272, SY0 = GRID + CELLH, SY1 = FOOT - SWBAND + 6;   // above the button band, like the cells
    static float stepX(int k) { return SX0 + (SX1 - SX0) * k / 16.f; }
    static constexpr float PADY1 = SY0 + 80, PY0 = SY0 + 98, PY1 = SY1 - 26;   // the DFAM's pattern: pads, then the pitches
    void pitchAt(float x, float y, bool zero) {
        const View V = view(); if (V.pat < 0 || !V.hasPitch) return;
        const int k = std::clamp((int) ((x - SX0) / (SX1 - SX0) * 16), 0, 15); const float mid = (PY0 + PY1) / 2, half = (PY1 - PY0) / 2 - 4;
        const float v = zero ? 0.f : std::clamp((mid - y) / half, -1.f, 1.f);
        const int t = box.selected.load(); box::Guard lp(panel.mx); panel.setp(t, panel.pitchId(t, V.pat, k), std::fabs(v) < .04f ? 0.f : v);
    }
    void steps(Canvas& C, const View& V) {
        const Px pc = partCol(V.part);
        if (V.pat >= 0) {   // DR8: a row of 16 pads, off / on / accent
            C.rrect(SX0, SY0, SX1, SY1, 14, CELL);
            if (V.hasPitch) {   // the DFAM: each step's pitch under its pad, drawn with a finger
                const float mid = (PY0 + PY1) / 2, half = (PY1 - PY0) / 2 - 4;
                C.rect(SX0 + 10, mid - .5f, SX1 - 10, mid + .5f, TRACK);
                C.text(bold, 12, SX0 + 12, PY0 - 4, "PITCH", TXT3);
                for (int k = 0; k < 16; ++k) {
                    const float x0 = stepX(k) + 10, x1 = stepX(k + 1) - 10, v = std::clamp(V.pitches[k], -1.f, 1.f), y = mid - v * half;
                    if (std::fabs(v) > .004f) C.rrect(x0, std::min(y, mid), x1, std::max(y, mid), 3, k == V.step ? TXT : scale(pc, .9f)); else C.rrect(x0, mid - 2, x1, mid + 2, 2, TXT3);
                }
            }
            for (int k = 0; k < 16; ++k) {
                const float x0 = stepX(k) + 6, x1 = stepX(k + 1) - 6, y0 = SY0 + 24, y1 = V.hasPitch ? PADY1 : SY1 - 34; const int v = (int) std::lround(V.steps[k]);
                if (k % 4 == 0 && k) C.rect(stepX(k) - .5f, SY0 + 10, stepX(k) + .5f, SY1 - 10, LINE);
                const bool now = k == V.step;
                if (v == 0) C.rframe(x0, y0, x1, y1, 8, 2, now ? TXT : rgb(46, 46, 52));
                else { C.rrect(x0, y0, x1, y1, 8, v == 2 ? mix(pc, TXT, .55f) : scale(pc, .8f)); if (v == 2) C.text(bold, 15, (x0 + x1) / 2, y0 + 22, "ACC", BG, 1); }
                if (now) C.rframe(x0 - 4, y0 - 4, x1 + 4, y1 + 4, 10, 2, TXT);
                char b[8]; std::snprintf(b, sizeof b, "%d", k + 1); C.text(reg, 15, (x0 + x1) / 2, SY1 - 12, b, now ? TXT : TXT3, 1);
            }
            return;
        }
        // the help, where cells 3 and 4 would be
        C.text(bold, 22, 664, GRID + 40, "SEQUENCER " + std::to_string(V.seq + 1), TXT2);
        C.text(reg, 20, 664, GRID + 80, "Knob 1: where it goes    Knob 2: how much", TXT2);
        C.text(reg, 20, 664, GRID + 110, "The 16 steps: slide a finger over them to draw them,", TXT3);
        C.text(reg, 20, 664, GRID + 136, "double-tap a step to reset it.", TXT3);
        const float mid = (SY0 + SY1) / 2, half = (SY1 - SY0) / 2 - 10;
        C.rrect(SX0, SY0, SX1, SY1, 14, CELL);
        C.rect(SX0 + 10, mid - .5f, SX1 - 10, mid + .5f, TRACK);
        for (int k = 0; k < 16; ++k) {
            const float x0 = stepX(k) + 6, x1 = stepX(k + 1) - 6, v = std::clamp(V.steps[k], -1.f, 1.f);
            if (k % 4 == 0 && k) C.rect(stepX(k) - .5f, SY0 + 10, stepX(k) + .5f, SY1 - 10, LINE);
            const bool now = k == V.step;
            if (now) C.rrect(x0 - 3, SY0 + 6, x1 + 3, SY1 - 6, 8, rgb(30, 30, 34));
            const float y = mid - v * half;
            if (std::fabs(v) > .004f) C.rrect(x0, std::min(y, mid), x1, std::max(y, mid), 4, now ? TXT : pc);
            else C.rrect(x0, mid - 2, x1, mid + 2, 2, TXT3);
            char b[8]; std::snprintf(b, sizeof b, "%d", k + 1); C.text(reg, 15, (x0 + x1) / 2, SY1 - 14, b, now ? TXT : TXT3, 1);
        }
    }

    // PRESET: the saved multis; load one, save a new one, or save over the one picked
    std::vector<std::string> listOf(const View& V) const {
        if (V.sounds) return V.presets;
        std::vector<std::string> v; for (auto n : V.presets) { if (n.size() > 4) n = n.substr(0, n.size() - 4); for (auto& c : n) if (c == '_') c = ' '; v.push_back(upper(n)); }
        return v;
    }
    // CUSTOM: the list of the engine's controls, to put one on a knob
    void pickList(Canvas& C, const View& V) {
        const Px pc = partCol(V.part); const int n = (int) V.picks.size(), pick = V.apick;
        const float x0 = 16, x1 = 836, top = 186, rh = 52;
        const int first = firstRow(pick, n);
        for (int r = 0; r < ROWS && first + r < n; ++r) {
            const int i = first + r; const float y = top + r * rh; const bool on = i == pick;
            if (on) { C.rrect(x0, y, x1, y + rh - 6, 10, rgb(26, 26, 30)); C.rrect(x0, y, x0 + 6, y + rh - 6, 3, pc); }
            const auto& q = V.picks[(size_t) i];
            const float sw = C.textWidth(reg, 17, q.second);
            C.text(on ? bold : reg, 24, x0 + 24, y + 33, C.fit(bold, 24, q.first, x1 - x0 - 60 - sw), on ? TXT : TXT2);
            C.text(reg, 17, x1 - 16, y + 32, q.second, on ? pc : TXT3, 2);
        }
        char b[64]; std::snprintf(b, sizeof b, "KNOB %d", V.assign + 1);
        C.text(bold, 30, BX, 220, b, pc);
        C.text(reg, 21, BX, 262, "turn knob 9 (or this one):", TXT2);
        C.text(reg, 21, BX, 290, "pick · push: assign", TXT2);
        const char* lbl[3] = { "USE THIS CONTROL", "NONE", "CANCEL" };
        for (int q = 0; q < 3; ++q) {
            C.rrect(BX, BTN[q], 1256, BTN[q] + BH, 12, rgb(26, 26, 30));
            C.rframe(BX, BTN[q], 1256, BTN[q] + BH, 12, 2, q == 0 ? pc : TXT3);
            C.text(bold, 22, (BX + 1256) / 2, BTN[q] + 40, lbl[q], TXT, 1);
        }
    }
    void pickTouch(float x, float y) {   // the list (a tap picks, a second tap puts it on the knob) and the three buttons
        box::Guard lp(panel.mx);
        if (x >= BX) {
            for (int q = 0; q < 3; ++q) if (y >= BTN[q] && y < BTN[q] + BH) {
                if (q == 1) panel.assignPick = 0;
                if (q == 2) panel.assign = -1; else panel.confirmAssign();
            }
            return;
        }
        if (x > 836 || y < 186) return;
        const int n = (int) panel.picks(box.selected.load()).size();
        const int i = firstRow(panel.assignPick, n) + (int) ((y - 186) / 52); if (i < 0 || i >= n) return;
        if (i == panel.assignPick) panel.confirmAssign(); else panel.assignPick = i;
    }

    static constexpr int ROWS = 9;
    static int firstRow(int pick, int n) { return std::clamp(pick - ROWS / 2, 0, std::max(0, n - ROWS)); }
    static constexpr float BX = 880, BTN[3] = { 400, 478, 556 }, BH = 62;   // the three buttons
    void presetList(Canvas& C, const View& V) {
        const Px pc = partCol(V.part); const auto L = listOf(V); const int n = (int) L.size(), pick = V.pick;
        const float x0 = 16, x1 = 836, top = 186, rh = 52;
        if (!n) C.text(reg, 26, x0 + 16, top + 50, V.sounds ? "No factory sounds for this engine." : "No multis saved yet.", TXT2);
        const int first = firstRow(pick, n);
        for (int r = 0; r < ROWS && first + r < n; ++r) {
            const int i = first + r; const float y = top + r * rh; const bool on = i == pick;
            if (on) { C.rrect(x0, y, x1, y + rh - 6, 10, rgb(26, 26, 30)); C.rrect(x0, y, x0 + 6, y + rh - 6, 3, pc); }
            char b[16]; std::snprintf(b, sizeof b, "%02d", i + 1); C.text(reg, 22, x0 + 24, y + 33, b, on ? pc : TXT3);
            C.text(on ? bold : reg, 26, x0 + 72, y + 34, C.fit(reg, 26, L[(size_t) i], x1 - x0 - 100), on ? TXT : TXT2);
        }
        C.text(bold, 22, BX, 214, "KNOB 1", TXT2); C.text(reg, 22, BX, 244, "turn: pick · push: load", TXT2);
        if (V.sounds) {   // the engine's factory sounds: load into the part chosen (its controls back to the sound's)
            C.text(reg, 20, BX, 296, std::string("into part ") + std::to_string(V.part + 1) + ", " + ENGS[V.engine].name, TXT2);
            C.rrect(BX, BTN[0], 1256, BTN[0] + BH, 12, rgb(26, 26, 30)); if (n) C.rframe(BX, BTN[0], 1256, BTN[0] + BH, 12, 2, pc);
            C.text(bold, 22, (BX + 1256) / 2, BTN[0] + 40, "LOAD", n ? TXT : TXT3, 1);
            return;
        }
        C.text(bold, 22, BX, 296, "KNOB 2", TXT2); C.text(reg, 22, BX, 326, "push: save under a new name", TXT2);
        const char* lbl[3] = { "LOAD", "SAVE AS NEW", "OVERWRITE SELECTED" };
        for (int q = 0; q < 3; ++q) {
            const bool can = q == 1 || n > 0;
            C.rrect(BX, BTN[q], 1256, BTN[q] + BH, 12, rgb(26, 26, 30));
            if (can) C.rframe(BX, BTN[q], 1256, BTN[q] + BH, 12, 2, q == 0 ? pc : TXT3);
            C.text(bold, 22, (BX + 1256) / 2, BTN[q] + 40, lbl[q], can ? TXT : TXT3, 1);
        }
    }

    static constexpr float TAB0 = FOOT + 9, TAB1 = VH - 9, TABX0 = 8, TABX1 = 1272;
    static constexpr float BAND0 = FOOT - SWBAND + 18, BAND1 = FOOT - 6;   // the button band (tabs there when it has no buttons)
    bool tabsInBand = false;
    static void tabRect(int q, int n, float& x0, float& x1) { const float w = (TABX1 - TABX0) / n; x0 = TABX0 + q * w + 3; x1 = TABX0 + (q + 1) * w - 3; }
    void footer(Canvas& C, const View& V, double now) {   // the tab row, or the last message and the multi loaded
        C.rect(0, FOOT, W, FOOT + 1, LINE);
        if (box.learnT.load() >= 0) { learnBanner(C, V); return; }
        if (!V.tabs.empty()) {   // tabs: straight to an LFO, a sequencer, a track; in the button band when it is free
            const Px pc = partCol(V.part); const int n = (int) V.tabs.size();
            const float y0 = tabsInBand ? BAND0 : TAB0, y1 = tabsInBand ? BAND1 : TAB1, fs = tabsInBand ? 18.f : 17.f;
            for (int q = 0; q < n; ++q) {
                float x0, x1; tabRect(q, n, x0, x1); const bool on = q == V.tab;
                C.rrect(x0, y0, x1, y1, 8, on ? pc : rgb(24, 24, 28));
                C.text(bold, fs, (x0 + x1) / 2, (y0 + y1) / 2 + fs * .35f, C.fit(bold, fs, V.tabs[(size_t) q].first, x1 - x0 - 8), on ? BG : TXT2, 1);
            }
            if (!tabsInBand) return;
        }
        const float a = std::clamp(1.f - (float) (now - msgAt - 2.5) / .8f, 0.f, 1.f);
        if (!msg.empty() && a > 0) C.text(bold, 22, 24, VH - 20, msg, TXT, 0, a);
        if (!presetName.empty()) C.text(reg, 18, 1256, VH - 22, upper(presetName), TXT3, 2);
    }

    // ---------------- the SAMPLER's PADS ----------------
    // the 16 pads of the bank (tap = play it and edit it), the banks, the pad's sample, its main controls (the knobs)
    static constexpr float PX0 = 8, PW = 170, PH = 122, RX = 708;
    static constexpr float BANKY0 = 178, BANKY1 = 228, LOADY = 322, ORIGY = 390, PBH = 58;
    double padFlash[64]; bool padFlashInit = false;
    void padGrid(Canvas& C, const View& V, double t) {
        if (!padFlashInit) { for (auto& f : padFlash) f = -10; padFlashInit = true; }
        const uint64_t hits = box.padHits[(size_t) V.part].exchange(0);
        for (int k = 0; k < 64; ++k) if (hits >> k & 1) padFlash[k] = t;
        const Px pc = partCol(V.part); const int bank = V.padSel / 16;
        for (int q = 0; q < 16; ++q) {
            const int k = bank * 16 + q, c = q % 4, r = 3 - q / 4;   // pad 1 at the bottom left, as on the classic samplers
            const float x0 = PX0 + c * PW + 4, y0 = GRID + r * PH + 4, x1 = x0 + PW - 8, y1 = y0 + PH - 8;
            const float hot = std::clamp(1.f - (float) (t - padFlash[k]) / .25f, 0.f, 1.f);
            C.rrect(x0, y0, x1, y1, 12, mix(rgb(22, 22, 26), pc, hot * .85f));
            if (k == V.padSel) C.rframe(x0, y0, x1, y1, 12, 3, pc);
            char nb[8]; std::snprintf(nb, sizeof nb, "%c%d", 'A' + bank, q + 1);
            C.text(bold, 20, x0 + 14, y0 + 30, nb, hot > .4f ? BG : k == V.padSel ? pc : TXT2);
            const std::string& n = V.padNames[q];
            C.text(bold, 16, x0 + 14, y1 - 14, C.fit(bold, 16, n.empty() ? "—" : n, x1 - x0 - 24), hot > .4f ? BG : n.empty() ? TXT3 : TXT);
        }
        for (int b = 0; b < 4; ++b) {   // the banks
            const float w = (1272 - RX) / 4, x0 = RX + b * w + 3, x1 = x0 + w - 6; const bool on = b == bank;
            C.rrect(x0, BANKY0, x1, BANKY1, 8, on ? pc : rgb(24, 24, 28));
            char bn[16]; std::snprintf(bn, sizeof bn, "BANK %c", 'A' + b); C.text(bold, 17, (x0 + x1) / 2, BANKY1 - 17, bn, on ? BG : TXT2, 1);
        }
        char pn[24]; std::snprintf(pn, sizeof pn, "PAD %c%d", 'A' + bank, V.padSel % 16 + 1);
        C.text(bold, 30, RX + 4, 274, pn, TXT);
        C.text(bold, 20, RX + 4, 304, C.fit(bold, 20, V.padSample.empty() ? "NO SOUND" : V.padSample, 1272 - RX - 8), V.padSample.empty() ? TXT3 : pc);
        const char* lbl[2] = { "LOAD A SOUND", V.padSel < 16 ? "ORIGINAL SOUND" : "CLEAR" };
        for (int q = 0; q < 2; ++q) {
            const float y0 = q ? ORIGY : LOADY; C.rrect(RX, y0, 1272, y0 + PBH, 10, rgb(26, 26, 30)); C.rframe(RX, y0, 1272, y0 + PBH, 10, 2, q ? TXT3 : pc);
            C.text(bold, 20, (RX + 1272) / 2, y0 + 37, lbl[q], TXT, 1);
        }
        for (int k = 1; k < 8; ++k) {   // what knobs 2-8 do on the pad
            const Cell& c = V.cell[k]; if (c.mode == EMPTY) continue;
            const int col = (k - 1) % 2, row = (k - 1) / 2; const float x0 = RX + col * 284, y = 486 + row * 46;
            C.text(reg, 16, x0 + 4, y, C.fit(reg, 16, std::to_string(k + 1) + "  " + c.name, 160), TXT3);
            C.text(bold, 18, x0 + 272, y, C.fit(bold, 18, c.value, 100), TXT, 2);
        }
    }
    void browser(Canvas& C, const View& V) {   // LOAD A SOUND: folders and WAV files
        const Px pc = partCol(V.part); const int n = (int) V.files.size(), pick = V.bpick;
        const float x0 = 16, x1 = 836, top = 186, rh = 52;
        const int first = firstRow(pick, n);
        if (n <= 1 && V.bdir != "SOURCES") C.text(reg, 22, x0 + 16, top + 100, "No WAV files here.", TXT2);
        for (int r = 0; r < ROWS && first + r < n; ++r) {
            const int i = first + r; const float y = top + r * rh; const bool on = i == pick; const auto& f = V.files[(size_t) i];
            if (on) { C.rrect(x0, y, x1, y + rh - 6, 10, rgb(26, 26, 30)); C.rrect(x0, y, x0 + 6, y + rh - 6, 3, pc); }
            C.text(on || f.second ? bold : reg, 23, x0 + 24, y + 33, C.fit(bold, 23, f.first, x1 - x0 - 40), f.second ? (on ? pc : TXT2) : on ? TXT : TXT2);
        }
        char pn[24]; std::snprintf(pn, sizeof pn, "PAD %c%d", 'A' + V.padSel / 16, V.padSel % 16 + 1);
        C.text(bold, 28, BX, 220, pn, pc);
        C.text(bold, 18, BX, 250, C.fit(bold, 18, V.padSample.empty() ? "NO SOUND" : V.padSample, 1256 - BX), TXT2);
        C.text(reg, 19, BX, 296, "tap a sound: it goes in the pad", TXT2);
        C.text(reg, 19, BX, 322, "and it plays · knob 9: browse", TXT2);
        C.text(reg, 15, BX, 360, C.fit(reg, 15, V.bdir, 1256 - BX), TXT3);
        C.rrect(BX, BTN[2], 1256, BTN[2] + BH, 12, rgb(26, 26, 30)); C.rframe(BX, BTN[2], 1256, BTN[2] + BH, 12, 2, pc);
        C.text(bold, 22, (BX + 1256) / 2, BTN[2] + 40, "DONE", TXT, 1);
    }
    void padsTouch(int what, float x, float y) {
        const int t = box.selected.load(); box::Guard lp(panel.mx); const int bank = panel.pad[t] / 16;
        if (what == 2) { if (drag.padHit >= 0) panel.padHit(t, drag.padHit, 0); drag.padHit = -1; drag.down = false; return; }
        if (what != 0) return;
        if (panel.browsing) {
            if (x >= BX) { if (y >= BTN[2] && y < BTN[2] + BH) panel.browsing = false; drag.down = false; return; }
            if (x < 836 && y >= 186) { drag.list = true; drag.listAcc = 0; drag.browse = true; }
            return;
        }
        if (x < RX - 12 && y >= GRID) {   // a pad: it plays (until the finger lifts) and it is the pad edited
            const int c = std::clamp((int) ((x - PX0) / PW), 0, 3), r = std::clamp((int) ((y - GRID) / PH), 0, 3), k = bank * 16 + (3 - r) * 4 + c;
            panel.pickPad(t, k); panel.padHit(t, k, .9f); drag.padHit = k; padFlash[k] = now; return;
        }
        if (y >= BANKY0 && y < BANKY1) { const int b = std::clamp((int) ((x - RX) / ((1272 - RX) / 4)), 0, 3); panel.pickPad(t, b * 16 + panel.pad[t] % 16); }
        else if (y >= LOADY && y < LOADY + PBH) panel.openBrowser();
        else if (y >= ORIGY && y < ORIGY + PBH) { const int sl = panel.slotOfPad(t, panel.pad[t]); host.command("sample " + std::to_string(sl + 1) + (sl < 16 ? " kit" : " -")); }
        drag.down = false;
    }

    // ---------------- the VC3's PATCH MATRIX: a pin board ----------------
    // rows: the sources, columns: the destinations, as on the real one. Touch a hole and lift: the pin in hand goes in;
    // touch that pin: it comes out. While the finger is down, its row and column light up (slide to correct).
    static constexpr float MLX = 8, MGX0 = 190, MGX1 = 1272, MTOP = 144, MHEAD = 44, MBOT = VH - 8;
    static constexpr float PB0 = RB - 264, PB1 = RB - 134, PB2 = RB;   // the pin colour buttons (white, green), left of the corner
    float mcw(const View& V) const { return (MGX1 - MGX0) / std::max(1, V.mcols); }
    float mrh(const View& V) const { return (MBOT - MTOP - MHEAD) / std::max(1, V.mrows); }
    bool matrixAt(const View& V, float x, float y, int& r, int& c) const {
        if (x < MGX0 || x >= MGX1 || y < MTOP + MHEAD || y >= MBOT || !V.mrows) return false;
        c = std::clamp((int) ((x - MGX0) / mcw(V)), 0, V.mcols - 1); r = std::clamp((int) ((y - MTOP - MHEAD) / mrh(V)), 0, V.mrows - 1); return true;
    }
    static Px pinCol(int v) { return v == 2 ? rgb(70, 205, 100) : rgb(236, 236, 236); }
    void matrix(Canvas& C, const View& V, double t) {
        const Px pc = partCol(V.part); const float cw = mcw(V), rh = mrh(V), gy = MTOP + MHEAD;
        // the bar: title, where the cursor is, the pin in hand
        C.text(bold, 28, 24, 134, "PATCH MATRIX", TXT);
        const std::string at = (V.mrow < (int) V.srcs.size() ? V.srcs[(size_t) V.mrow] : "") + "  →  " + (V.mcol < (int) V.dsts.size() ? V.dsts[(size_t) V.mcol] : "");
        C.text(bold, 20, 290, 132, C.fit(bold, 20, at, PB0 - 300), pc);
        const float a = std::clamp(1.f - (float) (t - msgAt - 2.5) / .8f, 0.f, 1.f);
        if (!msg.empty() && a > 0) C.text(reg, 16, 290, 112, msg, TXT2, 0, a);
        for (int q = 0; q < 2; ++q) {
            const float x0 = q ? PB1 : PB0, x1 = q ? PB2 : PB1 - 8; const bool on = V.mpin == q + 1;
            C.rrect(x0, 104, x1, 140, 8, on ? rgb(30, 30, 34) : rgb(16, 16, 18)); if (on) C.rframe(x0, 104, x1, 140, 8, 2, pc);
            C.disc(x0 + 20, 122, 8, pinCol(q + 1)); C.text(bold, 16, x0 + 36, 128, q ? "GREEN PIN" : "WHITE PIN", on ? TXT : TXT3);
        }
        // the cursor's row and column, then the board
        C.rect(MLX, gy + V.mrow * rh, MGX1, gy + (V.mrow + 1) * rh, rgb(22, 22, 27));
        C.rect(MGX0 + V.mcol * cw, MTOP, MGX0 + (V.mcol + 1) * cw, MBOT, rgb(22, 22, 27));
        for (int c = 0; c < V.mcols; ++c) {   // the destinations, in two short lines
            const std::string& d = V.dsts[(size_t) c]; const size_t sp = d.find(' ', d.size() / 2 > 2 ? d.size() / 2 - 2 : 0);
            const std::string l1 = sp == std::string::npos ? d : d.substr(0, sp), l2 = sp == std::string::npos ? "" : d.substr(sp + 1);
            const float cx = MGX0 + (c + .5f) * cw; const Px tc = c == V.mcol ? pc : TXT2;
            C.text(bold, 12, cx, MTOP + (l2.empty() ? 30 : 18), C.fit(bold, 12, l1, cw - 6), tc, 1);
            if (!l2.empty()) C.text(bold, 12, cx, MTOP + 34, C.fit(bold, 12, l2, cw - 6), tc, 1);
        }
        for (int r = 0; r < V.mrows; ++r) {
            const float y0 = gy + r * rh, cy = y0 + rh / 2;
            C.text(bold, 14, MLX + 8, cy + 5, C.fit(bold, 14, V.srcs[(size_t) r], MGX0 - MLX - 14), r == V.mrow ? pc : TXT2);
            C.rect(MLX, y0 - .5f, MGX1, y0 + .5f, rgb(24, 24, 28));
            for (int c = 0; c < V.mcols; ++c) {
                const float cx = MGX0 + (c + .5f) * cw; const int v = V.pins[(size_t) (r * V.mcols + c)];
                if (v) { C.disc(cx, cy, std::min(rh, cw) * .34f, pinCol(v)); C.disc(cx, cy, std::min(rh, cw) * .12f, rgb(40, 40, 44)); }
                else C.disc(cx, cy, 3.5f, rgb(70, 70, 76));
            }
        }
        for (int c = 1; c < V.mcols; ++c) C.rect(MGX0 + c * cw - .5f, MTOP + 6, MGX0 + c * cw + .5f, MBOT, rgb(24, 24, 28));
        const float x0 = MGX0 + V.mcol * cw, y0 = gy + V.mrow * rh;
        C.rframe(x0 + 1, y0 + 1, x0 + cw - 1, y0 + rh - 1, 6, 2, pc);
    }

    // ---------------- MIDI learn ----------------
    // a long press (0.7 s) on a control or a switch: it waits for a CC from a MIDI controller (on the part's channel)
    bool learning = false; std::string learnName;
    void longPress(double t) {
        if (!drag.down || drag.moved || drag.list || drag.steps || t - drag.t0 < .7) return;
        const int part = box.selected.load(); int id = -1;
        { box::Guard lp(panel.mx);
          if (drag.sw >= 0) { const auto* g = panel.group(part, panel.page); if (g && drag.sw < (int) g->sw.size()) id = g->sw[(size_t) drag.sw]; }
          else if (drag.cell >= 0 && panel.page != box::Panel::SYNC && panel.page != box::Panel::MIXER && panel.page != box::Panel::PRESET) id = panel.knobId(part, drag.cell);
          if (id < 0) return;
          learnName = upper(panel.label(part, id)); box.learnedCc.store(-1); panel.learn(part, id, 0); }
        learning = true; drag.down = false; drag.sw = -1;
    }
    static constexpr float LB0 = 860, LB1 = 1060;   // the learn banner's buttons: forget its CC, stop
    void learnBanner(Canvas& C, const View& V) {
        const Px pc = partCol(V.part);
        C.rrect(8, TAB0, 1272, TAB1, 8, rgb(24, 24, 28)); C.rframe(8, TAB0, 1272, TAB1, 8, 2, pc);
        C.text(bold, 18, 24, TAB1 - 13, C.fit(bold, 18, "MIDI LEARN · " + learnName + " : turn a knob on the controller", LB0 - 40), TXT);
        C.rrect(LB0, TAB0 + 5, LB1 - 6, TAB1 - 5, 6, rgb(36, 36, 40)); C.text(bold, 16, (LB0 + LB1 - 6) / 2, TAB1 - 13, "FORGET ITS CC", TXT, 1);
        C.rrect(LB1, TAB0 + 5, 1264, TAB1 - 5, 6, rgb(36, 36, 40)); C.text(bold, 16, (LB1 + 1264) / 2, TAB1 - 13, "CANCEL", TXT, 1);
    }

    // ---------------- touch (coordinates in the 1280 x 720 picture) ----------------
    struct Drag { bool down = false, moved = false, steps = false, move = false, list = false, matrix = false, browse = false, pitch = false, kind = false; int padHit = -1, marker = 1, nstep = -1; float nAcc = 0; int cell = -1, target = -1, sw = -1; float x0 = 0, y0 = 0, last = 0, acc = 0, listAcc = 0; double t0 = 0; };
    Drag drag; int lastTapCell = -1; double lastTapAt = -10;
    // SEQ: a finger on the steps sets the step under it to its height
    void stepAt(float x, float y, bool zero) {
        const View V = view(); if (V.seq < 0) return;
        const int k = std::clamp((int) ((x - SX0) / (SX1 - SX0) * 16), 0, 15);
        const float mid = (SY0 + SY1) / 2, half = (SY1 - SY0) / 2 - 10;
        const float v = zero ? 0.f : std::clamp((mid - y) / half, -1.f, 1.f);
        const int t = box.selected.load(); box::Guard lp(panel.mx); panel.stepSet(t, V.seq, k, std::fabs(v) < .04f ? 0.f : v);
    }

    int cellAt(float x, float y) const {
        for (int k = 0; k < 8; ++k) { float x0, y0, x1, y1; cellRect(k, x0, y0, x1, y1); if (x >= x0 && x < x1 && y >= y0 && y < y1) return k; }
        return -1;
    }
    void knob(int k, int d) { box::Guard l(panel.mx); panel.knob(k, d); }
    void push(int k) { box::Guard l(panel.mx); panel.push(k); }

    // 0 = finger down, 1 = moved, 2 = up
    void touch(int what, float x, float y, double now) {
        box::Guard l(mx);
        y += DY;   // the screen's coordinates to the page's
        if (what == 0) {
            drag = Drag(); drag.down = true; drag.x0 = x; drag.y0 = drag.last = y; drag.t0 = now;
            if (y < BAR && x >= SC0 - 4) {   // the corner: the waveform / tuner, the tempo (TEMPO & SYNC), play / stop
                if (x < SC1 + 4) tuner = !tuner;
                else if (x < TX1 + 4) { box::Guard lp(panel.mx); panel.openSync(); }
                else push(8);
                drag.down = false; return;
            }
            int page; { box::Guard lp(panel.mx); page = panel.page; }
            if (y >= FOOT && box.learnT.load() >= 0) {   // MIDI learn: forget the control's CC, or stop waiting
                const int part = box.selected.load(); box::Guard lp(panel.mx);
                if (x >= LB0 && x < LB1) { panel.learn(part, box.learnI.load(), 1); msg = learnName + " : CC cleared"; msgAt = now; }
                else if (x >= LB1) panel.learn(part, -1, 2);
                learning = false; box.learnedCc.store(-1); drag.down = false; return;
            }
            if (y >= FOOT || (tabsInBand && y >= BAND0 && y < BAND1 + 6)) {   // the tab row
                const View V = view(); const int n = (int) V.tabs.size();
                for (int q = 0; q < n; ++q) { float x0, x1; tabRect(q, n, x0, x1); if (x >= x0 && x < x1) { box::Guard lp(panel.mx); panel.pickTab(box.selected.load(), page, V.tabs[(size_t) q].second); } }
                drag.down = false; return;
            }
            {
                const View V = view();
                if (V.pads && y >= GRID && y < FOOT) { padsTouch(0, x, y); return; }
                if (V.matrix) {
                    if (y >= 100 && y < 144 && x >= PB0) { box::Guard lp(panel.mx); panel.mPin = x < PB1 ? 1 : 2; drag.down = false; return; }
                    int r, c; if (matrixAt(V, x, y, r, c)) { box::Guard lp(panel.mx); panel.mRow = r; panel.mCol = c; drag.matrix = true; return; }
                    drag.down = false; return;
                }
            }
            if (page == box::Panel::PRESET) { presetTouch(x, y); drag.down = false; return; }
            int assigning; { box::Guard lp(panel.mx); assigning = panel.assign; }
            if (assigning >= 0) { if (y >= GRID && y < FOOT) { drag.list = true; drag.listAcc = 0; } return; }
            if (page == box::Panel::PAGE2 && y >= SY0 && y < SY1) {   // DR8 pattern: a tap = off -> on -> accent -> off
                const View V = view();
                if (V.pat >= 0 && V.hasPitch && y >= PADY1 + 8) {   // the DFAM's pitches: drawn like the SEQ steps
                    drag.pitch = true; const int k = (int) ((x - SX0) / (SX1 - SX0) * 16);
                    if (k == lastTapCell + 200 && now - lastTapAt < .4) { pitchAt(x, y, true); lastTapCell = -1; drag.down = false; return; }
                    lastTapCell = k + 200; lastTapAt = now; pitchAt(x, y, false); return;
                }
                if (V.pat >= 0) {
                    const int k = std::clamp((int) ((x - SX0) / (SX1 - SX0) * 16), 0, 15), t = box.selected.load();
                    box::Guard lp(panel.mx); panel.setp(t, panel.patId(t, V.pat, k), (float) (((int) std::lround(V.steps[k]) + 1) % 3));
                    drag.down = false; return;
                }
            }
            if (page == box::Panel::SEQ && y >= SY0 && y < SY1 && view().seq >= 0) {
                drag.steps = true; const int k = (int) ((x - SX0) / (SX1 - SX0) * 16);
                if (k == lastTapCell + 100 && now - lastTapAt < .4) { stepAt(x, y, true); lastTapCell = -1; drag.down = false; return; }
                lastTapCell = k + 100; lastTapAt = now; stepAt(x, y, false); return;
            }
            if (recTouch(x, y)) { drag.down = false; return; }
            {   // ENGINE > SEQUENCE: a step
                bool ns; { box::Guard lp(panel.mx); ns = panel.nsPage(); }
                float lx0, ly0, lx1, ly1; lowArea(lx0, ly0, lx1, ly1);
                if (ns && y >= ly0 && y < ly1) { drag.nstep = noteStepAt(x); drag.nAcc = 0; box::Guard lp(panel.mx); panel.nsSel = drag.nstep; return; }
            }
            if (const int q = swAt(x, y); q >= 0) { drag.sw = q; return; }   // an on / off switch: flipped when the finger lifts
            if (kindTouch(0, x, y)) return;
            drag.cell = cellAt(x, y);
            if (page == box::Panel::SYNC && drag.cell == 3) { push(3); drag.down = false; }   // tap tempo: on the finger going down
            return;
        }
        if (!drag.down) return;
        if (drag.kind) { kindTouch(what, x, y); if (what == 2) drag.down = false; return; }
        if (drag.nstep >= 0) {   // a note step: drag = the note (a semitone per 14 px), a tap = rest -> note -> tie
            const int t = box.selected.load(); auto& T = box.tr[(size_t) t]; const int k = drag.nstep;
            if (what == 1) { drag.nAcc += (drag.last - y) / 14.f; drag.last = y; if (std::fabs(y - drag.y0) > 8) drag.moved = true;
                const int d = (int) drag.nAcc; if (d) { drag.nAcc -= (float) d; box::Guard lp(panel.mx); panel.nsSet(t, 100, std::clamp(T.nsNote[(size_t) k].load() + d, 0, 127), k); if (!T.nsGate[(size_t) k].load()) panel.nsSet(t, 101, 1, k); } }
            if (what == 2) { if (!drag.moved) { box::Guard lp(panel.mx); panel.nsSet(t, 101, (T.nsGate[(size_t) k].load() + 1) % 3, k); } drag.nstep = -1; drag.down = false; }
            return;
        }
        if (drag.padHit >= 0) { if (what == 2) padsTouch(2, x, y); return; }
        if (drag.browse) {   // the browser's list: drag to scroll, a tap opens
            if (what == 1) { drag.listAcc += (drag.last - y) / 52.f; drag.last = y; if (std::fabs(y - drag.y0) > 8) drag.moved = true;
                const int d = (int) drag.listAcc; if (d) { drag.listAcc -= (float) d; box::Guard lp(panel.mx); panel.browseMove(d); } }
            if (what == 2) {
                if (!drag.moved) { box::Guard lp(panel.mx); const int n = (int) panel.entries.size(); const int i = firstRow(panel.bpick, n) + (int) ((y - 186) / 52); if (i >= 0 && i < n) panel.browseOpen(i); }
                drag.down = false;
            }
            return;
        }
        if (drag.matrix) {   // the pin board: the cursor follows the finger, the pin goes in (or out) where it lifts
            const View V = view(); int r, c;
            if (matrixAt(V, x, y, r, c)) { box::Guard lp(panel.mx); panel.mRow = r; panel.mCol = c;
                if (what == 2) panel.pinTap(box.selected.load(), r, c); }
            if (what == 2) drag.down = false;
            return;
        }
        if (drag.sw >= 0) {   // a switch: a tap flips it (a long press learns a CC instead: longPress)
            if (what == 1 && (std::fabs(y - drag.y0) > 12 || std::fabs(x - drag.x0) > 12)) drag.moved = true;
            if (what == 2) {
                if (!drag.moved) { const int t = box.selected.load(); box::Guard lp(panel.mx); int pg = panel.page; const auto* g = panel.group(t, pg);
                    if (g && drag.sw < (int) g->sw.size()) panel.flip(t, g->sw[(size_t) drag.sw]); }
                drag.down = false; drag.sw = -1;
            }
            return;
        }
        if (drag.list) {   // the CUSTOM list: drag to scroll, a tap picks
            if (what == 1) { drag.listAcc += (drag.last - y) / 52.f; drag.last = y; if (std::fabs(y - drag.y0) > 8) drag.moved = true;
                const int d = (int) drag.listAcc; if (d) { drag.listAcc -= (float) d; box::Guard lp(panel.mx); panel.movePick(d); } }
            if (what == 2) { if (!drag.moved) pickTouch(x, y); drag.down = false; }
            return;
        }
        if (drag.pitch) { if (what == 1) pitchAt(x, y, false); else if (what == 2) drag.down = false; return; }
        if (drag.steps) { if (what == 1) stepAt(x, y, false); else drag.down = false; return; }
        if (what == 1 && drag.cell >= 0) {
            if (!drag.moved && (std::fabs(y - drag.y0) > 8 || std::fabs(x - drag.x0) > 8)) {
                drag.moved = true;   // FX CHAIN: sideways = move the effect in the chain, up / down = its mix
                drag.move = std::fabs(x - drag.x0) > std::fabs(y - drag.y0) && view().group == "CHAIN";
            }
            if (!drag.moved) return;
            if (drag.move) { drag.target = cellAt(x, y); return; }
            // continuous controls: one encoder detent per 4 pixels; lists: one step per 28 pixels (2 detents)
            const bool list = view().cell[drag.cell].mode == LIST;
            drag.acc += (drag.last - y) / (list ? 14.f : 4.f); drag.last = y;
            const int d = (int) drag.acc;
            if (d) { drag.acc -= (float) d; knob(drag.cell, d); }
            return;
        }
        if (what == 2) {
            if (drag.move && drag.target >= 0 && drag.target != drag.cell) { box::Guard lp(panel.mx); panel.fxMove(drag.cell, drag.target); }
            if (drag.cell >= 0 && !drag.moved && view().group == "CHAIN") { box::Guard lp(panel.mx); panel.fxSel = drag.cell; }   // CHAIN: pick the slot for knob 9
            int pg; { box::Guard lp(panel.mx); pg = panel.page; }
            if (pg == box::Panel::SYNC && drag.cell >= 0 && !drag.moved) { push(drag.cell); drag.down = false; return; }   // a tap changes the setting
            if (pg == box::Panel::ENGINE && drag.cell >= 0 && !drag.moved) {   // CUSTOM: a tap = choose the knob's control
                box::Guard lp(panel.mx); if (panel.customOn()) { panel.startAssign(drag.cell); drag.down = false; return; }
            }
            if (drag.cell >= 0 && !drag.moved) {   // a tap: two on the same control = back to its default
                if (drag.cell == lastTapCell && now - lastTapAt < .4) { box::Guard lp(panel.mx); panel.reset(drag.cell); lastTapCell = -1; }
                else { lastTapCell = drag.cell; lastTapAt = now; }
            }
            drag.down = false;
        }
    }

    // a computer keyboard standing in for the front panel (SDL window): 1-4 parts, page letters, [ ] knob 9, space play
    void key(int c) {
        using P = box::Panel;
        if (c >= '1' && c <= '4') { host.command("p " + std::string(1, (char) c)); return; }
        if (c == ' ') { push(8); return; }
        if (c == 's') { box::Guard l(panel.mx); panel.openSync(); return; }   // TEMPO & SYNC
        if (c == 'o') { tuner = !tuner; return; }                              // waveform / tuner
        if (c == '[' || c == ']') { knob(8, c == ']' ? 1 : -1); return; }
        if (c == 'p') { box::Guard l(panel.mx); panel.keysToggle(); return; }   // the KEYS button
        const char* keys = "evbftmqxi"; const int pages[] = { P::ENGINE, P::VOICE, P::PAGE2, P::FILTER, P::ENV, P::MOD, P::SEQ, P::FX, P::MIXER };
        for (int i = 0; keys[i]; ++i) if (c == keys[i]) { box::Guard l(panel.mx); panel.pagePress(pages[i]); }
    }

    // the name of a new multi: a keyboard on the screen
    bool naming() { box::Guard lp(panel.mx); return panel.naming; }
    static constexpr float KY0 = 262, KH = 76, KG = 8;
    static constexpr const char* KROWS[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM-" };
    struct Key { float x0, y0, x1, y1; int code; };   // code: a character, or 1 space, 2 erase, 3 cancel, 4 save
    std::vector<Key> keys() const {
        std::vector<Key> v; const float kw = 1264.f / 10;
        for (int r = 0; r < 4; ++r) { const std::string row = KROWS[r]; const float off = r >= 2 ? kw * .5f : 0;
            for (size_t c = 0; c < row.size(); ++c) { const float x0 = 8 + off + c * kw; v.push_back({ x0 + 3, KY0 + r * KH, x0 + kw - 3, KY0 + r * KH + KH - KG, row[c] }); } }
        const float y0 = KY0 + 4 * KH;
        v.push_back({ 8 + kw * 8.5f + 3, KY0 + 3 * KH, 1272 - 3, KY0 + 3 * KH + KH - KG, 2 });   // erase, at the end of the last row
        v.push_back({ 8 + 3, y0, 8 + kw * 4 - 3, y0 + KH - KG, 3 });
        v.push_back({ 8 + kw * 4 + 3, y0, 8 + kw * 7 - 3, y0 + KH - KG, 1 });
        v.push_back({ 8 + kw * 7 + 3, y0, 1272 - 3, y0 + KH - KG, 4 });
        return v;
    }
    void keyboard(Canvas& C, const View& V) {
        const Px pc = partCol(V.part); std::string name; { box::Guard lp(panel.mx); name = panel.nameBuf; }
        C.text(bold, 18, 24, 204, "NEW MULTI NAME", TXT2);
        C.rrect(330, 180, 1272, 244, 10, rgb(20, 20, 23)); C.rframe(330, 180, 1272, 244, 10, 2, pc);
        const float w = C.text(bold, 30, 350, 224, name, TXT);
        if (std::fmod(now, 1.0) < .6) C.rect(352 + w, 192, 355 + w, 232, pc);   // the cursor
        for (const auto& k : keys()) {
            const bool act = k.code == 4; C.rrect(k.x0, k.y0, k.x1, k.y1, 8, act ? pc : rgb(26, 26, 30));
            std::string l = k.code == 1 ? "SPACE" : k.code == 2 ? "ERASE" : k.code == 3 ? "CANCEL" : k.code == 4 ? "SAVE" : std::string(1, (char) k.code);
            C.text(bold, k.code > 4 ? 26 : 19, (k.x0 + k.x1) / 2, (k.y0 + k.y1) / 2 + (k.code > 4 ? 10 : 7), l, act ? BG : TXT, 1);
        }
    }
    void keyTouch(float x, float y) {
        for (const auto& k : keys()) if (x >= k.x0 && x < k.x1 && y >= k.y0 && y < k.y1) {
            box::Guard lp(panel.mx);
            if (k.code == 1) panel.nameKey(' '); else if (k.code == 2) panel.nameBack(); else if (k.code == 3) panel.naming = false; else if (k.code == 4) panel.nameOk(); else panel.nameKey((char) k.code);
        }
    }
    void presetTouch(float x, float y) {
        if (naming()) { keyTouch(x, y); return; }
        const View V = view(); const int n = (int) listOf(V).size();
        if (V.sounds) {   // factory sounds: a tap picks, a second tap (or LOAD) loads
            box::Guard lp(panel.mx); const int part = box.selected.load();
            if (x >= BX) { if (y >= BTN[0] && y < BTN[0] + BH && n) panel.loadSound(part, V.pick); return; }
            if (x > 836 || y < 186) return;
            const int i = firstRow(V.pick, n) + (int) ((y - 186) / 52); if (i < 0 || i >= n) return;
            if (i == V.pick) panel.loadSound(part, i); else panel.soundPick = i;
            return;
        }
        if (x >= BX) {   // CHARGER, ENREGISTRER NOUVEAU, REMPLACER LA SÉLECTION
            for (int q = 0; q < 3; ++q) if (y >= BTN[q] && y < BTN[q] + BH) {
                box::Guard lp(panel.mx);
                if (q == 0) panel.push(0); else if (q == 1) panel.startName(); else if (n) panel.saveOver();
            }
            return;
        }
        if (x > 836 || y < 186) return;
        const int i = firstRow(V.pick, n) + (int) ((y - 186) / 52); if (i < 0 || i >= n) return;
        if (i == V.pick) { push(0); return; }
        box::Guard lp(panel.mx); panel.presetPick = i;
    }

private:
    std::string msg, presetName; double msgAt = -10;
};

} // namespace ui
