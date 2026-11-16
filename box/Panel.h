// The front panel (Octavio Facade, RP2040 over USB-MIDI): what the knobs, buttons and LED rings do.
// Protocol: facade-fw/PROTOCOLE.md. Runs on its own thread; it changes the synth through PanelHost
// (which serialises with the terminal) and only reads the Box's atomics.
//
//   PART 1-4      select the part (its LED is lit in the part's colour)
//   page buttons  ENGINE   engine, volume, pan, MIDI channel, tempo; pressed again: CUSTOM, 8 knobs the player fills with
//                          any of the engine's controls (per part and engine, kept in the multi)
//                 VOICE    the plugin's main page, section by section in the plugin's order (PluginPages.h, made from
//                          the plugins by tools/plugin_pages.py), 8 controls at a time
//                 PAGE 2   the plugin's own extra page (SPECTRA, PATCH MATRIX, PATTERN), else the rest of VOICE. The VC3's
//                          PATCH MATRIX is a pin board on the touch screen (tap a hole: a pin; tap the pin: out), knob 1
//                          the row (source), knob 2 the column (destination), push = pin in / out, knob 3 white / green pin
//                          The SAMPLER's PAGE 2 is PADS: the 4 x 4 pads of a bank on the touch screen (tap = play and pick),
//                          the banks A-D, the pad's sample, loaded from the SD card (or a USB stick) with a file browser
//                 FILTER   the engine's filter controls        ENV   its envelopes (both taken from the plugin's pages)
//                 MOD      one page per LFO (1-4): rate, shape, where it goes, how much; the knob under RATE turns it
//                          into a sample & hold (RANDOM shape)
//                 SEQ      one page per step sequencer (1-4): knob 1 = destination, knob 2 = amount; the 16 steps are
//                          drawn on the touch screen
//                 FX       CHAIN: the 8 effects with their mix (push = that effect's page; drag on the screen to reorder),
//                          then one page per effect slot: the effect, its model, its controls as in the FX plugin
//                          SIDECHAIN is one of the effects: the part ducks on another part (its sound, its notes, one of
//                          its drum tracks / pads 1-8), an LFO or a 16-step sequence; attack, decay, amount
//                 MIXER    the 4 parts' volumes (knobs 1-4, push = mute) and pans (5-8); then (MIXER pressed again, or
//                          the tabs on the screen) the presets, in two: PRESETS · <ENGINE>, the engine's factory sounds
//                          (sounds/factory.txt, 5 per engine) for the part chosen, and PRESETS · ALL TRACKS, the multis
//                          (the 4 parts): knob 1 picks, push loads; push knob 2 saves a new multi. No button of its own.
//                 KEYS     (a little keyboard on the cap, where PRESET was) the 9 other page buttons become a keyboard
//                          for the part chosen: the QUANTIZER's key and scale when it has one, else C major; knob 9 turns
//                          the octave up or down. Pressed again, the buttons go back to their pages.
//   TEMPO & SYNC  (no button: a tap on the tempo at the top of the screen) tempo, clock source (internal, MIDI, USB,
//                 Bluetooth), start / stop received, tap tempo, clock and start / stop sent, MIDI thru, a multi's own tempo;
//                 a page button leaves it
//   knob 9        moves along a page by 8 controls; push = play / stop
//   a page button pressed again: the page's next tab (next LFO, sequencer, effect, DR8 track)
//   push knob 1-8 back to the default value (a level with an on / off switch: push = on / off)
//   on / off switches and two-way choices (SAW on, SYNC, SUB OCTAVE -1 / -2...) take no knob: they are buttons along the
//   bottom of the touch screen, with the controls of their section (MAX_SW per group)
//   volume pot    master volume
#pragma once
#include "Box.h"
#include "PluginPages.h"
#include "FactorySounds.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace box {

// the lock shared by the terminal, the panel and the screen; the browser build has a single thread and no locks
#if defined(__wasi__) && !defined(_REENTRANT)
struct Lock { void lock() {} void unlock() {} };
#else
using Lock = std::recursive_mutex;
#endif
struct Guard { Lock& m; explicit Guard(Lock& x) : m(x) { m.lock(); } ~Guard() { m.unlock(); } Guard(const Guard&) = delete; };

struct PanelHost {
    virtual ~PanelHost() = default;
    virtual void send(int t, Msg m) = 0;                 // to the audio thread, for part t
    virtual void command(const std::string& line) = 0;   // a terminal command (load, save...)
};

// the box's own engines' pages (the DFAM), in the same form as the plugins' (PluginPages.h); "t@": the track chosen
static const layout::Section BOX_SECTIONS[] = {
    { "dfam", "VOICE", "TRACK", "@pad t@lvl t@pan t@tune t@vel t@mute", "||||" },
    { "dfam", "VOICE", "MIXER", "t@l1 t@l2 t@noise", "||" },
    { "dfam", "VOICE", "OSCILLATORS", "t@f1 t@w1 t@f2 t@w2 t@fm t@sync t@veg t@vdec", "VCO 1|VCO 1|VCO 2|VCO 2|VCO 2|VCO 2|VCO EG|VCO EG" },
    { "dfam", "VOICE", "FILTER", "t@cut t@res t@fmode t@nmod t@feg t@fdec", "VCF|VCF|VCF|VCF|VCF EG|VCF EG" },
    { "dfam", "VOICE", "VCA", "t@vatt t@adec", "|" },
};

// pages laid out by hand, engine by engine (the rest come from the plugins' pages): a group of 8 knobs ('.' none), its
// switches (buttons along the bottom of the screen), its sliders (on the screen's bottom row) and its kind
struct PageDef { const char* eng; int page; const char* tab; const char* title; const char* kind; const char* knobs; const char* sws; const char* bars; };
enum { PD_VOICE = 1, PD_PAGE2 = 2, PD_FILTER = 3, PD_ENV = 4 };
static const PageDef PAGE_DEFS[] = {
    // VC3: PAGE 2 = the patch matrix and a big joystick pad; FILTER and ENV with their matrix outputs as buttons
    { "vc3", PD_VOICE, "OSC 1-2", "OSCILLATORS 1 · 2", "", "o1f o1sh o1sin o1saw o2f o2sh o2sq o2tri", "", "" },
    { "vc3", PD_VOICE, "OSC 3", "OSCILLATOR 3", "", "o3f o3sh o3sq o3tri tune", "", "" },
    { "vc3", PD_VOICE, "OUTPUT", "REVERB · OUTPUT", "", "rmix rlev out1 out2 pan1 pan2 tone", "", "" },
    { "vc3", PD_PAGE2, "MATRIX", "PATCH MATRIX", "matrix", "", "", "" },
    { "vc3", PD_PAGE2, "JOYSTICK", "JOYSTICK", "xy", "jx jy jrx jry . . jx jy", "", "" },
    { "vc3", PD_FILTER, "", "NOISE · RING · FILTER", "", "ncol nlvl rlvl ff fres flvl", "pin8_0 pin8_1 pin8_2 pin8_3 pin8_4 pin8_5 pin8_6 pin8_7 pin8_8 pin8_9 pin8_10 pin8_11 pin8_12 pin8_13", "" },
    { "vc3", PD_ENV, "", "TRAPEZOID · ENVELOPE SHAPER", "", "ea eon edec eoff etrap esig trig", "pin10_0 pin10_1 pin10_2 pin10_3 pin10_4 pin10_5 pin10_6 pin10_7 pin10_8 pin10_9 pin10_10 pin10_11 pin10_12 pin10_13", "" },
    // VACT: the spectral processor's 16 bands as sliders; one lopass gate per page; quadrature A-B, then C-D
    { "vact", PD_PAGE2, "SHIFTER", "FREQUENCY SHIFTER", "", "fssrc fsshift fsfine fsblend", "", "" },
    { "vact", PD_PAGE2, "SPECTRAL", "SPECTRAL PROCESSOR", "bars", "spfreq spwidth spmix spmode", "", "sp0 sp1 sp2 sp3 sp4 sp5 sp6 sp7 sp8 sp9 sp10 sp11 sp12 sp13 sp14 sp15" },
    { "vact", PD_PAGE2, "SPECTRAL 2", "SPECTRAL PROCESSOR 2", "", "sppreeven sppreodd spout spstereo spresp spevenin", "sptrans sptreo", "" },
    { "vact", PD_FILTER, "A", "LOPASS GATE A", "", "g0src g0mode g0cv g0off g0lvl g0amt g0pan", "", "" },
    { "vact", PD_FILTER, "B", "LOPASS GATE B", "", "g1src g1mode g1cv g1off g1lvl g1amt g1pan", "", "" },
    { "vact", PD_FILTER, "C", "LOPASS GATE C", "", "g2src g2mode g2cv g2off g2lvl g2amt g2pan", "", "" },
    { "vact", PD_FILTER, "D", "LOPASS GATE D", "", "g3src g3mode g3cv g3off g3lvl g3amt g3pan", "", "" },
    { "vact", PD_ENV, "A-B", "FUNCTION GENERATORS A · B", "", "f0att f0dec f0mode f0trig f1att f1dec f1mode f1trig", "quadab", "" },
    { "vact", PD_ENV, "C-D", "FUNCTION GENERATORS C · D", "", "f2att f2dec f2mode f2trig f3att f3dec f3mode f3trig", "quadcd", "" },
    // EASEL: the complex oscillator then the modulation oscillator on VOICE, the rest on PAGE 2 (the sequential voltage
    // source as sliders, the pulser's knobs above them); one lopass gate per page
    { "easel", PD_VOICE, "COMPLEX", "COMPLEX OSCILLATOR", "", "ppitch pfine pcva timbre tcva pwave pcv tcv", "pkbd", "" },
    { "easel", PD_VOICE, "MOD OSC", "MODULATION OSCILLATOR", "", "mfreq mcva modidx ixcva mwave mcv modmode ixcv", "mrange mkbd", "" },
    { "easel", PD_PAGE2, "SVS", "SEQUENTIAL VOLTAGE SOURCE · PULSER", "bars", "vstages vadv prate pmode", "", "v0 v1 v2 v3 v4" },
    { "easel", PD_PAGE2, "OUTPUT", "PULSER · OUTPUT · KEYBOARD 218", "", "pucva pucv rev revtime tune porta", "arp", "" },
    { "easel", PD_FILTER, "GATE 1", "LOPASS GATE 1", "", "g0lvl g0amt g0out g0pan g0mode g0cv", "", "" },
    { "easel", PD_FILTER, "GATE 2", "LOPASS GATE 2", "", "g1lvl g1amt g1out g1pan g1mode g1cv g1src", "", "" },
    // PRO70: the filter envelope and the amp envelope on their own pages
    { "pro70", PD_ENV, "FILTER", "FILTER ENVELOPE", "", "fatt fdec fsus frel fenvamt pmfenv", "", "" },
    { "pro70", PD_ENV, "AMP", "AMP ENVELOPE", "", "aatt adec asus arel", "release", "" },
    // J8: no arpeggio and no chorus (the box's arpeggiator, the FX chorus); ENV-1 and ENV-2 on their own pages
    { "j8", PD_PAGE2, "", "ASSIGN", "", "assign udet glide tune trig", "", "" },
    { "j8", PD_ENV, "ENV-1", "ENV-1", "", "e1a e1d e1s e1r e1kf venv", "e1pol envsel", "" },
    { "j8", PD_ENV, "ENV-2", "ENV-2", "", "e2a e2d e2s e2r e2kf menv", "", "" },
    // CS8: the levels together on VOICE's third page; PAGE 2 = sub-osc, ring, ribbon (with a ribbon to play), performance;
    // ENV: channel I's envelopes, its IL / AL, channel II's, its IL / AL (ring attack and decay on knobs 7-8)
    { "cs8", PD_VOICE, "CHANNEL I", "CHANNEL I", "", "afeet apw apwm asq asaw asine anoise abrill", "", "" },
    { "cs8", PD_VOICE, "CHANNEL II", "CHANNEL II", "", "bfeet bpw bpwm bsq bsaw bsine bnoise bbrill", "", "" },
    { "cs8", PD_VOICE, "LEVELS", "LEVELS", "", "alvl blvl", "", "" },
    { "cs8", PD_PAGE2, "SUB-OSC", "SUB-OSC", "", "sospd sofn sovco sovcf sovca", "", "" },
    { "cs8", PD_PAGE2, "RING", "RING MODULATOR", "", "rmspd rmdep rmatt rmdec", "", "" },
    { "cs8", PD_PAGE2, "RIBBON", "RIBBON CONTROLLER", "ribbon", "ribrng @ribdst tibr tilvl", "", "" },
    { "cs8", PD_PAGE2, "PLAY", "PERFORMANCE", "", "det pwmspd porta tune trig", "", "" },
    { "cs8", PD_ENV, "I", "CHANNEL I · ENVELOPES", "", "afa afd afr aaa aad aas aar", "", "" },
    { "cs8", PD_ENV, "IL / AL I", "CHANNEL I · IL / AL", "", "ail aal . . . . rmatt rmdec", "", "" },
    { "cs8", PD_ENV, "II", "CHANNEL II · ENVELOPES", "", "bfa bfd bfr baa bad bas bar", "", "" },
    { "cs8", PD_ENV, "IL / AL II", "CHANNEL II · IL / AL", "", "bil bal . . . . rmatt rmdec", "", "" },
    // DR8: ENV for the track picked in the tab row: its decays, and the box's envelope on the whole hit
    { "dr8", PD_ENV, "", "ENVELOPE", "", "t@dec t@ndec t@att t@hold t@rel t@vsens t@snap t@tone", "", "" },
    // DFAM: FILTER and ENV for the track picked in the tab row
    // DX7: VOICE = the algorithm and the 6 operators (a tab each), PAGE 2 = LFO and pitch envelope, FILTER = the
    // operators' levels side by side (the timbre: there is no filter), ENV = each operator's envelope
    { "dx7", PD_VOICE, "ALGORITHM", "ALGORITHM", "", "alg fb bright lvl tune porta", "mono", "" },
    { "dx7", PD_VOICE, "OP 1", "OPERATOR 1", "", "o1coarse o1fine o1det o1lvl o1vel o1rs o1kls o1ams", "o1on", "" },
    { "dx7", PD_VOICE, "OP 2", "OPERATOR 2", "", "o2coarse o2fine o2det o2lvl o2vel o2rs o2kls o2ams", "o2on", "" },
    { "dx7", PD_VOICE, "OP 3", "OPERATOR 3", "", "o3coarse o3fine o3det o3lvl o3vel o3rs o3kls o3ams", "o3on", "" },
    { "dx7", PD_VOICE, "OP 4", "OPERATOR 4", "", "o4coarse o4fine o4det o4lvl o4vel o4rs o4kls o4ams", "o4on", "" },
    { "dx7", PD_VOICE, "OP 5", "OPERATOR 5", "", "o5coarse o5fine o5det o5lvl o5vel o5rs o5kls o5ams", "o5on", "" },
    { "dx7", PD_VOICE, "OP 6", "OPERATOR 6", "", "o6coarse o6fine o6det o6lvl o6vel o6rs o6kls o6ams", "o6on", "" },
    { "dx7", PD_PAGE2, "LFO", "LFO", "", "lspd ldel lpmd lamd lwave pms", "lsync", "" },
    { "dx7", PD_PAGE2, "PITCH EG", "PITCH EG", "", "pr1 pr2 pr3 pr4 pl1 pl2 pl3 pl4", "", "" },
    { "dx7", PD_FILTER, "LEVELS", "OPERATOR LEVELS", "", "o1lvl o2lvl o3lvl o4lvl o5lvl o6lvl fb bright", "", "" },
    { "dx7", PD_ENV, "OP 1", "OP 1 ENVELOPE", "", "o1r1 o1r2 o1r3 o1r4 o1l1 o1l2 o1l3 o1l4", "", "" },
    { "dx7", PD_ENV, "OP 2", "OP 2 ENVELOPE", "", "o2r1 o2r2 o2r3 o2r4 o2l1 o2l2 o2l3 o2l4", "", "" },
    { "dx7", PD_ENV, "OP 3", "OP 3 ENVELOPE", "", "o3r1 o3r2 o3r3 o3r4 o3l1 o3l2 o3l3 o3l4", "", "" },
    { "dx7", PD_ENV, "OP 4", "OP 4 ENVELOPE", "", "o4r1 o4r2 o4r3 o4r4 o4l1 o4l2 o4l3 o4l4", "", "" },
    { "dx7", PD_ENV, "OP 5", "OP 5 ENVELOPE", "", "o5r1 o5r2 o5r3 o5r4 o5l1 o5l2 o5l3 o5l4", "", "" },
    // LINE IN: VOICE = the input (gain, mode, width, balance, drive, level), PAGE 2 = the gate, FILTER, ENV = the NOTE gate's ADSR
    { "line", PD_VOICE, "", "INPUT", "", "gain mode width bal drive lvl", "inv", "" },
    { "line", PD_PAGE2, "", "GATE", "", "gmode thr att rel", "", "" },
    { "line", PD_FILTER, "", "FILTER", "", "ftype cut res fenv locut", "", "" },
    { "line", PD_ENV, "", "GATE ENVELOPE", "", "att dec sus rel vsens", "", "" },
    { "dx7", PD_ENV, "OP 6", "OP 6 ENVELOPE", "", "o6r1 o6r2 o6r3 o6r4 o6l1 o6l2 o6l3 o6l4", "", "" },
    { "dfam", PD_FILTER, "", "VCF · VCF EG", "", "t@cut t@res t@nmod t@feg t@fdec", "t@fmode", "" },
    { "dfam", PD_ENV, "", "VCO EG · VCF EG · VCA", "", "t@veg t@vdec t@feg t@fdec t@adec t@vel", "t@vatt", "" },
    // SAMPLER: VOICE = the pads; PAGE 2 = the pad's sample (level, start, end, tune over its waveform), then the rest
    { "sp64", PD_VOICE, "", "PADS", "pads", "", "", "" },
    { "sp64", PD_PAGE2, "SAMPLE", "SAMPLE", "wave", "k@lvl k@st k@en k@tune", "k@rev", "" },
    { "sp64", PD_PAGE2, "PAD", "PAD", "", "k@src k@pan k@vel k@dec k@mode", "", "" },
    { "sp64", PD_PAGE2, "GLOBAL", "GLOBAL", "", "tune gcut gdec chrom", "", "" },
};

struct Panel {
    enum Page { ENGINE, VOICE, PAGE2, FILTER, ENV, MOD, SEQ, FX, MIXER, PRESET, SYNC, NPAGE };   // SYNC has no button
    static constexpr int NBUTTON = 10;
    static constexpr int NSEQ = 4, NLFO = 4;   // the panel shows 4 of the engines' 6 sequencers, their 4 LFOs
    Box& box; PanelHost& host; std::function<void(const std::vector<uint8_t>&)> out;
    std::string presetDir;
    Lock& mx;            // shared with the terminal and the screen: one lock, so no lock-order trouble
    int page = VOICE, presetPick = 0, pad[NT] {};   // pad: the SAMPLER pad being edited
    int wins[NT][NPAGE] {}, winEngine[NT] { -1, -1, -1, -1 };   // where each part is on each page (back to the start on a new engine)
    int lfoShape[NT][NLFO] {};                       // the LFO shape to go back to when a sample & hold is turned back
    bool connected = false;
    int fxSel = -1;                      // FX CHAIN: the slot touched on the screen; knob 9 then changes its effect
    float acc[9] {};                     // fractional steps for list controls (knobs 1-9)
    int prevPage = VOICE;                // the page TEMPO & SYNC was opened from
    int presetKind = 1;                  // PRESETS: 0 the engine's factory sounds, 1 the multis (ALL TRACKS)
    int soundPick = 0;                   // the factory sound picked
    static constexpr int KEYS_BTN = 9;   // the 10th page button: the keyboard on / off (PRESET is a tab of MIXER)
    bool keys = false;                   // the page buttons play notes
    int keysOct = 4;                     // the octave of the first key (4: from middle C)
    int keyHeld[9] { -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // the note each button is holding, and on which part (x 128)
    int syncTab = 0;                     // its tabs: 0 TEMPO & SYNC, 1 QUANTIZER
    int custom[NT][NENG][8];             // ENGINE > CUSTOM: the control on each knob (-1: none), per part and engine
    int assign = -1, assignPick = 0;     // CUSTOM: the knob being given a control, and the control picked in the list
    std::vector<double> taps;            // tap tempo
    std::function<double()> clock = [] { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); };

    struct RingOut { int v = -1, mode = -1; uint8_t r = 0, g = 0, b = 0; };
    RingOut sentRing[8];
    int sentLed[14][3];

    Panel(Box& b, PanelHost& h, std::function<void(const std::vector<uint8_t>&)> o, std::string dir, Lock& lock)
        : box(b), host(h), out(std::move(o)), presetDir(std::move(dir)), mx(lock) { forget(); for (auto& a : custom) for (auto& b2 : a) std::fill(b2, b2 + 8, -1); }

    static constexpr uint8_t COL[NT][3] = { { 127, 45, 0 }, { 0, 95, 127 }, { 120, 0, 95 }, { 45, 127, 0 } };   // orange, blue, pink, green

    void forget() { for (auto& r : sentRing) r = RingOut(); for (auto& l : sentLed) l[0] = l[1] = l[2] = -1; }
    void hello() { out({ 0xF0, 0x7D, 0x4F, 0x01, 0xF7 }); }
    int& W(int pg) {
        const int t = box.selected.load(), e = box.tr[(size_t) t].e.load();
        if (winEngine[t] != e) { winEngine[t] = e; std::fill(wins[t], wins[t] + NPAGE, 0); }
        return wins[t][pg];
    }

    // SEQ 5 and 6 and the modulation matrix are not on the box (the LFOs use the matrix through link()): kept out of the
    // lists, and the box keeps SEQ 5 and 6 silent (Box::muteHidden)
    static bool hiddenMod(const std::string& id) {
        if (id.rfind("s4", 0) == 0 || id.rfind("s5", 0) == 0) return true;
        return id.size() >= 4 && id[0] == 'm' && std::isdigit((unsigned char) id[1]) && (id.substr(2) == "src" || id.substr(2) == "dst" || id.substr(2) == "amt");
    }

    // ---------------- the engines, in families: mono synths, poly synths, then the drums and the sampler ----------------
    static const std::vector<int>& order() {
        static std::vector<int> v; if (!v.empty()) return v;
        for (const char* id : { "mini", "a26", "m20", "vc3", "vact", "easel", "pro70", "j8", "cs8", "dx7", "dfam", "dr8", "sp64", "line" }) { const int e = engineIndex(id); if (e >= 0) v.push_back(e); }
        for (int e = 0; e < NENG; ++e) if (std::find(v.begin(), v.end(), e) == v.end()) v.push_back(e);   // any other at the end
        return v;
    }
    static int orderPos(int e) { const auto& v = order(); for (size_t q = 0; q < v.size(); ++q) if (v[q] == e) return (int) q; return 0; }
    static const char* family(int e) {
        const std::string id = ENGS[e].id;
        if (id == "pro70" || id == "j8" || id == "cs8" || id == "dx7") return "POLY";
        if (id == "dr8" || id == "dfam" || id == "sp64") return "RHYTHM";
        if (id == "line") return "INPUT";
        return "MONO";
    }

    // ---------------- names ----------------
    static std::string up(const char* s) { std::string x = s; for (auto& c : x) c = (char) std::toupper((unsigned char) c); return x; }
    // the plugin's name for a control (longer than the engine's short one), or the short one
    std::string label(int t, int i) { return labelOf(box.tr[(size_t) t].e.load(), i); }
    std::string labelOf(int e, int i) {
        auto& T = box.tr[0]; const auto& map = T.ctl[(size_t) e];
        const PDef& d = T.eng[(size_t) e]->D[i]; const std::string id = d.id;
        if (id.size() > 1 && id[0] == 'k' && std::isdigit((unsigned char) id[1])) return d.name;   // SAMPLER pad controls: the pad is on the PAD knob
        for (size_t k = 0; k < map.size(); ++k) if (map[k] == i && ENGS[e].names && ENGS[e].names[k]) return ENGS[e].names[k];
        int a = -1, b = -1; char w[16] {};
        if (std::sscanf(id.c_str(), "l%d%15s", &a, w) == 2) return std::string(w) == "rate" ? "RATE" : "SHAPE";
        if (std::sscanf(id.c_str(), "m%d%15s", &a, w) == 2) return "MOD " + std::to_string(a + 1) + " " + up(w[0] == 's' ? "source" : w[0] == 'd' ? "dest" : "amount");
        if (std::sscanf(id.c_str(), "s%d_%d", &a, &b) == 2) return "STEP " + std::to_string(b + 1);
        if (std::sscanf(id.c_str(), "s%d%15s", &a, w) == 2) return w[0] == 'd' ? "DESTINATION" : "AMOUNT";
        if (std::sscanf(id.c_str(), "p%d_%d", &a, &b) == 2) return "T" + std::to_string(a + 1) + " · PAS " + std::to_string(b + 1);
        if (id == "ffbon") return "FILTER BANK";
        if (id.rfind("ffb", 0) == 0) return std::string("BANK ") + d.name;
        if (std::sscanf(id.c_str(), "sp%d", &a) == 1 && id.size() <= 4) return "SPECTRA " + std::to_string(a + 1);
        if (id.rfind("sp", 0) == 0) return std::string("SPECTRA ") + d.name;
        return d.name;
    }
    static std::string choice(const char* list, int k) {
        std::string c = list ? list : ""; size_t p = 0;
        while (k-- > 0 && p != std::string::npos) { p = c.find('|', p); if (p != std::string::npos) ++p; }
        if (p == std::string::npos) return "?";
        return c.substr(p, c.find('|', p) - p);
    }

    // ---------------- on/off switches that ride on a level ----------------
    // the knob sets the level and its push switches it (the MINI's mixer: OSC 1 on + OSC 1 level, glide on + glide time;
    // a DR8 track's mute + its level). The switch then has no knob of its own.
    std::array<std::vector<int>, NENG> sw; std::array<std::vector<char>, NENG> paired;
    void pairs(int e) {
        if (!sw[(size_t) e].empty()) return;
        auto& E = *box.tr[0].eng[(size_t) e];
        sw[(size_t) e].assign((size_t) E.np, -1); paired[(size_t) e].assign((size_t) E.np, 0);
        auto ends = [](const std::string& s, const char* x) { const size_t n = std::strlen(x); return s.size() > n && s.compare(s.size() - n, n, x) == 0; };
        for (int j = 0; j < E.np; ++j) {
            if (E.D[j].type != 'b') continue;
            const std::string s = E.D[j].id;
            const std::string base = ends(s, "mute") ? s.substr(0, s.size() - 4) : ends(s, "on") ? s.substr(0, s.size() - 2) : "";
            if (base.empty()) continue;
            for (int i = 0; i < E.np; ++i) {
                if (E.D[i].type == 'b' || E.D[i].type == 'c') continue;
                const std::string id = E.D[i].id;
                if (id == base + "lvl" || id == base) { sw[(size_t) e][(size_t) i] = j; paired[(size_t) e][(size_t) j] = 1; break; }
            }
        }
    }
    int switchOf(int e, int i) { pairs(e); return i >= 0 && i < (int) sw[(size_t) e].size() ? sw[(size_t) e][(size_t) i] : -1; }
    static bool isMute(const PDef& d) { const std::string s = d.id; return s.size() > 4 && s.compare(s.size() - 4, 4, "mute") == 0; }
    bool levelOff(int t, int i) {   // the sound of level i is switched off (its switch off, or its mute on)
        auto& T = box.tr[(size_t) t]; const int e = T.e.load(); const int s = switchOf(e, i); if (s < 0) return false;
        const bool on = T.shown[(size_t) e][(size_t) s].load() > .5f;
        return isMute(T.eng[(size_t) e]->D[s]) ? on : !on;
    }

    // ---------------- knobs that are not a parameter of their own ----------------
    // PAD: the SAMPLER pad being edited. LFO n: its destination and amount (the first matrix link from that LFO; turning
    // them takes a free link when there is none) and LFO / S&H (S&H = the RANDOM shape).
    static constexpr int PADSEL = -2;
    int padCount(int t) { return eng(t).id("t0vdec") >= 0 || eng(t).id("t0kit") >= 0 ? 8 : 64; }   // the DFAM's tracks, or the SAMPLER's pads
    static constexpr int MAX_SW = 16;
    static bool isSwitch(const PDef& d) { return d.type == 'b' || (d.type == 'c' && d.mx - d.mn == 1); }   // a button on the screen
    void flip(int t, int i) {   // a switch tapped: on / off, the other of two, or the next of a patch pin's three
        auto& E = eng(t); if (i < 0 || i >= E.np) return; const float v = val(t, i); const auto& d = E.D[i];
        if (d.type == 'c' && d.mx - d.mn > 1) { setp(t, i, std::fmod(std::round(v - d.mn) + 1, d.mx - d.mn + 1) + d.mn); return; }
        setp(t, i, v > d.mn + .5f ? d.mn : d.mx);
    }
    static int bmId(int f) { return -20000 - f; }   // a setting of the box's LFOs / sequencers 5-8 (Track::bm)
    static bool isBm(int id) { return id <= -20000 && id > -20000 - Track::BM_N; }
    float bmVal(int t, int f) { return box.tr[(size_t) t].bm[(size_t) f].load(); }
    void bmSet(int t, int f, float v) { Msg m; m.k = BOXMOD; m.i = f; m.v = v; host.send(t, m); }
    std::vector<std::string> dstNames(int t) {   // the engine's modulation destinations (its matrix's), "---" first
        std::vector<std::string> v; auto& E = eng(t); const int m = E.id("m0dst"); if (m < 0) return { "---" };
        for (int k = 0; k <= (int) E.D[m].mx; ++k) { std::string n = up(choice(E.D[m].choices, k).c_str()); if (n.empty()) { const int pi = E.DSTP ? E.DSTP[k] : -1; n = k == 0 || pi < 0 ? "---" : label(t, pi); } v.push_back(n); }
        return v;
    }
    static int lfoDst(int n) { return -100 - n; }
    static int lfoAmt(int n) { return -110 - n; }
    static int lfoMode(int n) { return -120 - n; }
    static bool isVirtual(int id) { return id <= -2; }
    // the VC3's pin matrix: a cursor (row = source, column = destination) and the pin in hand (1 white, 2 green)
    static constexpr int MROW = -130, MCOL = -131, MPIN = -132;
    // the CS8's ribbon: what it plays (0: the pitch, as the real one; else one of the engine's modulation destinations)
    static constexpr int RIBDST = -140;
    int ribDst[NT] {};
    std::vector<std::pair<std::string, int>> ribTargets(int t) {   // name, control (-1: the pitch)
        std::vector<std::pair<std::string, int>> v { { "PITCH", -1 } }; auto& E = eng(t); const int m = E.id("m0dst"); if (m < 0) return v;
        const auto& d = E.D[m];
        for (int k = 1; k <= (int) d.mx; ++k) { const int pi = E.DSTP ? E.DSTP[k] : -1; if (pi >= 0) { std::string n = up(choice(d.choices, k).c_str()); if (n.empty()) n = label(t, pi); v.push_back({ n, pi }); } }
        return v;
    }
    void ribbon(int t, float x, bool down) {   // the finger on the ribbon: x 0-1; let go: the pitch back to the middle
        const auto v = ribTargets(t); const int k = std::clamp(ribDst[t], 0, (int) v.size() - 1);
        if (v[(size_t) k].second < 0) { const int b = down ? std::clamp((int) (x * 16383), 0, 16383) : 8192; Msg m; m.k = MIDI; m.m[0] = 0xE0 | (uint8_t) box.chan[(size_t) t].load(); m.m[1] = (uint8_t) (b & 127); m.m[2] = (uint8_t) (b >> 7); host.send(t, m); return; }
        if (!down) return; const int i = v[(size_t) k].second; const auto& d = eng(t).D[i]; setp(t, i, d.mn + x * (d.mx - d.mn));
    }
    int mRow = 0, mCol = 0, mPin = 1;
    int pinId(int t, int r, int c) { return eng(t).id("pin" + std::to_string(r) + "_" + std::to_string(c)); }
    int pinRows(int t) { int r = 0; while (pinId(t, r, 0) >= 0) ++r; return r; }
    int pinCols(int t) { int c = 0; while (pinId(t, 0, c) >= 0) ++c; return c; }
    std::string pinSide(int t, int r, int c, bool dst) {   // "OSC 1 SINE > FILTER IN": the source or the destination
        const int i = pinId(t, r, c); if (i < 0) return ""; const std::string n = eng(t).D[i].name; const size_t p = n.find(" > ");
        if (p == std::string::npos) return n;
        std::string x = dst ? n.substr(p + 3) : n.substr(0, p); while (!x.empty() && x.back() == ' ') x.pop_back(); return x;
    }
    void pinTap(int t, int r, int c) {   // a hole: the pin in hand goes in; that pin: it comes out; the other colour: swapped
        const int i = pinId(t, r, c); if (i < 0) return; mRow = r; mCol = c;
        setp(t, i, (int) std::lround(val(t, i)) == mPin ? 0.f : (float) mPin);
    }
    struct VInfo { std::string name, value; float norm = 0; int mode = 0; bool off = false; };   // mode: 0 arc, 1 dot, 2 bipolar
    oct::EngineBase& eng(int t) { return box.engine(t); }
    float val(int t, int i) { auto& T = box.tr[(size_t) t]; return T.shown[(size_t) T.e.load()][(size_t) i].load(); }
    void setp(int t, int i, float v) { auto& E = eng(t); if (i < 0) return; Msg m; m.k = PARAM; m.i = i; m.v = std::clamp(v, E.D[i].mn, E.D[i].mx); host.send(t, m); }
    int lfoSource(int t, int n) {   // the matrix source number of LFO n (its place in the SOURCE list)
        auto& E = eng(t); const int s = E.id("m0src"); if (s < 0) return -1;
        const std::string want = "LFO " + std::to_string(n + 1);
        for (int k = 0; k <= (int) E.D[s].mx; ++k) if (choice(E.D[s].choices, k) == want) return k;
        return -1;
    }
    int link(int t, int n, bool take) {   // the matrix link LFO n drives (-1: none; take: claim a free one)
        auto& E = eng(t); const int src = lfoSource(t, n); if (src < 0) return -1;
        for (int k = 0; k < 8; ++k) { const int s = E.id("m" + std::to_string(k) + "src"); if (s >= 0 && (int) std::lround(val(t, s)) == src) return k; }
        if (!take) return -1;
        for (int k = 0; k < 8; ++k) {
            const int s = E.id("m" + std::to_string(k) + "src"), d = E.id("m" + std::to_string(k) + "dst");
            if (s >= 0 && d >= 0 && (int) std::lround(val(t, s)) == 0 && (int) std::lround(val(t, d)) == 0) { setp(t, s, (float) src); box.tr[(size_t) t].shown[(size_t) box.tr[(size_t) t].e.load()][(size_t) s].store((float) src); return k; }
        }
        return -1;
    }
    static VInfo fxInfo(const ::fxp::PDef& d, float v) {
        VInfo o; o.name = d.name; o.norm = d.mx > d.mn ? (v - d.mn) / (d.mx - d.mn) : 0;
        char b[32];
        if (d.type == 'c') { o.value = choice(d.choices, (int) std::lround(v - d.mn)); o.mode = 1; }
        else if (d.type == 'b') { o.value = v > .5f ? "ON" : "OFF"; o.mode = 1; o.off = v <= .5f; }
        else {
            o.mode = d.mn < 0 && d.mx > 0 ? 2 : 0; const float span = d.mx - d.mn;
            if (d.mn == 0 && d.mx == 1) std::snprintf(b, sizeof b, "%d", (int) std::lround(v * 100));
            else if (span >= 100) std::snprintf(b, sizeof b, "%.0f", v); else if (span >= 10) std::snprintf(b, sizeof b, "%.1f", v); else std::snprintf(b, sizeof b, "%.2f", v);
            o.value = b;
        }
        return o;
    }
    void fxSet(int t, int i, float v) { const auto& d = ::fxp::PDEFS[i]; Msg m; m.k = FXPARAM; m.i = i; m.v = std::clamp(v, d.mn, d.mx); host.send(t, m); }
    bool vinfo(int t, int id, VInfo& o) {
        auto& E = eng(t);
        if (isFxMix(id)) {
            const int q = -9000 - id, f = (int) std::lround(fxVal(t, box.slotParam[q])) - 1, mi = mixOf(t, q);
            o.name = std::to_string(q + 1) + " · " + (f >= 0 ? choice(::fxp::PDEFS[box.slotParam[0]].choices, f + 1) : "EMPTY");
            if (mi < 0) { o.value = "—"; o.off = true; o.mode = 0; return true; }
            o.value = std::to_string((int) std::lround(fxVal(t, mi) * 100)); o.norm = fxVal(t, mi); o.mode = 0; return true;
        }
        if (isFx(id)) { const int i = -10000 - id; o = fxInfo(::fxp::PDEFS[i], fxVal(t, i)); if (std::string(::fxp::PDEFS[i].id).rfind("slot", 0) == 0) o.off = fxVal(t, i) < .5f; return true; }
        if (isBm(id)) {
            const int f = -20000 - id; const float v = bmVal(t, f);
            if (f < Track::BM_SEQ) {   // an LFO 5-8: rate, shape, destination, amount
                switch (f % 4) {
                case 0: o.name = "RATE"; { char b[16]; const double hz = oct::lfoHz(v); std::snprintf(b, sizeof b, hz < 10 ? "%.2f Hz" : "%.1f Hz", hz); o.value = b; } o.norm = v; o.mode = 0; break;
                case 1: { static const char* SH[5] = { "SINE", "TRI", "SAW", "SQUARE", "S&H" }; o.name = "SHAPE"; o.value = SH[std::clamp((int) v, 0, 4)]; o.norm = v / 4; o.mode = 1; break; }
                case 2: { const auto d = dstNames(t); const int k = std::clamp((int) v, 0, (int) d.size() - 1); o.name = "DESTINATION"; o.value = d[(size_t) k]; o.norm = d.size() > 1 ? k / (float) (d.size() - 1) : 0; o.mode = 1; o.off = k == 0; break; }
                default: { char ab[16]; std::snprintf(ab, sizeof ab, "%.2f", v); o.name = "AMOUNT"; o.value = ab; } o.norm = (v + 1) / 2; o.mode = 2; o.off = v == 0;
                }
            } else {   // a sequencer 5-8: destination, amount
                const int r = (f - Track::BM_SEQ) % 18;
                if (r == 0) { const auto d = dstNames(t); const int k = std::clamp((int) v, 0, (int) d.size() - 1); o.name = "DESTINATION"; o.value = d[(size_t) k]; o.norm = d.size() > 1 ? k / (float) (d.size() - 1) : 0; o.mode = 1; o.off = k == 0; }
                else { char ab[16]; std::snprintf(ab, sizeof ab, "%.2f", v); o.name = "AMOUNT"; o.value = ab; o.norm = (v + 1) / 2; o.mode = 2; o.off = v == 0; }
            }
            return true;
        }
        if (id == PADSEL) { const int n = padCount(t); if (n == 8) { o.name = "TRACK"; o.value = "TRACK " + std::to_string(pad[t] + 1); } else { o.name = "PAD"; o.value = padName(pad[t]); } o.norm = pad[t] / (float) (n - 1); o.mode = 1; return true; }
        if (id == MROW) { o.name = "SOURCE"; o.value = pinSide(t, mRow, 0, false); o.norm = mRow / (float) std::max(1, pinRows(t) - 1); o.mode = 1; return true; }
        if (id == MCOL) { o.name = "DESTINATION"; o.value = pinSide(t, 0, mCol, true); o.norm = mCol / (float) std::max(1, pinCols(t) - 1); o.mode = 1; return true; }
        if (id == RIBDST) { const auto v = ribTargets(t); const int k = std::clamp(ribDst[t], 0, (int) v.size() - 1); o.name = "RIBBON >"; o.value = v[(size_t) k].first; o.norm = v.size() > 1 ? k / (float) (v.size() - 1) : 0; o.mode = 1; return true; }
        if (id == MPIN) { o.name = "PIN"; o.value = mPin == 1 ? "WHITE" : "GREEN"; o.norm = mPin == 1 ? 0.f : 1.f; o.mode = 1; return true; }
        for (int n = 0; n < NLFO; ++n) {
            if (id == lfoMode(n)) {
                const int sh = E.id("l" + std::to_string(n) + "shape"); const bool sah = sh >= 0 && (int) std::lround(val(t, sh)) == 4;
                o.name = "LFO / S&H"; o.value = sah ? "S&H" : "LFO"; o.norm = sah ? 1.f : 0.f; o.mode = 1; return true;
            }
            if (id == lfoDst(n) || id == lfoAmt(n)) {
                const int k = link(t, n, false); const bool dst = id == lfoDst(n);
                o.name = dst ? "DESTINATION" : "AMOUNT"; o.mode = dst ? 1 : 2;
                if (k < 0) { o.value = dst ? "---" : "0.00"; o.norm = dst ? 0.f : .5f; o.off = true; return true; }
                const int i = E.id("m" + std::to_string(k) + (dst ? "dst" : "amt")); const float v = val(t, i); const auto& d = E.D[i];
                if (dst) { o.value = choice(d.choices, (int) std::lround(v)); o.norm = v / std::max(1.f, d.mx); o.off = std::lround(v) == 0; }
                else { char b[16]; std::snprintf(b, sizeof b, "%.2f", v); o.value = b; o.norm = (v - d.mn) / (d.mx - d.mn); }
                return true;
            }
        }
        return false;
    }
    void vturn(int t, int k, int id, int d) {
        auto& E = eng(t);
        if (isFxMix(id)) { const int mi = mixOf(t, -9000 - id); if (mi >= 0) fxSet(t, mi, fxVal(t, mi) + d / 127.f); return; }
        if (isFx(id)) {
            const int i = -10000 - id; const auto& p = ::fxp::PDEFS[i];
            if (p.type == 'f') fxSet(t, i, fxVal(t, i) + d * (p.mx - p.mn) / 127.f);
            else { acc[k] += d * .5f; const int s = (int) acc[k]; if (s) { acc[k] -= (float) s; fxSet(t, i, fxVal(t, i) + (float) s); } }
            return;
        }
        if (id == PADSEL) { acc[k] += d * .5f; const int s = (int) acc[k]; if (s) { acc[k] -= (float) s; pad[t] = std::clamp(pad[t] + s, 0, padCount(t) - 1); } return; }
        if (isBm(id)) {
            const int f = -20000 - id; const float v = bmVal(t, f); const bool lfo = f < Track::BM_SEQ; const int r = lfo ? f % 4 : (f - Track::BM_SEQ) % 18;
            if (lfo && r == 0) { bmSet(t, f, std::clamp(v + d / 127.f, 0.f, 1.f)); return; }
            if ((lfo && r == 3) || (!lfo && r == 1)) { bmSet(t, f, std::clamp(v + d / 100.f, -1.f, 1.f)); return; }
            acc[k] += d * .5f; const int s2 = (int) acc[k]; if (!s2) return; acc[k] -= (float) s2;
            const int mx = lfo && r == 1 ? 4 : (int) dstNames(t).size() - 1; bmSet(t, f, (float) std::clamp((int) v + s2, 0, mx)); return;
        }
        if (id == RIBDST) { acc[k] += d * .5f; const int s2 = (int) acc[k]; if (s2) { acc[k] -= (float) s2; ribDst[t] = std::clamp(ribDst[t] + s2, 0, (int) ribTargets(t).size() - 1); } return; }
        if (id == MROW || id == MCOL || id == MPIN) {
            acc[k] += d * .5f; const int s = (int) acc[k]; if (!s) return; acc[k] -= (float) s;
            if (id == MROW) mRow = std::clamp(mRow + s, 0, pinRows(t) - 1);
            else if (id == MCOL) mCol = std::clamp(mCol + s, 0, pinCols(t) - 1);
            else mPin = s > 0 ? 2 : 1;
            return;
        }
        for (int n = 0; n < NLFO; ++n) {
            if (id == lfoMode(n)) { if (d > 0) toSah(t, n, true); else if (d < 0) toSah(t, n, false); return; }
            if (id == lfoDst(n) || id == lfoAmt(n)) {
                const int lk = link(t, n, true); if (lk < 0) return;
                const int i = E.id("m" + std::to_string(lk) + (id == lfoDst(n) ? "dst" : "amt")); const auto& p = E.D[i];
                if (id == lfoDst(n)) { acc[k] += d * .5f; const int s = (int) acc[k]; if (!s) return; acc[k] -= (float) s; setp(t, i, val(t, i) + (float) s); }
                else setp(t, i, val(t, i) + d * (p.mx - p.mn) / 127.f);
                return;
            }
        }
    }
    void toSah(int t, int n, bool sah) {
        auto& E = eng(t); const int sh = E.id("l" + std::to_string(n) + "shape"); if (sh < 0) return;
        const int cur = (int) std::lround(val(t, sh));
        if (sah && cur != 4) { lfoShape[t][n] = cur; setp(t, sh, 4); }
        if (!sah && cur == 4) setp(t, sh, (float) lfoShape[t][n]);
    }
    void vpush(int t, int id) {   // pushed: S&H <-> LFO; a destination: unhook the link; an amount: 0; FX: default (a slot: empty)
        auto& E = eng(t);
        if (id == MROW || id == MCOL) { pinTap(t, mRow, mCol); return; }   // the matrix: the pin at the cursor in / out
        if (isBm(id)) { const int f = -20000 - id; const bool lfo = f < Track::BM_SEQ; const int r = lfo ? f % 4 : (f - Track::BM_SEQ) % 18;
            if ((lfo && (r == 2 || r == 3)) || (!lfo && r <= 1)) bmSet(t, f, 0); else if (lfo && r == 0) bmSet(t, f, .35f); else if (lfo && r == 1) bmSet(t, f, bmVal(t, f) >= 4 ? 0.f : 4.f); return; }
        if (id == MPIN) { mPin = mPin == 1 ? 2 : 1; return; }
        if (isFx(id)) { const int i = -10000 - id; const auto& p = ::fxp::PDEFS[i]; fxSet(t, i, std::string(p.id).rfind("slot", 0) == 0 ? 0.f : p.df); return; }
        if (isFxMix(id)) {   // CHAIN: push = that effect's page
            const auto& G = groups(t, FX); const std::string head = "EFFECT " + std::to_string(-9000 - id + 1) + " ";
            for (int w = 0; w < (int) G.size(); ++w) if (G[(size_t) w].title.rfind(head, 0) == 0) { W(FX) = w; break; }
            return;
        }
        for (int n = 0; n < NLFO; ++n) {
            if (id == lfoMode(n)) { const int sh = E.id("l" + std::to_string(n) + "shape"); toSah(t, n, sh >= 0 && (int) std::lround(val(t, sh)) != 4); return; }
            if (id == lfoAmt(n)) { const int lk = link(t, n, false); if (lk >= 0) setp(t, E.id("m" + std::to_string(lk) + "amt"), 0); return; }
            if (id == lfoDst(n)) { const int lk = link(t, n, false); if (lk >= 0) { setp(t, E.id("m" + std::to_string(lk) + "dst"), 0); setp(t, E.id("m" + std::to_string(lk) + "src"), 0); } return; }
        }
    }

    // ---------------- the pages, cut into groups of 8 (one control per knob) ----------------
    struct Group { int id[8]; std::string title; int seq = -1, pat = -1; std::string names[8]; std::vector<int> sw; std::vector<std::string> swCap; bool matrix = false, pads = false;
        std::string tab, kind; std::vector<int> bars; };   // tab: its name in the tab row; kind: xy (a pad for knobs 1-2), ribbon, wave (the pad's sample); bars: sliders on the screen (bottom row)   // pads: the SAMPLER's pad grid   // matrix: the VC3's pin matrix, on the touch screen   // swCap: each switch's section   // sw: on / off (and two-way) switches, as buttons on the touch screen   // names: shown instead of the control's own   // seq: the step sequencer shown (SEQ); pat: the DR8 track whose pattern is shown
    struct Layout { int e = -1, pad = -1; bool split = false; std::vector<Group> pg[7]; std::vector<int> fxKey; };   // split: PAGE 2 = the second half of VOICE   // VOICE, PAGE2, FILTER, ENV, MOD, SEQ
    std::array<Layout, NT> lay;
    static int slot(int pg) { return pg == VOICE ? 0 : pg == PAGE2 ? 1 : pg == FILTER ? 2 : pg == ENV ? 3 : pg == MOD ? 4 : pg == SEQ ? 5 : pg == FX ? 6 : -1; }
    bool voiceSplit(int t) { groups(t, VOICE); return lay[(size_t) t].split; }
    static const char* page2Name(int e) {
        if (std::string(ENGS[e].id) == "dfam") return "PATTERN";
        for (auto& p : layout::PAGE2_NAME) if (std::string(p[0]) == ENGS[e].id) return p[1];
        return nullptr;
    }
    const std::vector<Group>& groups(int t, int pg) {
        static const std::vector<Group> none;
        const int s = slot(pg); if (s < 0) return none;
        auto& T = box.tr[(size_t) t]; const int e = T.e.load(); auto& L = lay[(size_t) t];
        if (pad[t] >= padCount(t)) pad[t] = 0;
        if (L.e != e || L.pad != pad[t]) build(L, e, pad[t]);
        if (pg == FX) buildFx(t, L);
        return L.pg[s];
    }
    // what goes on FILTER and ENV: the plugin's controls whose section, group or name say so
    static bool isFilter(const std::string& s) {
        for (const char* k : { "FILT", "CUTOFF", "RESON", "VCF", "HPF", "LPF", "EMPH", "HIGH PASS", "LOW PASS", "LOPASS" }) if (s.find(k) != std::string::npos) return true;
        return false;
    }
    static bool isEnv(const std::string& s) {
        for (const char* k : { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ENV", "CONTOUR", "ADSR", "FUNCTION GEN" }) if (s.find(k) != std::string::npos) return true;
        for (const char* k : { "ATT", "DEC", "SUS", "REL" }) { const size_t p = s.find(k); if (p != std::string::npos && (p == 0 || s[p - 1] == ' ')) return true; }
        return false;
    }
    void build(Layout& L, int e, int pd) {
        L.e = e; L.pad = pd; for (auto& g : L.pg) g.clear();
        auto& E = *box.tr[0].eng[(size_t) e]; pairs(e);
        const std::string eid = ENGS[e].id;
        auto id = [&](std::string s) -> int {
            if (s == "@pad") return PADSEL;
            const size_t a = s.find('@'); if (a != std::string::npos) s = s.substr(0, a) + std::to_string(pd) + s.substr(a + 1);
            const int i = E.id(s); return i >= 0 && paired[(size_t) e][(size_t) i] ? -3 : i;   // -3: a switch riding on its level
        };
        auto split = [](const char* x, char sep) { std::vector<std::string> v; std::string w; for (const char* c = x;; ++c) { if (!*c || *c == sep) { v.push_back(w); w.clear(); if (!*c) break; } else w += *c; } return v; };
        auto open = [](std::vector<Group>& G, const std::string& tt) { Group g; std::fill(g.id, g.id + 8, -1); g.title = tt; G.push_back(g); };
        // a section that fits in what is left of the current group joins it; otherwise it starts a new one
        auto isSw = [&](int i) { return i >= 0 && isSwitch(E.D[i]); };
        auto addSw = [&](std::vector<Group>& G, const std::string& title, int i) {   // a switch joins the group of its section
            if (G.empty() || (int) G.back().sw.size() >= MAX_SW) open(G, title);
            G.back().sw.push_back(i); G.back().swCap.push_back(title);
        };
        auto place = [&](std::vector<Group>& G, const std::string& title, const std::vector<int>& all) {
            std::vector<int> ids, sws; for (int i : all) (isSw(i) ? sws : ids).push_back(i);
            for (int i : sws) if (ids.empty()) addSw(G, title, i);   // a section of switches only: with the group before
            if (ids.empty()) return;
            int used = 0; if (!G.empty()) for (int q = 0; q < 8; ++q) if (G.back().id[q] != -1) used = q + 1;
            if (!G.empty() && used > 0 && used + (int) ids.size() <= 8 && (int) (G.back().sw.size() + sws.size()) <= MAX_SW) {
                for (size_t k = 0; k < ids.size(); ++k) G.back().id[used + (int) k] = ids[k];
                if (G.back().title.find(title) == std::string::npos) G.back().title += " · " + title;
                for (int i : sws) { G.back().sw.push_back(i); G.back().swCap.push_back(title); }
                return;
            }
            for (size_t k = 0; k < ids.size(); ++k) {
                if (k % 8 == 0) open(G, ids.size() > 8 ? title + " " + std::to_string(k / 8 + 1) + "/" + std::to_string((ids.size() + 7) / 8) : title);
                G.back().id[k % 8] = ids[k];
            }
            for (int i : sws) addSw(G, title, i);
        };
        // FILTER and ENV: first what the plugin's sections and groups say is a filter or an envelope, then the other controls
        // whose names say so (modulation routings...), packed one after the other
        std::vector<std::pair<int, std::string>> fA, fB, eA, eB;
        struct Sec { int page; std::string title; std::vector<int> ids; }; std::vector<Sec> secs;
        std::vector<layout::Section> all(std::begin(layout::SECTIONS), std::end(layout::SECTIONS));
        all.insert(all.end(), std::begin(BOX_SECTIONS), std::end(BOX_SECTIONS));
        for (const auto& sec : all) {
            if (eid != sec.engine) continue;
            const std::string pgname = sec.page, title = sec.title;
            if (pgname != "VOICE" && pgname != "PAGE2") continue;   // MOD and SEQ are laid out for the panel below
            const auto names = split(sec.ids, ' '), caps = split(sec.caps, '|');
            std::vector<int> ids;
            for (size_t k = 0; k < names.size(); ++k) {
                if (names[k].empty()) continue;
                const int i = id(names[k]); if (i == -1 || i == -3) continue;
                ids.push_back(i);
                if (i < 0) continue;
                const std::string where = up(title.c_str()) + " " + up(k < caps.size() ? caps[k].c_str() : "");
                const std::string name = up(labelOf(e, i).c_str()) + " " + up(E.D[i].id);
                const std::string cap = k < caps.size() && !caps[k].empty() ? caps[k] : title;
                const bool voice = pgname == "VOICE";   // a name alone counts on VOICE only (not the VC3's patch pins, "OSC 1 > FILTER")
                if (isFilter(where) && !isEnv(where)) fA.push_back({ i, cap }); else if (voice && isFilter(name)) fB.push_back({ i, cap });
                if (isEnv(where) || (isEnv(name) && isFilter(where))) eA.push_back({ i, cap }); else if (voice && isEnv(name)) eB.push_back({ i, cap });
            }
            secs.push_back({ pgname == "VOICE" ? 0 : 1, title, ids });
        }
        // VOICE: what is not on FILTER or ENV already. PAGE 2: the plugin's own extra page when it has one, otherwise the
        // second half of VOICE (fewer groups to go through on each button)
        std::vector<int> taken; for (auto& x : fA) taken.push_back(x.first); for (auto& x : eA) taken.push_back(x.first);
        std::vector<Group> voice, extra;
        for (auto& s : secs) {
            std::vector<int> ids; for (int i : s.ids) if (std::find(taken.begin(), taken.end(), i) == taken.end()) ids.push_back(i);
            place(s.page == 0 ? voice : extra, s.title, ids);
        }
        L.split = extra.empty() && voice.size() > 1 && E.id("p0_0") < 0 && E.id("k0src") < 0 && E.id("pin0_0") < 0;   // not when PAGE 2 is the engine's own
        if (L.split) { const size_t h = (voice.size() + 1) / 2; L.pg[1].assign(voice.begin() + (long) h, voice.end()); voice.resize(h); }
        else L.pg[1] = extra;
        L.pg[0] = voice;
        if (E.id("p0_0") >= 0) {   // DR8 PATTERN: one page per track, its 16 steps on the touch screen; knobs: level (push = mute), tune, swing, accent
            L.pg[1].clear();
            for (int n = 0; n < 8; ++n) {
                open(L.pg[1], "TRACK " + std::to_string(n + 1));
                auto& g = L.pg[1].back(); g.pat = n;
                g.id[0] = E.id("t" + std::to_string(n) + "lvl"); g.id[1] = E.id("t" + std::to_string(n) + "tune"); g.id[2] = E.id("swing"); g.id[3] = E.id("accent");
                if (E.id("t0spa") >= 0) g.id[1] = E.id("t" + std::to_string(n) + "spa");   // the DFAM: how far the steps' pitches go (its tune is on VOICE)
            }
        }
        if (E.id("k0src") >= 0) {   // the SAMPLER's PADS: the pad grid on the screen; knobs: pad, level, tune, start, end, decay, pan, mode
            L.pg[1].clear(); open(L.pg[1], "PADS"); auto& g = L.pg[1].back(); g.pads = true;
            const char* K[8] = { "@pad", "k@lvl", "k@tune", "k@st", "k@en", "k@dec", "k@pan", "k@mode" };
            for (int q = 0; q < 8; ++q) g.id[q] = id(K[q]);
        }
        if (E.id("pin0_0") >= 0) {   // the VC3's PATCH MATRIX: the pin board on the screen, a cursor on the knobs
            L.pg[1].clear(); open(L.pg[1], "PATCH MATRIX");
            auto& g = L.pg[1].back(); g.matrix = true; g.id[0] = MROW; g.id[1] = MCOL; g.id[2] = MPIN;
        }
        auto pack = [&](std::vector<Group>& G, std::vector<std::pair<int, std::string>> all, const std::vector<std::pair<int, std::string>>& more) {
            all.insert(all.end(), more.begin(), more.end());
            std::vector<std::pair<int, std::string>> v, sws; for (auto& x : all) (isSw(x.first) ? sws : v).push_back(x);
            if (v.empty()) { for (auto& x : sws) addSw(G, x.second, x.first); return; }
            for (size_t k = 0; k < v.size(); ++k) {
                if (k % 8 == 0) open(G, v[k].second);
                else if (G.back().title.find(v[k].second) == std::string::npos) G.back().title += " · " + v[k].second;
                G.back().id[k % 8] = v[k].first;
            }
            for (auto& x : sws) {   // with the group that holds the knobs of the same section, else the last one
                Group* g = nullptr; for (auto& q : G) if (q.title.find(x.second) != std::string::npos && (int) q.sw.size() < MAX_SW) g = &q;
                if (g) { g->sw.push_back(x.first); g->swCap.push_back(x.second); } else addSw(G, x.second, x.first);
            }
        };
        pack(L.pg[2], fA, fB);
        pack(L.pg[3], eA, eB);
        // the pages laid out by hand for an engine (PAGE_DEFS) take the place of the ones above
        for (int pg : { VOICE, PAGE2, FILTER, ENV }) {
            bool any = false; for (const auto& d : PAGE_DEFS) any = any || (eid == d.eng && d.page == pg);
            if (!any) continue;
            auto& G = L.pg[(size_t) slot(pg)]; G.clear(); if (pg == PAGE2) L.split = false;
            for (const auto& d : PAGE_DEFS) {
                if (eid != d.eng || d.page != pg) continue;
                open(G, d.title); auto& g = G.back(); g.tab = d.tab; g.kind = d.kind;
                std::istringstream k(d.knobs); std::string w; int q = 0;
                while (k >> w && q < 8) { g.id[q++] = w == "." ? -1 : w == "@ribdst" ? RIBDST : id(w); }
                std::istringstream sw(d.sws); while (sw >> w) { const int i = id(w); if (i >= 0) { g.sw.push_back(i);
                    std::string cap = d.title; int r = 0, c2 = 0; if (std::sscanf(w.c_str(), "pin%d_%d", &r, &c2) == 2) { const std::string n = E.D[i].name; cap = "OUTPUT " + n.substr(0, n.find(" > ")) + " (MATRIX) →"; }
                    g.swCap.push_back(cap); } }
                std::istringstream b(d.bars); while (b >> w) { const int i = id(w); if (i >= 0) g.bars.push_back(i); }
                if (g.kind == "matrix") { g.matrix = true; g.id[0] = MROW; g.id[1] = MCOL; g.id[2] = MPIN; }
                if (g.kind == "pads") { g.pads = true; const char* K[8] = { "@pad", "k@lvl", "k@tune", "k@st", "k@en", "k@dec", "k@pan", "k@mode" }; for (int z = 0; z < 8; ++z) g.id[z] = id(K[z]); }
            }
        }
        // MOD: first the 8 LFOs' amounts on the 8 knobs, then one page per LFO (1-4 the engine's: their destination and
        // amount are their first matrix link; 5-8 the box's own, Track::bm)
        if (E.id("l0rate") >= 0) {
            open(L.pg[4], "AMOUNTS"); auto& g = L.pg[4].back(); g.tab = "AMOUNTS";
            for (int n = 0; n < 8; ++n) { g.id[n] = n < NLFO ? lfoAmt(n) : bmId(Track::BM_LFO + 4 * (n - 4) + 3); g.names[n] = "LFO " + std::to_string(n + 1); }
        }
        for (int n = 0; n < NLFO; ++n) {
            const int r = E.id("l" + std::to_string(n) + "rate"), s = E.id("l" + std::to_string(n) + "shape"); if (r < 0) continue;
            open(L.pg[4], "LFO " + std::to_string(n + 1));
            auto& g = L.pg[4].back(); g.id[0] = r; g.id[1] = s; g.id[2] = lfoDst(n); g.id[3] = lfoAmt(n); g.id[4] = lfoMode(n);
        }
        if (E.id("l0rate") >= 0) for (int n = 4; n < 8; ++n) {
            const int b = Track::BM_LFO + 4 * (n - 4); open(L.pg[4], "LFO " + std::to_string(n + 1));
            auto& g = L.pg[4].back(); g.id[0] = bmId(b); g.id[1] = bmId(b + 1); g.id[2] = bmId(b + 2); g.id[3] = bmId(b + 3);
        }
        // SEQ: first the 8 sequencers' amounts, then one page per sequencer; the knobs set where it goes and how much,
        // its 16 steps are on the touch screen (1-4 the engine's, 5-8 the box's own)
        if (E.id("s0dst") >= 0) {
            open(L.pg[5], "AMOUNTS"); auto& g = L.pg[5].back(); g.tab = "AMOUNTS";
            for (int q = 0; q < 8; ++q) { g.id[q] = q < NSEQ ? E.id("s" + std::to_string(q) + "amt") : bmId(Track::BM_SEQ + 18 * (q - 4) + 1); g.names[q] = "SEQ " + std::to_string(q + 1); }
        }
        for (int q = 0; q < 8; ++q) {
            const int d = q < NSEQ ? E.id("s" + std::to_string(q) + "dst") : E.id("s0dst") >= 0 ? bmId(Track::BM_SEQ + 18 * (q - 4)) : -1; if (d == -1) continue;
            open(L.pg[5], "SEQUENCER " + std::to_string(q + 1));
            auto& g = L.pg[5].back(); g.id[0] = d; g.id[1] = q < NSEQ ? E.id("s" + std::to_string(q) + "amt") : bmId(Track::BM_SEQ + 18 * (q - 4) + 1); g.seq = q;
        }
    }
    // FX: one page per slot. Knob 1: the effect, knob 2: its model, then its controls as in the FX plugin (PluginPages.h);
    // a long effect goes on over a second group. FX controls are the rack's parameters, not the engine's: fxCtl(i).
    static int fxCtl(int i) { return -10000 - i; }
    static int fxMix(int q) { return -9000 - q; }   // CHAIN: the mix of slot q's effect
    static bool isFxMix(int id) { return id <= -9000 && id > -9008; }
    int mixOf(int t, int q) {   // the mix control of slot q's effect (-1: an empty slot)
        const int f = (int) std::lround(fxVal(t, box.slotParam[q])) - 1; if (f < 0) return -1;
        return fxIdx("q" + std::to_string(q) + "f" + std::to_string(f) + "mix");
    }
    void fxMove(int from, int to) { if (from == to) return; Msg m; m.k = FXMOVE; m.i = from; m.v = (float) to; host.send(box.selected.load(), m); lay[(size_t) box.selected.load()].fxKey.clear(); }
    static bool isFx(int id) { return id <= -10000 && id > -10000 - ::fxp::NP; }
    static int fxIdx(const std::string& id) {
        static std::unordered_map<std::string, int> m; if (m.empty()) for (int i = 0; i < ::fxp::NP; ++i) m[::fxp::PDEFS[i].id] = i;
        auto it = m.find(id); return it == m.end() ? -1 : it->second;
    }
    float fxVal(int t, int i) { return box.tr[(size_t) t].fxShown[(size_t) i].load(); }
    void buildFx(int t, Layout& L) {
        std::vector<int> key;
        for (int q = 0; q < 8; ++q) {
            const int f = (int) std::lround(fxVal(t, box.slotParam[q])) - 1; key.push_back(f);
            const int mi = f >= 0 ? fxIdx("q" + std::to_string(q) + "f" + std::to_string(f) + "model") : -1;
            key.push_back(mi >= 0 ? (int) std::lround(fxVal(t, mi)) : 0);
        }
        if (key == L.fxKey && !L.pg[6].empty()) return;
        L.fxKey = key; auto& G = L.pg[6]; G.clear();
        const auto& sd = ::fxp::PDEFS[box.slotParam[0]];
        { Group g; g.title = "CHAIN"; for (int q = 0; q < 8; ++q) g.id[q] = fxMix(q); G.push_back(g); }   // the chain: the 8 mixes; drag on the screen to reorder
        for (int q = 0; q < 8; ++q) {
            const int f = key[(size_t) q * 2], m = key[(size_t) q * 2 + 1];
            const std::string name = f >= 0 ? choice(sd.choices, f + 1) : "EMPTY", qs = "q" + std::to_string(q);
            std::vector<std::pair<int, std::string>> ctl { { fxCtl(box.slotParam[q]), "EFFECT" } };
            if (f >= 0) {
                const int mi = fxIdx(qs + "f" + std::to_string(f) + "model"); if (mi >= 0) ctl.push_back({ fxCtl(mi), "MODEL" });
                std::vector<layout::FxModel> models(std::begin(layout::FX_MODELS), std::end(layout::FX_MODELS));
                models.push_back({ 3, 4, "f3dmode f3width f3mix", "MODE|WIDTH|MIX" });   // the J8 chorus (box): mode 1 = I, 2 = II, 3 = I+II
                models.push_back({ 16, 0, "f16src f16key f16rate f16att f16dec f16amt f16mix f16st0 f16st1 f16st2 f16st3 f16st4 f16st5 f16st6 f16st7 f16st8 f16st9 f16st10 f16st11 f16st12 f16st13 f16st14 f16st15",
                                   "SOURCE|KEY|RATE|ATTACK|DECAY|AMOUNT|MIX|STEP 1|STEP 2|STEP 3|STEP 4|STEP 5|STEP 6|STEP 7|STEP 8|STEP 9|STEP 10|STEP 11|STEP 12|STEP 13|STEP 14|STEP 15|STEP 16" });   // SIDECHAIN
                for (const auto& fm : models) {
                    if (fm.fx != f || fm.model != (mi >= 0 ? m : 0)) continue;
                    std::vector<std::string> ids, nm; std::string w;
                    for (const char* c = fm.ids;; ++c) { if (!*c || *c == ' ') { if (!w.empty()) ids.push_back(w); w.clear(); if (!*c) break; } else w += *c; }
                    for (const char* c = fm.names;; ++c) { if (!*c || *c == '|') { nm.push_back(w); w.clear(); if (!*c) break; } else w += *c; }
                    for (size_t k = 0; k < ids.size(); ++k) { const int i = fxIdx(qs + ids[k]); if (i >= 0) ctl.push_back({ fxCtl(i), k < nm.size() ? nm[k] : "" }); }
                    break;
                }
            }
            const int ng = (int) (ctl.size() + 7) / 8;
            for (size_t k = 0; k < ctl.size(); ++k) {
                if (k % 8 == 0) { Group g; std::fill(g.id, g.id + 8, -1); g.title = "EFFECT " + std::to_string(q + 1) + " · " + name + (ng > 1 ? " " + std::to_string(k / 8 + 1) + "/" + std::to_string(ng) : ""); G.push_back(g); }
                G.back().id[k % 8] = ctl[k].first; G.back().names[k % 8] = ctl[k].second;
            }
        }
    }

    // the controls of a page, 8 per group (-1 = an empty knob), as one list
    std::vector<int> controls(int t, int pg) {
        std::vector<int> v; for (const auto& g : groups(t, pg)) v.insert(v.end(), g.id, g.id + 8);
        while (!v.empty() && v.back() == -1) v.pop_back();
        return v;
    }
    // MOD > LFO n: which LFO (0-7) the page shows, -1 none; SYNC puts it on the tempo
    int lfoShown(int t) { if (page != MOD) return -1; const Group* g = group(t, MOD); int n = 0;
        return g && std::sscanf(g->title.c_str(), "LFO %d", &n) == 1 && n >= 1 && n <= 8 ? n - 1 : -1; }
    void lfoSyncToggle(int t, int n) { Msg m; m.k = box::Kind::LFOSYNC; m.i = n; m.v = box.tr[(size_t) t].lfoSync[(size_t) n].load() ? 0.f : 1.f; host.send(t, m); }
    const Group* group(int t, int pg) { const auto& G = groups(t, pg); if (G.empty()) return nullptr; return &G[(size_t) std::clamp(W(pg), 0, (int) G.size() - 1)]; }
    std::string groupTitle(int t, int pg) { const Group* g = group(t, pg); return g ? g->title : ""; }
    // tabs for the bottom of the screen: straight to an LFO, a sequencer, a DR8 track (label, group)
    std::vector<std::pair<std::string, int>> tabs(int t, int pg) {
        std::vector<std::pair<std::string, int>> v;
        if (pg == ENGINE) { if (synthEngine(t)) return { { "ENGINE", 0 }, { "SEQUENCE", 2 }, { "ARPEGGIATOR", 3 }, { "CUSTOM", 1 } }; return { { "ENGINE", 0 }, { "ARPEGGIATOR", 3 }, { "CUSTOM", 1 } }; }
        if (pg == SYNC) return { { "TEMPO & SYNC", 0 }, { "QUANTIZER", 1 } };
        if (pg == MIXER || pg == PRESET) return { { "MIXER", 0 }, { std::string("PRESETS · ") + ENGS[box.tr[(size_t) t].e.load()].name, 1 }, { "PRESETS · ALL TRACKS", 2 } };
        if (const int np = padTabs(t, pg)) {   // the DFAM's tracks, the DR8's, the SAMPLER's pads (of the bank): a tap picks one
            const int bank = pad[t] / 16 * 16;
            for (int k = 0; k < np; ++k) v.push_back({ np == 16 ? padName(bank + k) : "T" + std::to_string(k + 1), PADTAB - (np == 16 ? bank + k : k) });
            return v;
        }
        const auto& G = groups(t, pg);
        for (int w = 0; w < (int) G.size(); ++w) {
            const std::string& s = G[(size_t) w].title; int n = 0;
            auto add = [&](const std::string& l) { for (auto& x : v) if (x.first == l) return; v.push_back({ l, w }); };
            if (!G[(size_t) w].tab.empty()) add(G[(size_t) w].tab);
            else if (std::sscanf(s.c_str(), "LFO %d", &n) == 1) add("LFO " + std::to_string(n));
            else if (s.rfind("MATRIX", 0) == 0) add("MATRIX");
            else if (std::sscanf(s.c_str(), "SEQUENCER %d", &n) == 1) add("SEQ " + std::to_string(n));
            else if (std::sscanf(s.c_str(), "TRACK %d", &n) == 1) add("T" + std::to_string(n));
            else if (s.rfind("TRANSPORT", 0) == 0) add("SWING");
            else if (s == "CHAIN") add("CHAIN");
            else if (std::sscanf(s.c_str(), "EFFECT %d", &n) == 1) { std::string nm = s.substr(s.find("· ") + 3); const size_t sp = nm.find_last_of(' '); if (sp != std::string::npos && nm.find('/', sp) != std::string::npos) nm = nm.substr(0, sp); add(std::to_string(n) + " " + (nm == "EMPTY" ? "—" : nm)); }
        }
        if (v.size() < 2) v.clear();
        return v;
    }
    static constexpr int PADTAB = -1000;   // a tab value at or under this one picks a track or a pad: PADTAB - its number
    // the pages whose tab row picks the track (DFAM, DR8: 8) or the pad (SAMPLER: the 16 of the bank) they edit
    int padTabs(int t, int pg) {
        auto& E = eng(t);
        if (E.id("t0vdec") >= 0) return pg == VOICE || pg == FILTER || pg == ENV ? 8 : 0;   // DFAM
        if (E.id("k0src") >= 0) return pg == PAGE2 || pg == FILTER || pg == ENV ? 16 : 0;   // SAMPLER
        if (E.id("t0kit") >= 0) return pg == ENV ? 8 : 0;                                     // DR8
        return 0;
    }
    void pickTab(int t, int pg, int v) { if (pg == SYNC) syncTab = v; else if (pg == MIXER || pg == PRESET) { page = v ? PRESET : MIXER; if (v) presetKind = v - 1; } else if (v <= PADTAB) pickPad(t, PADTAB - v); else W(pg) = v; }
    int seqShown(int t) { const Group* g = page == SEQ ? group(t, SEQ) : nullptr; return g ? g->seq : -1; }   // SEQ: which sequencer
    int stepId(int t, int q, int k) { return q >= NSEQ ? bmId(Track::BM_SEQ + 18 * (q - NSEQ) + 2 + k) : eng(t).id("s" + std::to_string(q) + "_" + std::to_string(k)); }
    float stepVal(int t, int q, int k) { const int i = stepId(t, q, k); return isBm(i) ? bmVal(t, -20000 - i) : i >= 0 ? val(t, i) : 0.f; }
    void stepSet(int t, int q, int k, float v) { const int i = stepId(t, q, k); if (isBm(i)) bmSet(t, -20000 - i, v); else if (i >= 0) setp(t, i, v); }
    int pitchId(int t, int n, int k) { return eng(t).id("n" + std::to_string(n) + "_" + std::to_string(k)); }   // the DFAM's step pitches
    int patId(int t, int n, int k) { return eng(t).id("p" + std::to_string(n) + "_" + std::to_string(k)); }
    std::string padName(int k) const { return std::string(1, (char) ('A' + k / 16)) + std::to_string(k % 16 + 1); }
    int count(int t, int pg) { int k = 0; for (int i : controls(t, pg)) k += i != -1; return k; }   // controls on a page
    // ---------------- SAMPLER: pads, samples, the file browser ----------------
    static constexpr const char* KIT[16] = { "KICK", "SNARE", "CL HAT", "OP HAT", "CLAP", "RIM", "LO TOM", "HI TOM", "COWBELL", "CRASH", "SHAKER", "SUB C2", "STAB Cm7", "PLUCK C3", "BELL C5", "ZAP" };
    std::string smpPath[NT][64];
    std::vector<float> peaks[NT][64];   // each slot's sample, 256 columns (the waveform on PAGE 2); the kit's are made here
    static void peaksOf(const float* x, size_t n, std::vector<float>& out) {
        out.assign(256, 0.f); if (!n) return;
        for (size_t i = 0; i < n; ++i) { float& o = out[i * 256 / n]; o = std::max(o, std::fabs(x[i])); }
    }
    const std::vector<float>& slotPeaks(int t, int s) {
        auto& P = peaks[t][s];
        if (P.empty() && smpPath[t][s].empty() && s < 16) { const auto k = oct::Sp64::kitGen(s, 48000); peaksOf(k.data(), k.size(), P); }
        return P;
    }   // the file in each slot of each part's SAMPLER ("" = the kit sound, or nothing)
    static std::string baseName(const std::string& f) { const size_t p = f.find_last_of('/'); std::string b = p == std::string::npos ? f : f.substr(p + 1); const size_t d = b.rfind('.'); return d == std::string::npos ? b : b.substr(0, d); }
    std::string slotName(int t, int s) { if (s < 0 || s >= 64) return ""; if (!smpPath[t][s].empty()) return smpPath[t][s] == "-" ? "" : baseName(smpPath[t][s]); return s < 16 ? KIT[s] : ""; }
    int slotOfPad(int t, int k) { const int i = eng(t).id("k" + std::to_string(k) + "src"); return i >= 0 ? (int) std::lround(val(t, i)) : k; }
    void padHit(int t, int k, float vel, int ms = 0) { Msg m; m.k = PADHIT; m.i = k; m.v = vel; m.ms = ms; host.send(t, m); }
    void pickPad(int t, int k) { pad[t] = std::clamp(k, 0, 63); }
    // the browser: folders and WAV files on the SD card (~/octavio/samples) and on USB sticks (/media, /mnt)
    struct Entry { std::string name, path; bool dir = false; };
    bool browsing = false; std::string bdir; int bpick = 0; std::vector<Entry> entries;
    std::function<std::vector<Entry>(const std::string&)> listDir;   // set by the browser build (its "SD card" is the page's files)
    std::vector<Entry> readDir(const std::string& dir) {
        if (listDir) return listDir(dir);
        std::vector<Entry> v;
        if (dir.empty()) {   // the top: the SD card's sample folder, then each USB stick
            v.push_back({ "SD CARD · samples", presetDir + "/samples", true });
            auto subdirs = [](const std::string& dir) { std::vector<std::string> o; if (DIR* d = opendir(dir.c_str())) { while (auto* e = readdir(d)) { const std::string n = e->d_name; if (n[0] == '.') continue; if (DIR* q = opendir((dir + "/" + n).c_str())) { closedir(q); o.push_back(n); } } closedir(d); } std::sort(o.begin(), o.end()); return o; };
            const char* user = std::getenv("USER");
            for (const auto& n : subdirs("/media")) {   // Raspberry Pi OS mounts a stick at /media/<user>/<its name>
                if (user && n == user) { for (const auto& m : subdirs("/media/" + n)) v.push_back({ "USB · " + m, "/media/" + n + "/" + m, true }); }
                else v.push_back({ "USB · " + n, "/media/" + n, true });
            }
            for (const auto& n : subdirs("/mnt")) v.push_back({ "DISK · " + n, "/mnt/" + n, true });
            return v;
        }
        v.push_back({ "..", "", true });
        DIR* d = opendir(dir.c_str()); if (!d) return v;
        std::vector<Entry> dirs, files;
        while (auto* e = readdir(d)) {
            const std::string n = e->d_name; if (n.empty() || n[0] == '.') continue;
            const std::string path = dir + "/" + n;
            if (DIR* q = opendir(path.c_str())) { closedir(q); dirs.push_back({ n, path, true }); continue; }
            std::string lo = n; for (auto& c : lo) c = (char) std::tolower((unsigned char) c);
            if (lo.size() > 4 && lo.compare(lo.size() - 4, 4, ".wav") == 0) files.push_back({ n, path, false });
        }
        closedir(d);
        auto by = [](const Entry& a, const Entry& b) { return a.name < b.name; };
        std::sort(dirs.begin(), dirs.end(), by); std::sort(files.begin(), files.end(), by);
        v.insert(v.end(), dirs.begin(), dirs.end()); v.insert(v.end(), files.begin(), files.end());
        return v;
    }
    void openBrowser() {
        if (!listDir && bdir.empty()) mkdir((presetDir + "/samples").c_str(), 0755);
        browsing = true; entries = readDir(bdir); bpick = std::min(bpick, std::max(0, (int) entries.size() - 1));
    }
    void browseTo(const std::string& dir) { bdir = dir; bpick = 0; entries = readDir(bdir); }
    // an entry chosen: into a folder, up, or the file goes into the pad's slot and the pad plays it (to hear it)
    void browseOpen(int i) {
        if (i < 0 || i >= (int) entries.size()) return; const Entry e = entries[(size_t) i]; bpick = i;
        if (e.name == "..") {   // up; from a top folder (one listed at the top), back to the top
            bool top = false; for (const auto& r : readDir("")) top = top || r.path == bdir;
            const size_t p = bdir.find_last_of('/'); browseTo(top || p == std::string::npos || p == 0 ? "" : bdir.substr(0, p)); return; }
        if (e.dir) { browseTo(e.path); return; }
        const int t = box.selected.load(); const int s = slotOfPad(t, pad[t]);
        host.command("sample " + std::to_string(s + 1) + " " + e.path);
        padHit(t, pad[t], .85f, 1500);
    }
    void browseMove(int d) { if (!entries.empty()) bpick = std::clamp(bpick + d, 0, (int) entries.size() - 1); }

    // ---------------- MIDI learn (a long press on a control on the screen) ----------------
    void learn(int t, int i, int how) { Msg m; m.k = LEARN; m.i = i; m.v = (float) how; host.send(t, m); }   // how: 0 wait, 1 forget, 2 stop
    int ccOf(int t, int i) {   // the CC learnt for control i (-1: none)
        if (i < 0) return -1; auto& L = box.tr[(size_t) t].learnt[(size_t) box.tr[(size_t) t].e.load()];
        for (int cc = 0; cc < 128; ++cc) if (L[(size_t) cc].load() == i) return cc;
        return -1;
    }

    // ---------------- ENGINE > ARPEGGIATOR and ENGINE > SEQUENCE (the part's note sequencer) ----------------
    struct Item { std::string name, value; float norm = 0; int mode = 1; bool off = false; };
    static constexpr const char* ARP_MODES[5] = { "UP", "DOWN", "UP / DOWN", "RANDOM", "AS PLAYED" };
    static constexpr const char* RATES[6] = { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" };
    static std::string noteName(int n) { static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return std::string(N[((n % 12) + 12) % 12]) + std::to_string(n / 12 - 1); }
    bool arpPage() { return page == ENGINE && W(ENGINE) == 3; }
    bool nsPage() { return page == ENGINE && W(ENGINE) == 2; }
    bool synthEngine(int t) { auto& E = eng(t); return E.id("t0kit") < 0 && E.id("t0vdec") < 0 && E.id("k0src") < 0; }
    int nsSel = 0;            // the step knobs 5-6 edit
    bool seqRec = false;      // SEQ: REC, the AMOUNT knob writes the step under the playhead
    float seqRecVal[8] {};
    void arpSet(int t, int i, int v) { Msg m; m.k = ARP; m.i = i; m.v = (float) v; host.send(t, m); }
    void nsSet(int t, int i, int v, int step = -1) { Msg m; m.k = NOTESEQ; m.i = i; m.v = (float) v; m.ms = step; host.send(t, m); }
    Item arpItem(int t, int k) {
        auto& A = box.tr[(size_t) t].arp; Item o; const bool on = A[0].load();
        switch (k) {
        case 0: o = { "ARPEGGIATOR", on ? "ON" : "OFF", on ? 1.f : 0.f, 1, !on }; break;
        case 1: o = { "MODE", ARP_MODES[std::clamp(A[1].load(), 0, 4)], A[1].load() / 4.f, 1 }; break;
        case 2: o = { "OCTAVES", std::to_string(A[2].load()), (A[2].load() - 1) / 3.f, 1 }; break;
        case 3: o = { "SPEED", RATES[std::clamp(A[3].load(), 0, 5)], A[3].load() / 5.f, 1 }; break;
        case 4: o = { "GATE", std::to_string(A[4].load()) + " %", A[4].load() / 100.f, 0 }; break;
        case 5: o = { "LATCH", A[5].load() ? "ON" : "OFF", A[5].load() ? 1.f : 0.f, 1, !A[5].load() }; break;
        default: o.mode = -1;
        }
        if (k > 0 && k < 6 && !on) o.off = true;
        return o;
    }
    void arpTurn(int t, int k, int d) {
        auto& A = box.tr[(size_t) t].arp;
        if (k == 4) { arpSet(t, 4, std::clamp(A[4].load() + d, 5, 100)); return; }
        acc[k] += d * .5f; const int s2 = (int) acc[k]; if (!s2) return; acc[k] -= (float) s2;
        const int lo[6] = { 0, 0, 1, 0, 5, 0 }, hi[6] = { 1, 4, 4, 5, 100, 1 }; if (k > 5) return;
        arpSet(t, k, std::clamp(A[(size_t) k].load() + s2, lo[k], hi[k]));
    }
    void arpPush(int t, int k) { auto& A = box.tr[(size_t) t].arp; if (k == 0 || k == 5) arpSet(t, k, !A[(size_t) k].load()); else if (k == 4) arpSet(t, 4, 50); }
    Item nsItem(int t, int k) {
        auto& T = box.tr[(size_t) t]; Item o; const bool on = T.ns[0].load();
        switch (k) {
        case 0: o = { "SEQUENCE", on ? "ON" : "OFF", on ? 1.f : 0.f, 1, !on }; break;
        case 1: o = { "LENGTH", std::to_string(T.ns[1].load()), (T.ns[1].load() - 1) / 15.f, 1 }; break;
        case 2: o = { "SPEED", RATES[std::clamp(T.ns[2].load(), 0, 5)], T.ns[2].load() / 5.f, 1 }; break;
        case 3: o = { "GATE", std::to_string(T.ns[3].load()) + " %", T.ns[3].load() / 100.f, 0 }; break;
        case 4: o = { "STEP", std::to_string(nsSel + 1), nsSel / 15.f, 1 }; break;
        case 5: o = { "STEP NOTE", noteName(T.nsNote[(size_t) nsSel].load()), T.nsNote[(size_t) nsSel].load() / 127.f, 0 }; break;
        case 6: { const int x = T.ns[4].load(); o = { "TRANSPOSE", (x > 0 ? "+" : "") + std::to_string(x), (x + 24) / 48.f, 2 }; break; }
        case 7: o = { "REC", T.ns[5].load() ? "ON" : "OFF", T.ns[5].load() ? 1.f : 0.f, 1, !T.ns[5].load() }; break;
        }
        return o;
    }
    void nsTurn(int t, int k, int d) {
        auto& T = box.tr[(size_t) t];
        if (k == 3) { nsSet(t, 3, std::clamp(T.ns[3].load() + d, 5, 100)); return; }
        acc[k] += d * .5f; const int s2 = (int) acc[k]; if (!s2) return; acc[k] -= (float) s2;
        switch (k) {
        case 0: nsSet(t, 0, s2 > 0); break;
        case 1: nsSet(t, 1, std::clamp(T.ns[1].load() + s2, 1, 16)); break;
        case 2: nsSet(t, 2, std::clamp(T.ns[2].load() + s2, 0, 5)); break;
        case 4: nsSel = std::clamp(nsSel + s2, 0, 15); break;
        case 5: nsSet(t, 100, std::clamp(T.nsNote[(size_t) nsSel].load() + s2, 0, 127), nsSel); if (!T.nsGate[(size_t) nsSel].load()) nsSet(t, 101, 1, nsSel); break;
        case 6: nsSet(t, 4, std::clamp(T.ns[4].load() + s2, -24, 24)); break;
        case 7: nsSet(t, 5, s2 > 0); break;
        }
    }
    void nsPush(int t, int k) {
        auto& T = box.tr[(size_t) t];
        if (k == 0 || k == 7) nsSet(t, k == 0 ? 0 : 5, !T.ns[(size_t) (k == 0 ? 0 : 5)].load());
        else if (k == 4 || k == 5) nsSet(t, 101, (T.nsGate[(size_t) nsSel].load() + 1) % 3, nsSel);   // the step: rest -> note -> tie
        else if (k == 6) nsSet(t, 4, 0);
    }
    // SEQ with REC: the AMOUNT knob writes into the step under the playhead (the sequencer records the knob's moves)
    void seqRecord(int t, int d) {
        const Group* g = group(t, SEQ); if (!g || g->seq < 0) return; const int q = g->seq;
        const int k = ((box.tr[(size_t) t].engStep.load() % 16) + 16) % 16;   // the sound's step (another copy in the browser)
        float& r = seqRecVal[q]; r = std::clamp(r + d / 64.f, -1.f, 1.f); stepSet(t, q, k, r);
    }

    // ---------------- ENGINE > CUSTOM ----------------
    bool customOn() { return page == ENGINE && W(ENGINE) == 1; }
    int& slotOf(int t, int k) { return custom[t][box.tr[(size_t) t].e.load()][k]; }
    struct Pick { int id; std::string sec; };   // the list to pick a control from: the engine's pages in order, then the rest
    std::vector<Pick> picks(int t) {
        std::vector<Pick> v { { -1, "" } }; auto& E = eng(t); std::vector<char> seen((size_t) E.np, 0);
        auto add = [&](int i, const std::string& sec) { if (i >= 0 && i < E.np && !seen[(size_t) i]) { seen[(size_t) i] = 1; v.push_back({ i, sec }); } };
        for (int pg : { VOICE, PAGE2, FILTER, ENV, MOD, SEQ })
            for (const auto& g : groups(t, pg)) { for (int i : g.id) add(i, g.title); for (size_t q = 0; q < g.sw.size(); ++q) add(g.sw[q], q < g.swCap.size() ? g.swCap[q] : g.title); }
        for (int i = 0; i < E.np; ++i) {
            const std::string id = E.D[i].id; int a, b;
            if (id == "bpm" || id == "play" || id == "master" || std::sscanf(id.c_str(), "s%d_%d", &a, &b) == 2 || std::sscanf(id.c_str(), "p%d_%d", &a, &b) == 2 || id.rfind("pin", 0) == 0) continue;
            if (hiddenMod(id)) continue;   // SEQ 5-6 and the matrix: not on the box
            add(i, "OTHERS");
        }
        return v;
    }
    void startAssign(int k) {
        const int t = box.selected.load(); const auto v = picks(t); assign = k; assignPick = 0;
        for (int q = 0; q < (int) v.size(); ++q) if (v[(size_t) q].id == slotOf(t, k)) assignPick = q;
    }
    void movePick(int d) { const int n = (int) picks(box.selected.load()).size(); assignPick = std::clamp(assignPick + d, 0, n - 1); }
    void confirmAssign() {
        if (assign < 0) return; const int t = box.selected.load(); const auto v = picks(t);
        if (assignPick < (int) v.size()) slotOf(t, assign) = v[(size_t) assignPick].id;
        assign = -1;
    }

    int knobId(int t, int k) {   // the control under knob k on a paged page (-1: none)
        if (page == ENGINE) return customOn() && k >= 0 && k < 8 ? slotOf(t, k) : -1;
        const Group* g = group(t, page); return g && k >= 0 && k < 8 ? g->id[k] : -1;
    }

    std::function<std::vector<std::string>()> listPresets;   // set by the browser build (no files there)
    std::vector<std::string> presets() {
        if (listPresets) return listPresets();
        std::vector<std::string> v; DIR* d = opendir(presetDir.c_str()); if (!d) return v;
        while (auto* e = readdir(d)) { std::string n = e->d_name; if (n.size() > 4 && n.substr(n.size() - 4) == ".oct") v.push_back(n); }
        closedir(d); std::sort(v.begin(), v.end()); return v;
    }
    void saveNew() {   // a new multi: multi-001.oct, multi-002.oct...
        const auto p = presets();
        for (int q = (int) p.size() + 1;; ++q) {
            char b[32]; std::snprintf(b, sizeof b, "multi-%03d.oct", q);
            if (std::find(p.begin(), p.end(), b) == p.end()) { host.command("save " + presetDir + "/" + b); break; }
        }
        const auto p2 = presets(); presetPick = std::max(0, (int) p2.size() - 1);
    }
    // a new multi gets a name typed on the screen's keyboard (or with knob 1: turn = the last letter, push = one more)
    bool naming = false; std::string nameBuf;
    static constexpr const char* CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
    void startName() {
        const auto p = presets(); int q = (int) p.size() + 1; char b[32];
        for (;; ++q) { std::snprintf(b, sizeof b, "multi-%03d.oct", q); if (std::find(p.begin(), p.end(), b) == p.end()) break; }
        std::snprintf(b, sizeof b, "MULTI %03d", q); nameBuf = b; naming = true;
    }
    void nameKey(char c) { if (nameBuf.size() < 24) nameBuf += c; }
    void nameBack() { if (!nameBuf.empty()) nameBuf.pop_back(); }
    void nameTurn(int d) {   // knob 1: the last letter goes through A-Z, 0-9, space, -
        if (nameBuf.empty()) nameBuf = "A";
        const std::string cs = CHARS; size_t i = cs.find(nameBuf.back()); if (i == std::string::npos) i = 0;
        nameBuf.back() = cs[(i + (size_t) ((d % (int) cs.size()) + (int) cs.size())) % cs.size()];
    }
    static std::string fileOf(const std::string& name) {   // "MY PAD 2" -> "my_pad_2.oct" (no spaces: the save command takes one word)
        std::string f; for (char c : name) f += c == ' ' ? '_' : (char) std::tolower((unsigned char) c);
        while (!f.empty() && f.back() == '_') f.pop_back();
        return (f.empty() ? "multi" : f) + ".oct";
    }
    void nameOk() {
        const auto p = presets(); std::string f = fileOf(nameBuf);
        for (int q = 2; std::find(p.begin(), p.end(), f) != p.end(); ++q) f = fileOf(nameBuf + " " + std::to_string(q));   // never over another one
        host.command("save " + presetDir + "/" + f); naming = false;
        const auto p2 = presets(); for (int q = 0; q < (int) p2.size(); ++q) if (p2[(size_t) q] == f) presetPick = q;
    }
    void saveOver() { const auto p = presets(); if (presetPick < (int) p.size()) host.command("save " + presetDir + "/" + p[(size_t) presetPick]); }
    static bool paged(int pg) { return pg >= VOICE && pg <= FX; }

    // ---------------- TEMPO & SYNC ----------------
    static constexpr const char* CLOCK_NAMES[4] = { "INTERNAL", "MIDI", "USB", "BLUETOOTH" };
    static constexpr const char* TX_NAMES[8] = { "NO", "MIDI", "USB", "MIDI + USB", "BLUETOOTH", "MIDI + BT", "USB + BT", "ALL" };
    static constexpr int SYNC_KNOB[8] = { -1, 0, 1, -1, 2, 3, 4, 5 };   // knob -> Box::SYNC_KEYS (knob 1: tempo, knob 4: tap)
    bool external() const { return box.sync.clock.load() != 0; }
    void openSync() { if (page == SYNC) page = prevPage; else { prevPage = page; page = SYNC; } fxSel = -1; }
    void syncSet(const char* key, int v) { host.command(std::string("sync ") + key + " " + std::to_string(v)); }
    void setBpm(float b) { if (external()) return; Msg m; m.k = BPM; m.v = std::clamp(std::round(b * 10) / 10, 40.f, 200.f); host.send(box.selected.load(), m); }
    void tap() {   // tap tempo: the average of the last taps (a pause of 2 s starts again)
        const double now = clock();
        if (!taps.empty() && now - taps.back() > 2) taps.clear();
        taps.push_back(now); if (taps.size() > 5) taps.erase(taps.begin());
        if (taps.size() >= 2) setBpm((float) (60.0 * (double) (taps.size() - 1) / (taps.back() - taps.front())));
    }
    bool tapping() { return !taps.empty() && clock() - taps.back() < 2; }
    // QUANTIZER: knob 1 the key, 2 the scale, 3 the rounding, 5-8 the parts it acts on (their note sequencers)
    static constexpr const char* KEY_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static constexpr const char* QMODE_NAMES[3] = { "NEAREST", "BELOW", "ABOVE" };
    bool quantOn() const { return page == SYNC && syncTab == 1; }
    void quantTurn(int k, int d) {
        acc[k] += d * .5f; const int s = (int) acc[k]; if (!s) return; acc[k] -= (float) s;
        if (k == 0) syncSet("qroot", ((box.qRoot.load() + s) % 12 + 12) % 12);
        else if (k == 1) syncSet("qscale", std::clamp(box.qScale.load() + s, 0, Box::NSCALE - 1));
        else if (k == 2) syncSet("qmode", std::clamp(box.qMode.load() + s, 0, 2));
        else if (k >= 4) { const int b = 1 << (k - 4), q = box.qParts.load(); syncSet("qparts", s > 0 ? q | b : q & ~b); }
    }
    void quantPush(int k) {   // lists go round, the parts flip; a push on the key: back to DO
        if (k == 0) syncSet("qroot", 0);
        else if (k == 1) syncSet("qscale", (box.qScale.load() + 1) % Box::NSCALE);
        else if (k == 2) syncSet("qmode", (box.qMode.load() + 1) % 3);
        else if (k >= 4) syncSet("qparts", box.qParts.load() ^ (1 << (k - 4)));
    }
    void syncTurn(int k, int d) {
        if (quantOn()) { quantTurn(k, d); return; }
        if (k == 0) { setBpm(box.bpm.load() + d * .5f); return; }   // half a BPM per detent
        if (k == 3) return;
        const char* key = Box::SYNC_KEYS[SYNC_KNOB[k]]; const int cur = box.getSync(key), n = k == 1 ? 4 : k == 4 ? 8 : 2;
        acc[k] += d * .5f; const int s = (int) acc[k]; if (!s) return; acc[k] -= (float) s;
        const int v = std::clamp(cur + s, 0, n - 1); if (v != cur) syncSet(key, v);
    }
    void syncPush(int k) {   // a push: lists go round, switches flip, knob 4 taps the tempo, knob 1 back to 120
        if (quantOn()) { quantPush(k); return; }
        if (k == 0) { setBpm(120); return; }
        if (k == 3) { tap(); return; }
        const char* key = Box::SYNC_KEYS[SYNC_KNOB[k]]; const int n = k == 1 ? 4 : k == 4 ? 8 : 2;
        syncSet(key, (box.getSync(key) + 1) % n);
    }

    // a knob turned: d detents (+ right)
    void knob(int k, int d) {
        const int t = box.selected.load(); auto& T = box.tr[(size_t) t]; Msg m;
        if (naming) { if (k == 0) { acc[0] += d * .5f; const int s2 = (int) acc[0]; if (s2) { acc[0] -= (float) s2; nameTurn(s2); } } return; }
        if (browsing) { if (k == 8 || k == 0) { acc[k] += d * .5f; const int s2 = (int) acc[k]; if (s2) { acc[k] -= (float) s2; browseMove(s2); } } return; }
        if (assign >= 0) {   // CUSTOM: picking a control for a knob (knob 9 or that knob go along the list)
            if (k == 8 || k == assign) { acc[k] += d * .5f; const int s = (int) acc[k]; if (s) { acc[k] -= (float) s; movePick(s); } }
            return;
        }
        if (k == 8 && keys) {   // the keyboard on: knob 9 moves it an octave up or down
            acc[8] += d * .5f; const int s = (int) acc[8]; if (s) { acc[8] -= (float) s; keysOct = std::clamp(keysOct + s, 0, 8); } return; }
        if (k == 8) {   // knob 9: along the page, a group at a time
            if (page == FX && fxSel >= 0 && groupTitle(t, FX) == "CHAIN") {   // ...or the effect of the slot touched on CHAIN
                acc[8] += d * .5f; const int s = (int) acc[8]; if (!s) return; acc[8] -= (float) s;
                const int i = box.slotParam[fxSel]; fxSet(t, i, fxVal(t, i) + (float) s); return;
            }
            fxSel = -1;
            const auto& G = groups(t, page); if (G.empty()) return;
            W(page) = std::clamp(W(page) + (d > 0 ? 1 : -1), 0, (int) G.size() - 1);
            return;
        }
        if (arpPage()) { arpTurn(t, k, d); return; }
        if (nsPage()) { nsTurn(t, k, d); return; }
        if (page == SEQ && seqRec && k == 1) { seqRecord(t, d); return; }
        switch (customOn() ? -1 : page) {
        case ENGINE:
            if (k == 0) { acc[0] += d * .5f; const int s = (int) acc[0]; if (s) { acc[0] -= (float) s; m.k = box::Kind::ENGINE; m.i = order()[(size_t) ((orderPos(T.e.load()) + s + NENG * 8) % NENG)]; host.send(t, m); } }
            else if (k == 1) { m.k = VOL; m.v = std::clamp(T.vol.load() + d * .01f, 0.f, 1.5f); host.send(t, m); }
            else if (k == 2) { m.k = PAN; m.v = std::clamp(T.pan.load() + d * .02f, -1.f, 1.f); host.send(t, m); }
            else if (k == 3) { acc[3] += d * .5f; const int s = (int) acc[3]; if (s) { acc[3] -= (float) s; host.command("p " + std::to_string(t + 1)); host.command("chan " + std::to_string(std::clamp(box.chan[(size_t) t].load() + s, 0, 15) + 1)); } }
            else if (k == 4) setBpm(box.bpm.load() + d * 1.f);
            else if (k == 5) { m.k = box::Kind::TUNE; m.v = std::clamp(std::round((T.tuneSt.load() + d * .05f) * 100) / 100, -12.f, 12.f); host.send(t, m); }   // 5 cents a click
            else if (k == 6) { m.k = box::Kind::VINTAGE; m.v = std::clamp(T.vint.load() + d * .01f, 0.f, 1.f); host.send(t, m); }
            break;
        case SYNC: syncTurn(k, d); break;
        case PRESET:
            if (k == 0 && presetKind == 0) { const int n = (int) sounds(T.e.load()).size(); if (n) soundPick = std::clamp(soundPick + d, 0, n - 1); }
            else if (k == 0) { const int n = (int) presets().size(); if (n) presetPick = std::clamp(presetPick + d, 0, n - 1); }
            break;
        case MIXER: {   // knobs 1-4: the parts' volumes, 5-8: their pans
            const int p = k % 4; auto& P = box.tr[(size_t) p];
            if (k < 4) { m.k = VOL; m.v = std::clamp(P.vol.load() + d * .01f, 0.f, 1.5f); }
            else { m.k = PAN; m.v = std::clamp(P.pan.load() + d * .02f, -1.f, 1.f); }
            host.send(p, m); break; }
        default: {
            const int i = knobId(t, k); if (i == -1) break;
            if (isVirtual(i)) { vturn(t, k, i, d); break; }
            auto& E = *T.eng[(size_t) T.e.load()]; const auto& p = E.D[i];
            const float v = T.shown[(size_t) T.e.load()][(size_t) i].load();
            float nv;
            if (p.type == 'f') nv = v + d * (p.mx - p.mn) / 127.f;
            else { acc[k] += d * .5f; const int s = (int) acc[k]; if (!s) break; acc[k] -= (float) s; nv = v + (float) s; }
            if (std::string(p.id).find("shape") != std::string::npos && p.id[0] == 'l') nv = std::min(nv, 3.f);   // LFO shapes; RANDOM is the S&H knob
            int tk; if (std::sscanf(p.id, "t%dkit", &tk) == 1 && E.id("t0inst") >= 0) {   // the DR8's kits in the order 808 909 LINN DX VERM
                static const int ORD[5] = { 0, 1, 2, 4, 3 }; int pos = 0; for (int q = 0; q < 5; ++q) if (ORD[q] == (int) std::lround(v)) pos = q;
                nv = (float) ORD[std::clamp(pos + (int) std::lround(nv - v), 0, 4)];
            }
            m.k = PARAM; m.i = i; m.v = std::clamp(nv, p.mn, p.mx); host.send(t, m);
        } }
    }

    void push(int k) {   // a knob pushed
        const int t = box.selected.load(); auto& T = box.tr[(size_t) t]; Msg m;
        if (browsing) { if (k == 8 || k == 0) browseOpen(bpick); return; }
        if (naming) { if (k == 0) nameKey(nameBuf.empty() ? 'A' : nameBuf.back()); else if (k == 1 || k == 8) nameOk(); else if (k == 2) nameBack(); return; }
        if (assign >= 0) { if (k == 8 || k == assign) confirmAssign(); return; }
        if (customOn() && k < 8 && slotOf(t, k) < 0) { startAssign(k); return; }   // an empty CUSTOM knob: give it a control
        if (k == 8) { m.k = PLAY; m.i = !box.playing.load(); host.send(t, m); return; }
        if (arpPage()) { arpPush(t, k); return; }
        if (nsPage()) { nsPush(t, k); return; }
        if (page == SYNC) { syncPush(k); return; }
        if (page == PRESET && presetKind == 0) { if (k == 0) loadSound(t, soundPick); return; }
        if (page == PRESET) {
            const auto p = presets();
            if (k == 0 && presetPick < (int) p.size()) host.command("load " + presetDir + "/" + p[(size_t) presetPick]);
            if (k == 1) startName();
            return;
        }
        if (page == MIXER) {   // push 1-4: mute the part, 5-8: pan to the centre
            const int p = k % 4; if (k < 4) { m.k = MUTE; m.i = !box.tr[(size_t) p].mute.load(); } else { m.k = PAN; m.v = 0; }
            host.send(p, m); return;
        }
        if (page == ENGINE && !customOn()) { if (k == 1) { m.k = VOL; m.v = .8f; host.send(t, m); } if (k == 2) { m.k = PAN; m.v = 0; host.send(t, m); } if (k == 5) { m.k = box::Kind::TUNE; m.v = 0; host.send(t, m); } if (k == 6) { m.k = box::Kind::VINTAGE; m.v = 0; host.send(t, m); } return; }
        const int i = knobId(t, k); if (i == -1) return;
        if (isVirtual(i)) { vpush(t, i); return; }
        const int e = T.e.load(); const int s = switchOf(e, i);
        if (s >= 0) { setp(t, s, T.shown[(size_t) e][(size_t) s].load() > .5f ? 0.f : 1.f); return; }   // a level with its switch: on / off
        reset(k);
    }
    void reset(int k) {   // back to the default value (the screen's double tap)
        const int t = box.selected.load(); const int i = knobId(t, k); if (i == -1) return;
        if (isVirtual(i)) { if (isFxMix(i)) { const int mi = mixOf(t, -9000 - i); if (mi >= 0) fxSet(t, mi, ::fxp::PDEFS[mi].df); return; } if (isFx(i)) { vpush(t, i); return; } for (int n = 0; n < NLFO; ++n) { if (i == lfoMode(n)) toSah(t, n, false); if (i == lfoAmt(n)) vpush(t, i); } return; }
        setp(t, i, eng(t).D[i].df);
    }

    // a page button: the page; pressed again, the next tab of the page (next LFO, sequencer, effect, DR8 track),
    // or the next group when the page has no tabs; round at the end
    void pagePress(int pg) {
        fxSel = -1; assign = -1; browsing = false; naming = false;
        if (pg == MIXER && (page == MIXER || page == PRESET)) {   // MIXER -> PRESETS · ENGINE -> PRESETS · ALL TRACKS -> MIXER
            if (page == MIXER) { page = PRESET; presetKind = 0; } else if (presetKind == 0) presetKind = 1; else page = MIXER;
            return; }
        if (pg == ENGINE && page == ENGINE) {   // pressed again: the next tab as shown (ENGINE, SEQUENCE, ARPEGGIATOR, CUSTOM)
            const auto tb = tabs(box.selected.load(), ENGINE); size_t k = 0; while (k < tb.size() && tb[k].second != W(ENGINE)) ++k;
            W(ENGINE) = tb[(k + 1) % tb.size()].second; return; }
        if (pg != page) { page = pg; return; }
        if (pg == SYNC) return;
        const int t = box.selected.load(); const auto& G = groups(t, pg); if (G.size() < 2) return;
        const auto tb = tabs(t, pg); int& w = W(pg);
        if (tb.empty()) { w = (w + 1) % (int) G.size(); return; }
        for (const auto& x : tb) if (x.second > w) { w = x.second; return; }
        w = tb.front().second;
    }

    // ---------------- the factory sounds (FactorySounds.h, from sounds/factory.txt) ----------------
    static std::vector<int> sounds(int e) { std::vector<int> v; for (int i = 0; i < NFACTORY; ++i) if (!std::strcmp(FACTORY[i].eng, ENGS[e].id)) v.push_back(i); return v; }
    std::string loadedSound; int loadedEng = -1;   // the last one loaded (shown on the screen)
    void loadSound(int t, int k) {   // every control of the engine back to its default, then the sound's
        const int e = box.tr[(size_t) t].e.load(); const auto v = sounds(e); if (k < 0 || k >= (int) v.size()) return;
        auto& E = eng(t); Msg m; m.k = PARAM;
        for (int i = 0; i < E.np; ++i) { const std::string id = E.D[i].id; if (id == "bpm" || id == "play" || id == "master") continue; m.i = i; m.v = E.D[i].df; host.send(t, m); }
        std::istringstream in(FACTORY[v[(size_t) k]].body); std::string id; float x;
        while (in >> id >> x) { const int i = E.id(id); if (i < 0) continue; m.i = i; m.v = std::clamp(x, E.D[i].mn, E.D[i].mx); host.send(t, m); }
        loadedSound = FACTORY[v[(size_t) k]].name; loadedEng = e;
    }

    // ---------------- KEYS: the page buttons as a keyboard ----------------
    int keyNote(int k) const {   // the k-th note of the scale from the octave's key
        const int sc = box.qScale.load(), root = sc ? box.qRoot.load() : 0; const uint16_t mk = Box::SCALE_MASK[sc ? sc : 1];
        for (int s = 0, n = 0; s < 120; ++s) if ((mk >> (s % 12)) & 1) { if (n == k) return std::clamp(12 * (keysOct + 1) + root + s, 0, 127); ++n; }
        return 60;
    }
    void keyPlay(int k, bool on) {
        Msg m; m.k = box::Kind::MIDI;
        if (keyHeld[k] >= 0) { m.m[0] = 0x80; m.m[1] = (uint8_t) (keyHeld[k] % 128); m.m[2] = 0; host.send(keyHeld[k] / 128, m); keyHeld[k] = -1; }
        if (!on) return;
        const int t = box.selected.load(), n = keyNote(k);
        m.m[0] = 0x90; m.m[1] = (uint8_t) n; m.m[2] = 100; host.send(t, m); keyHeld[k] = t * 128 + n;
    }
    void keysToggle() { for (int k = 0; k < 9; ++k) keyPlay(k, false); keys = !keys; }

    // MIDI from the panel
    void midi(const uint8_t* b, size_t n) {
        Guard lk(mx);
        if (n >= 4 && b[0] == 0xF0 && b[1] == 0x7D && b[2] == 0x4F && b[3] == 0x02) { connected = true; forget(); return; }
        if (n < 3 || (b[0] & 0x0F) != 15) return;
        const int st = b[0] & 0xF0;
        if (st == 0xB0 && b[1] >= 16 && b[1] < 25) { const int d = b[2] < 64 ? b[2] : b[2] - 128; knob(b[1] - 16, d); }
        else if (st == 0xB0 && b[1] == 7) box.master.store(b[2] / 127.f * 1.2f);
        else if (st == 0x90 || st == 0x80) {
            const bool on = st == 0x90 && b[2] > 0; const int note = b[1];
            if (note == 16 + KEYS_BTN) { if (on) keysToggle(); }
            else if (note >= 16 && note < 16 + NBUTTON && keys) keyPlay(note - 16, on);   // the keyboard: a note while held
            else if (note >= 16 && note < 16 + NBUTTON && on) pagePress(note - 16);
            else if (note >= 32 && note < 36 && on) host.command("p " + std::to_string(note - 31));
            else if (note < 9 && on) push(note);
            else if (note == 9 && on) host.command("poweroff");   // DÉFILER held 2 s (the panel's firmware): switch the machine off
        }
    }

    // ---------------- LEDs (called ~30 times a second) ----------------
    void ringFor(int k, int& v, int& mode, bool* dim = nullptr) {
        const int t = box.selected.load(); auto& T = box.tr[(size_t) t]; const int e = T.e.load();
        v = 0; mode = 3;
        auto norm = [&](float x, float a, float b) { return (int) std::lround(std::clamp((x - a) / (b - a), 0.f, 1.f) * 127); };
        if (arpPage() || nsPage()) { const Item o = arpPage() ? arpItem(t, k) : nsItem(t, k); if (o.mode >= 0) { v = (int) std::lround(std::clamp(o.norm, 0.f, 1.f) * 127); mode = o.mode; if (dim && o.off) *dim = true; } }
        else switch (customOn() ? -1 : page) {
        case ENGINE:
            if (k == 0) { v = norm((float) orderPos(e), 0, (float) (NENG - 1)); mode = 1; }
            if (k == 1) { v = norm(T.vol.load(), 0, 1.5f); mode = 0; }
            if (k == 2) { v = norm(T.pan.load(), -1, 1); mode = 2; }
            if (k == 3) { v = norm((float) box.chan[(size_t) t].load(), 0, 15); mode = 1; }
            if (k == 4) { v = norm(box.bpm.load(), 40, 200); mode = 0; }
            if (k == 5) { v = norm(T.tuneSt.load(), -12, 12); mode = 2; }
            if (k == 6) { v = norm(T.vint.load(), 0, 1); mode = 0; if (dim && T.vint.load() <= 0) *dim = true; }
            break;
        case SYNC:
            if (quantOn()) {
                if (k == 0) { v = box.qRoot.load() * 127 / 11; mode = 1; }
                else if (k == 1) { v = box.qScale.load() * 127 / (Box::NSCALE - 1); mode = 1; if (dim && box.qScale.load() == 0) *dim = true; }
                else if (k == 2) { v = box.qMode.load() * 127 / 2; mode = 1; }
                else if (k >= 4) { const bool on = (box.qParts.load() >> (k - 4)) & 1; v = on ? 127 : 0; mode = 1; if (dim && !on) *dim = true; }
                break;
            }
            if (k == 0) { v = norm(box.bpm.load(), 40, 200); mode = 0; if (dim && external()) *dim = true; }
            else if (k == 3) { v = tapping() ? 127 : 0; mode = 1; }
            else { const int n = k == 1 ? 4 : k == 4 ? 8 : 2, x = box.getSync(Box::SYNC_KEYS[SYNC_KNOB[k]]); v = x * 127 / (n - 1); mode = 1; if (dim && x == 0 && k != 1) *dim = true; }
            break;
        case PRESET: if (k == 0) { const int n = presetKind == 0 ? (int) sounds(e).size() : (int) presets().size(), pk = presetKind == 0 ? soundPick : presetPick;
            if (n) { v = n > 1 ? pk * 127 / (n - 1) : 0; mode = 1; } } break;
        case MIXER: { const auto& P = box.tr[(size_t) (k % 4)];
            if (k < 4) { v = norm(P.vol.load(), 0, 1.5f); mode = 0; if (dim && P.mute.load()) *dim = true; } else { v = norm(P.pan.load(), -1, 1); mode = 2; }
            break; }
        default: {
            const int i = knobId(t, k); if (i == -1) break;
            if (isVirtual(i)) { VInfo o; if (vinfo(t, i, o)) { v = (int) std::lround(std::clamp(o.norm, 0.f, 1.f) * 127); mode = o.mode; if (dim && o.off) *dim = true; } break; }
            const auto& p = T.eng[(size_t) e]->D[i]; const float x = T.shown[(size_t) e][(size_t) i].load();
            v = norm(x, p.mn, p.mx); mode = p.mn < 0 && p.mx > 0 ? 2 : (p.type == 'f' ? 0 : 1);
            if (dim && levelOff(t, i)) *dim = true;   // switched off: the ring stays, dimmed
        } }
    }

    void refresh() {
        Guard lk(mx);
        if (!connected) return;
        const int t = box.selected.load();
        for (int k = 0; k < 8; ++k) {
            int v, mode; bool dim = false; ringFor(k, v, mode, &dim);
            const int ct = page == MIXER ? k % 4 : t;   // MIXER: each knob in its part's colour
            uint8_t cd[3]; for (int q = 0; q < 3; ++q) cd[q] = (uint8_t) (dim ? COL[ct][q] / 6 : COL[ct][q]);
            const uint8_t* c = cd; auto& s = sentRing[k];
            if (s.v == v && s.mode == mode && s.r == c[0] && s.g == c[1] && s.b == c[2]) continue;
            if (s.mode == mode && s.r == c[0] && s.g == c[1] && s.b == c[2]) out({ (uint8_t) (0xBF), (uint8_t) (16 + k), (uint8_t) v });
            else out({ 0xF0, 0x7D, 0x4F, 0x10, (uint8_t) k, (uint8_t) v, c[0], c[1], c[2], (uint8_t) mode, 0xF7 });
            s = { v, mode, c[0], c[1], c[2] };
        }
        for (int j = 0; j < 14; ++j) {   // 4 part LEDs then 10 page LEDs
            int r, g, b;
            if (j < 4) {
                const float lvl = std::min(1.f, box.tr[(size_t) j].meter.load() * 2);
                const float k = j == t ? 1.f : .08f + .5f * lvl;
                r = (int) (COL[j][0] * k); g = (int) (COL[j][1] * k); b = (int) (COL[j][2] * k);
            } else {
                const int pg = j - 4; const bool lit = pg == page || (pg == MIXER && page == PRESET);
                r = g = b = lit ? 100 : 6;
                if (pg == SEQ && box.playing.load()) { r = 0; g = lit ? 120 : 60; b = lit ? 40 : 20; }
                if (keys) {   // the keyboard: its button in the part's colour, the keys dim, lit while held
                    const bool on = pg == KEYS_BTN || keyHeld[pg] >= 0;
                    r = on ? COL[t][0] : COL[t][0] / 10; g = on ? COL[t][1] : COL[t][1] / 10; b = on ? COL[t][2] : COL[t][2] / 10;
                } else if (pg == KEYS_BTN) r = g = b = 6;
            }
            if (sentLed[j][0] == r && sentLed[j][1] == g && sentLed[j][2] == b) continue;
            out({ 0xF0, 0x7D, 0x4F, 0x11, (uint8_t) (96 + j), (uint8_t) r, (uint8_t) g, (uint8_t) b, 0xF7 });
            sentLed[j][0] = r; sentLed[j][1] = g; sentLed[j][2] = b;
        }
    }
};

} // namespace box
