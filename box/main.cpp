// octavio-box: the Octavio groovebox as a program, for the hardware synth (Raspberry Pi) and for testing on a computer.
//   octavio-box                         play live: audio out, every MIDI input, commands typed in the terminal
//   octavio-box --render out.wav < s    no devices: run the commands of script s and write the sound to a WAV file
// Options: --threads 0-3 (helpers rendering the parts in parallel)  --rate 48000  --buffer 128  --device <n> (see the "devices" command)  --script <file> (run first)
//          --serial /dev/ttyAMA2 (DIN MIDI of the main board)  --presets <dir>  --phones <0-63>
//          --screen / --no-screen   --rotate 90|270 (Touch Display 2)  --zoom 0.75 (window)  --fonts <dir>
// The screen (ui/Screen.h): on the Pi it drives the Touch Display 2 directly (KMS); on a computer it opens a window
// when built with SDL2. Without a screen the program runs as before, from the terminal.
// The front panel ("Octavio Facade" USB-MIDI device) is found by name; see Panel.h.
// Type "help" for the commands.
#include "Box.h"
#include "Panel.h"
#include "hw.h"
#include "Wav.h"
#include "ui/Display.h"
#include "ui/Screen.h"
#include "ui/Splash.h"
#ifndef OCTAVIO_NO_DEVICES
#include "third_party/rtaudio/RtAudio.h"
#include "third_party/rtmidi/RtMidi.h"
#endif
#include <chrono>
#include <cstdio>
#include <fstream>
#include <dirent.h>
#include <iostream>
#include <mutex>
#include <cstdlib>
#include <sys/stat.h>
#include <map>
#include <set>
#include <sstream>
#include <thread>

using namespace box;

static const char* HELP = R"(Commands (parts 1-4; "p" picks the part the next commands change):
  p <1-4>                 select a part (it also plays the MIDI channels no part listens to)
  chan <1-16>             the part's MIDI channel (two parts on one channel play together: a layer)
  engines                 list the engines           eng <name>      put an engine on the part (mini, pro70, sampler...)
  params [word]           the part's controls (with their CC); a word filters the list
  set <control> <value>   set a control by its id or number      get <control>
  fx [word]               the part's FX controls                fxset <control> <value>   (slot0-slot7: 0 = off, 1-16 = effect)
  vol <0-1>  pan <-1..1>  mute  unmute  tune <-12..12 semitones>  vintage <0-1>
  note <n> [vel 0-1] [ms] play a note on the part (default .8, 400 ms)
  bpm <40-200>  play  stop  panic
  sample <1-64> <wav|kit|-> a SAMPLER slot of the part (sample clear: the kit back)
  arp <key> <n>           arpeggiator: on mode(0-4) oct(1-4) rate(0-5) gate(5-100) latch
  nseq <key> <n>          note sequencer: on len(1-16) rate(0-5) gate(5-100) transpose rec;  nstep <1-16> <note> <gate>
  cc <0-127> <eng> <id>   a CC drives a control (MIDI learn; long press on the screen)   cc clear <eng>
  custom <1-8> <eng> <id> the part's ENGINE > CUSTOM knob (- = none; saved in multis)
  sync [key n]            tempo and sync: clock 0-3 (internal, MIDI, USB, Bluetooth), rxstart 0/1, txclock 0-7 (bits:
                          1 MIDI, 2 USB, 4 Bluetooth), txstart 0/1, thru 0/1, multitempo 0/1 (kept in <presets>/reglages.txt)
  phones <0-63>           headphone amplifier volume (main board)
  wait <ms>               pause (in --render: render that much sound)
  status                  parts, engines, channels, levels, CPU
  page <name>             the front panel's page (engine voice page2 filter env mod seq fx mixer preset)
  knob <1-9> <steps>      turn a front-panel knob     push <1-9>   push it
  shot <file.png>         a picture of the screen        screentime   how long a picture takes to draw
  touch <x> <y> [dy] [dx] a finger on the screen (1280 x 720), dragged by dx, dy
  devices                 audio and MIDI devices
  save <file>  load <file>    the whole synth (4 parts) as a command script
  sound <1-5>                 the engine's factory sound into the selected part
  savepart <file>             the selected part's sound only (load it into any part: p 3, then load <file>)
  quit
)";

static PDef asP(const ::fxp::PDef& f) { return { f.id, f.name, f.type, f.mn, f.mx, f.df, f.nch, f.choices }; }

struct Cpu { std::atomic<float> load { 0 }, peak { 0 }; };

static std::recursive_mutex appMx;        // the terminal, the front panel and the touch screen all run commands
static Panel* g_panel = nullptr;          // the front panel's logic (also driven by the screen)
static ui::Screen* g_screen = nullptr;
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static std::string baseName(const std::string& f) { const size_t p = f.find_last_of('/'); std::string b = p == std::string::npos ? f : f.substr(p + 1); const size_t d = b.rfind('.'); return d == std::string::npos ? b : b.substr(0, d); }

struct App {
    Box& box; Cpu& cpu; bool offline; std::vector<float> wav; int sel = 0;
    std::string confFile, sessionFile; int loading = 0;   // sessionFile: the state kept when the machine is switched off   // the TEMPO & SYNCHRO settings are kept in confFile; loading: inside a multi
    App(Box& b, Cpu& c, bool off) : box(b), cpu(c), offline(off) {}

    void send(Msg m) { sendTo(sel, m); }
    void sendTo(int t, Msg m) {   // live: through the queue to the audio thread; offline: there is no other thread, apply now
        m.t = (int8_t) t;
        if (offline) { box.apply(m); return; }
        while (!box.fromUi.push(m)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    oct::SmpData lineIn; size_t linePos = 0;   // offline: the file played into LINE IN (--line-in), looped
    void render(int ms) {   // offline: run the box for ms milliseconds
        const int total = (int) (ms * box.fs / 1000); float L[128], R[128], IL[128], IR[128];
        for (int done = 0; done < total; done += 128) {
            const int n = std::min(128, total - done);
            if (lineIn.L.empty()) box.process(L, R, n);
            else {
                for (int i = 0; i < n; ++i) { const size_t k = linePos++ % lineIn.L.size(); IL[i] = lineIn.L[k]; IR[i] = lineIn.R.empty() ? IL[i] : lineIn.R[k]; }
                box.process(L, R, n, IL, IR);
            }
            for (int i = 0; i < n; ++i) { wav.push_back(L[i]); wav.push_back(R[i]); }
        }
    }

    int control(const std::string& s) {   // a control by number or id, on the track's engine
        auto& E = box.engine(sel);
        if (!s.empty() && std::isdigit((unsigned char) s[0])) { const int i = std::stoi(s); return i >= 0 && i < E.np ? i : -1; }
        return E.id(s);
    }
    static std::string describe(const PDef& d, float v) {
        char b[64];
        if (d.type == 'c' && d.choices && *d.choices) {
            std::string c = d.choices; int k = (int) std::lround(v - d.mn); size_t p = 0;
            while (k-- > 0 && p != std::string::npos) { p = c.find('|', p); if (p != std::string::npos) ++p; }
            if (p == std::string::npos) return std::to_string(v);
            return c.substr(p, c.find('|', p) - p);
        }
        if (d.type == 'b') return v > .5f ? "on" : "off";
        std::snprintf(b, sizeof b, "%g", v); return b;
    }
    int ccOf(int i) const {
        const auto& map = box.tr[(size_t) sel].ctl[(size_t) box.tr[(size_t) sel].e.load()];
        for (size_t k = 0; k < map.size(); ++k) if (map[k] == i) return synthmap::CC_POOL[k];
        return -1;
    }
    void status() {
        std::printf("tempo %.1f  %s  CPU %.1f %% (peak %.1f %%)  master %.2f\n", box.bpm.load(), box.playing.load() ? "playing" : "stopped",
                    cpu.load.load(), cpu.peak.load(), box.masterMeter.load());
        for (int t = 0; t < NT; ++t) {
            auto& T = box.tr[(size_t) t];
            std::printf("%s part %d  %-8s ch %2d  vol %.2f pan %+.2f %s level %.2f\n", t == sel ? ">" : " ", t + 1,
                        ENGS[T.e.load()].name, box.chan[(size_t) t].load() + 1, T.vol.load(), T.pan.load(), T.mute.load() ? "MUTED" : "     ", T.meter.load());
        }
    }
    void savePartTo(std::ostream& f, int t) {   // a part's sound: engine, its controls, its FX (not channel or mixer)
        auto& T = box.tr[(size_t) t]; const int e = T.e.load(); auto& E = *T.eng[(size_t) e];
        f << "eng " << ENGS[e].id << "\n";
        for (int i = 0; i < E.np; ++i) { const float v = T.shown[(size_t) e][(size_t) i].load(); if (v != E.D[i].df && std::string(E.D[i].id) != "bpm" && std::string(E.D[i].id) != "play") f << "set " << E.D[i].id << " " << v << "\n"; }
        for (int i = 0; i < ::fxp::NP; ++i) { const float v = T.fxShown[(size_t) i].load(); if (v != ::fxp::PDEFS[i].df && std::string(::fxp::PDEFS[i].id) != "bpm") f << "fxset " << ::fxp::PDEFS[i].id << " " << v << "\n"; }
        if (g_panel) { bool any = false; for (int sl = 0; sl < 64; ++sl) any = any || !g_panel->smpPath[t][sl].empty();
            if (any) { f << "sample clear\n"; for (int sl = 0; sl < 64; ++sl) if (!g_panel->smpPath[t][sl].empty()) f << "sample " << sl + 1 << " " << g_panel->smpPath[t][sl] << "\n"; } }
        for (int k = 0; k < Track::BM_N; ++k) f << "bmod " << k << " " << T.bm[(size_t) k].load() << "\n";
        for (int k = 0; k < 8; ++k) f << "lfosync " << k << " " << T.lfoSync[(size_t) k].load() << "\n";
        for (int k = 0; k < 6; ++k) f << "arp " << Box::ARP_KEYS[k] << " " << T.arp[(size_t) k].load() << "\n";
        for (int k = 0; k < 5; ++k) f << "nseq " << Box::NS_KEYS[k] << " " << T.ns[(size_t) k].load() << "\n";
        for (int k = 0; k < 16; ++k) f << "nstep " << k + 1 << " " << T.nsNote[(size_t) k].load() << " " << T.nsGate[(size_t) k].load() << "\n";
        f << "cc clear " << ENGS[e].id << "\n";
        for (int cc = 0; cc < 128; ++cc) { const int i = T.learnt[(size_t) e][(size_t) cc].load(); if (i >= 0 && i < E.np) f << "cc " << cc << " " << ENGS[e].id << " " << E.D[i].id << "\n"; }
        if (g_panel) for (int k = 0; k < 8; ++k) { const int i = g_panel->custom[t][e][k]; f << "custom " << k + 1 << " " << ENGS[e].id << " " << (i >= 0 ? E.D[i].id : "-") << "\n"; }
    }
    void savePart(const std::string& file) {
        std::ofstream f(file); if (!f) { std::printf("cannot write %s\n", file.c_str()); return; }
        f << "# Octavio part sound\n"; savePartTo(f, sel); std::printf("saved part %d to %s\n", sel + 1, file.c_str());
    }
    void save(const std::string& file) {
        std::ofstream f(file); if (!f) { std::printf("cannot write %s\n", file.c_str()); return; }
        f << "# Octavio multi (4 parts)\nbpm " << box.bpm.load() << "\n";
        for (int t = 0; t < NT; ++t) {
            auto& T = box.tr[(size_t) t];
            f << "p " << t + 1 << "\nchan " << box.chan[(size_t) t].load() + 1 << "\nvol " << T.vol.load() << "\npan " << T.pan.load() << "\ntune " << T.tuneSt.load() << "\nvintage " << T.vint.load() << "\n" << (T.mute.load() ? "mute" : "unmute") << "\n";
            savePartTo(f, t);
        }
        f << "p " << sel + 1 << "\n"; std::printf("saved %s\n", file.c_str());
    }

    bool command(const std::string& line);   // false = quit
    void script(std::istream& in) { std::string l; while (std::getline(in, l)) if (!command(l)) break; }
};

#ifndef OCTAVIO_NO_DEVICES
static void devices() {
    RtAudio a; std::printf("Audio outputs:\n");
    for (unsigned id : a.getDeviceIds()) { auto i = a.getDeviceInfo(id); if (i.outputChannels >= 2) std::printf("  %u  %s%s\n", id, i.name.c_str(), i.isDefaultOutput ? "  (default)" : ""); }
    RtMidiIn m; std::printf("MIDI inputs (all are opened):\n");
    for (unsigned k = 0; k < m.getPortCount(); ++k) std::printf("  %u  %s\n", k, m.getPortName(k).c_str());
}
#endif

bool App::command(const std::string& line) {
    std::istringstream in(line); std::string c; if (!(in >> c) || c[0] == '#') return true;
    sel = box.selected.load();   // the panel may have changed the part
    auto& T = box.tr[(size_t) sel]; Msg m;
    if (c == "quit" || c == "exit") return false;
    else if (c == "help") std::printf("%s", HELP);
    else if (c == "p" || c == "t") { int t = 0; in >> t; if (t >= 1 && t <= NT) { sel = t - 1; box.selected.store(sel); } else std::printf("part 1-4\n"); }
    else if (c == "chan") { int ch = 0; in >> ch; if (ch >= 1 && ch <= 16) { m.k = PANIC; send(m); box.chan[(size_t) sel].store(ch - 1); } else std::printf("channel 1-16\n"); }
    else if (c == "savepart") { std::string f; in >> f; savePart(f); }
    else if (c == "engines") { for (int e : Panel::order()) std::printf("  %-6s %-8s %s\n", ENGS[e].id, ENGS[e].name, Panel::family(e)); }
    else if (c == "eng") { std::string s; in >> s; const int e = engineIndex(s); if (e < 0) std::printf("unknown engine (see \"engines\")\n"); else { m.k = ENGINE; m.i = e; send(m); } }
    else if (c == "params" || c == "fx") {
        std::string w; in >> w; for (auto& ch : w) ch = (char) std::toupper((unsigned char) ch);
        const bool fx = c == "fx"; const int e = T.e.load(); auto& E = *T.eng[(size_t) e];
        const int n = fx ? ::fxp::NP : E.np;
        for (int i = 0; i < n; ++i) {
            const PDef d = fx ? asP(::fxp::PDEFS[i]) : E.D[i]; std::string name = d.name, id = d.id, up = id;
            for (auto& ch : up) ch = (char) std::toupper((unsigned char) ch);
            if (!w.empty() && name.find(w) == std::string::npos && up.find(w) == std::string::npos) continue;
            const float v = fx ? T.fxShown[(size_t) i].load() : T.shown[(size_t) e][(size_t) i].load();
            const int cc = fx ? -1 : ccOf(i);
            std::printf("  %3d  %-12s %-28s %-10s [%g..%g]%s\n", i, d.id, d.name, describe(d, v).c_str(), d.mn, d.mx, cc >= 0 ? (" CC " + std::to_string(cc)).c_str() : "");
        }
    }
    else if (c == "set" || c == "get") {
        std::string s; float v = 0; in >> s; const int i = control(s);
        if (i < 0) { std::printf("unknown control (see \"params\")\n"); return true; }
        if (c == "get") { auto& E = box.engine(sel); std::printf("  %s = %s\n", E.D[i].id, describe(E.D[i], T.shown[(size_t) T.e.load()][(size_t) i].load()).c_str()); }
        else if (in >> v) { m.k = PARAM; m.i = i; m.v = v; send(m); }
    }
    else if (c == "fxset") {
        std::string s; float v = 0; in >> s >> v;
        const int i = !s.empty() && std::isdigit((unsigned char) s[0]) ? std::stoi(s) : Box::fxIndex(s);
        if (i < 0 || i >= ::fxp::NP) std::printf("unknown FX control (see \"fx\")\n"); else { m.k = FXPARAM; m.i = i; m.v = v; send(m); }
    }
    else if (c == "tune") { float v = 0; in >> v; m.k = TUNE; m.v = std::min(std::max(v, -12.f), 12.f); send(m); }
    else if (c == "vintage") { float v = 0; in >> v; m.k = VINTAGE; m.v = std::min(std::max(v, 0.f), 1.f); send(m); }
    else if (c == "vol") { float v = .8f; in >> v; m.k = VOL; m.v = std::min(std::max(v, 0.f), 1.5f); send(m); }
    else if (c == "pan") { float v = 0; in >> v; m.k = PAN; m.v = std::min(std::max(v, -1.f), 1.f); send(m); }
    else if (c == "mute" || c == "unmute") { m.k = MUTE; m.i = c == "mute"; send(m); }
    else if (c == "note") { int n = 60, ms = 400; float v = .8f; in >> n; in >> v; in >> ms; m.k = NOTE; m.i = n; m.v = v; m.ms = ms; send(m); }
    else if (c == "sample") {   // sample <1-64> <file.wav | kit | ->: a SAMPLER slot of the part; sample clear: all back to the kit
        std::string a, path; in >> a; std::getline(in >> std::ws, path);
        auto one = [&](int sl, const std::string& f) {
            if (f == "kit" || f.empty()) { box.sendSample(sel, sl, nullptr, false); if (g_panel) { g_panel->smpPath[sel][sl].clear(); g_panel->peaks[sel][sl].clear(); } return true; }
            if (f == "-") { box.sendSample(sel, sl, nullptr, true); if (g_panel) { g_panel->smpPath[sel][sl] = "-"; g_panel->peaks[sel][sl].assign(256, 0.f); } return true; }
            auto d = std::make_shared<oct::SmpData>(); std::string err;
            if (!loadWav(f, *d, err)) { std::printf("%s: %s\n", f.c_str(), err.c_str()); if (g_screen) g_screen->notify(Panel::baseName(f) + " : " + err, nowSec()); return false; }
            box.sendSample(sel, sl, d, true); if (g_panel) { g_panel->smpPath[sel][sl] = f; Panel::peaksOf(d->L.data(), d->L.size(), g_panel->peaks[sel][sl]); } return true;
        };
        if (a == "clear") { for (int sl = 0; sl < 64; ++sl) if (!g_panel || !g_panel->smpPath[sel][sl].empty()) one(sl, "kit"); }
        else { const int sl = std::atoi(a.c_str()) - 1; if (sl < 0 || sl >= 64) std::printf("sample <1-64> <file.wav | kit | ->\n"); else one(sl, path); }
    }
    else if (c == "bmod") { int k = -1; float v = 0; in >> k >> v; if (k >= 0 && k < Track::BM_N) { m.k = BOXMOD; m.i = k; m.v = v; send(m); } }
    else if (c == "lfosync") { int k = -1, v = 0; in >> k >> v; if (k >= 0 && k < 8) { m.k = LFOSYNC; m.i = k; m.v = (float) v; send(m); } }   // LFO k on the tempo   // the box's LFOs / sequencers 5-8
    else if (c == "arp" || c == "nseq") {   // arp <on|mode|oct|rate|gate|latch> <n>; nseq <on|len|rate|gate|transpose|rec> <n>
        std::string k; int v = 0; in >> k >> v; const auto* K = c == "arp" ? Box::ARP_KEYS : Box::NS_KEYS;
        for (int q = 0; q < 6; ++q) if (k == K[q]) { m.k = c == "arp" ? ARP : NOTESEQ; m.i = q; m.v = (float) v; send(m); }
    }
    else if (c == "nstep") {   // nstep <1-16> <note> <gate 0 rest, 1 note, 2 tie>: a step of the note sequencer
        int k = 0, n = 60, g = 1; in >> k >> n >> g; if (k < 1 || k > 16) return true;
        m.k = NOTESEQ; m.ms = k - 1; m.i = 100; m.v = (float) n; send(m); m.i = 101; m.v = (float) g; send(m);
    }
    else if (c == "cc") {   // cc <0-127> <engine> <control>: MIDI learn by hand; cc clear <engine>: back to the default map
        std::string a, en, id; in >> a >> en >> id; const int e = engineIndex(en);
        if (e < 0) { std::printf("cc <0-127> <engine> <control>  |  cc clear <engine>\n"); return true; }
        m.k = CCMAP; m.m[0] = (uint8_t) e;
        if (a == "clear") { m.i = -1; for (int cc = 0; cc < 128; ++cc) { m.ms = cc; send(m); } }
        else { m.ms = std::atoi(a.c_str()); m.i = T.eng[(size_t) e]->id(id); if (m.i >= 0 && m.ms >= 0 && m.ms < 128) send(m); }
    }
    else if (c == "custom" && g_panel) {   // custom <1-8> <engine> <control | ->: the part's ENGINE > CUSTOM knob
        int k = 0; std::string en, id; in >> k >> en >> id; const int e = engineIndex(en);
        if (k < 1 || k > 8 || e < 0) { std::printf("custom <1-8> <engine> <control | ->\n"); return true; }
        g_panel->custom[sel][e][k - 1] = id == "-" ? -1 : T.eng[(size_t) e]->id(id);
    }
    else if (c == "sync") {   // sync <clock|rxstart|txclock|txstart|thru|multitempo> <n>: the TEMPO & SYNCHRO page
        std::string k; int v = 0;
        if (!(in >> k >> v)) { std::printf("%s", box.syncLines().c_str()); return true; }
        if (!box.setSync(k, v)) { std::printf("sync clock 0-3 (internal, MIDI, USB, Bluetooth) | rxstart 0/1 | txclock 0-7 (1 MIDI + 2 USB + 4 Bluetooth) | txstart 0/1 | thru 0/1 | multitempo 0/1\n"
                                   "     qroot 0-11 (C..B) | qscale 0-24 (0 off) | qmode 0-2 (nearest, down, up) | qparts 0-15 (bits: parts 1-4)\n"); return true; }
        if (!offline && !confFile.empty() && !loading) { std::ofstream f(confFile); f << "# Octavio: tempo, sync and quantizer settings\n" << box.syncLines(); }
    }
    else if (c == "bpm") { float v = 120; in >> v;
        if (loading && (!box.sync.multiTempo.load() || box.sync.clock.load() != 0)) return true;   // a multi keeps today's tempo
        m.k = BPM; m.v = std::min(std::max(v, 40.f), 200.f); send(m); }
    else if (c == "play" || c == "stop") { m.k = PLAY; m.i = c == "play"; send(m); }
    else if (c == "panic") { m.k = PANIC; send(m); }
    else if (c == "phones") { int v = 48; in >> v; std::printf(phonesSet(v) ? "headphones %d\n" : "no headphone amplifier (main board, /dev/i2c-1)\n", v); }
    else if (c == "wait") { int ms = 0; in >> ms; if (offline) render(ms); else std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
    else if (c == "status") status();
    else if (c == "poweroff") {   // a long press on DÉFILER (the front panel's note 9): keep the session, then switch off
        if (!sessionFile.empty()) save(sessionFile);
        if (g_screen) g_screen->notify("Shutting down…", nowSec());
        std::printf("power off\n");
        if (!offline) std::thread([] { std::this_thread::sleep_for(std::chrono::milliseconds(900));
            if (std::system("sudo -n /usr/bin/systemctl poweroff") != 0) (void) std::system("systemctl poweroff"); }).detach();
    }
    else if (c == "save") { std::string f; in >> f; save(f); if (g_screen) { g_screen->notify("Saved: " + baseName(f), nowSec()); g_screen->setPresetName(baseName(f)); } }
    else if (c == "load") {
        std::string f; in >> f; std::ifstream s(f);
        if (!s) std::printf("cannot read %s\n", f.c_str());
        else { ++loading; script(s); --loading; if (g_screen) { g_screen->notify("Loaded: " + baseName(f), nowSec()); g_screen->setPresetName(baseName(f)); } }
    }
    else if (c == "sound" && g_panel) {   // the engine's factory sound n (1-5), into the part chosen
        int n = 0; in >> n; std::lock_guard<std::recursive_mutex> l(g_panel->mx);
        const auto v = Panel::sounds(T.e.load()); if (n < 1 || n > (int) v.size()) { std::printf("sound 1-%d\n", (int) v.size()); return true; }
        { std::istringstream b(FACTORY[v[(size_t) n - 1]].body); std::string id; float x; while (b >> id >> x) if (box.engine(sel).id(id) < 0) std::printf("unknown control %s in %s\n", id.c_str(), FACTORY[v[(size_t) n - 1]].name); }
        g_panel->loadSound(sel, n - 1); std::printf("sound %s\n", g_panel->loadedSound.c_str());
    }
    else if (c == "page" && g_panel) {
        std::string w; in >> w; w = ui::Screen::upper(w);
        auto bare = [](std::string s) { s.erase(std::remove(s.begin(), s.end(), ' '), s.end()); return s; };
        if (w == "KEYS") { std::lock_guard<std::recursive_mutex> l(g_panel->mx); g_panel->keysToggle(); }   // the keyboard button
        for (int p = 0; p < Panel::NPAGE; ++p) if (w == bare(ui::Screen::pageName(p))) { std::lock_guard<std::recursive_mutex> l(g_panel->mx); g_panel->pagePress(p); }
    }
    else if ((c == "knob" || c == "push") && g_panel) {
        int k = 0, d = 1; in >> k; in >> d;
        if (k < 1 || k > 9) std::printf("knob 1-9\n");
        else { std::lock_guard<std::recursive_mutex> l(g_panel->mx); if (c == "knob") g_panel->knob(k - 1, d); else g_panel->push(k - 1); }
    }
    else if (c == "touch" && g_screen) {   // a finger: down at x y, dragged by dy (tests and remote control)
        float x = 0, y = 0, dy = 0; in >> x >> y >> dy; const double t = nowSec();
        g_screen->touch(0, x, y, t);
        float dx = 0; in >> dx;   // touch x y dy [dx]
        for (int k = 1; k <= 10 && (dy != 0 || dx != 0); ++k) g_screen->touch(1, x + dx * k / 10, y + dy * k / 10, t);
        g_screen->touch(2, x + dx, y + dy, t);
    }
    else if (c == "screentime" && g_screen) {   // how long one picture takes to draw
        ui::Canvas cv(ui::Screen::W, ui::Screen::H); const double t0 = nowSec();
        for (int k = 0; k < 30; ++k) g_screen->render(cv, nowSec());
        std::printf("screen: %.1f ms per picture\n", (nowSec() - t0) / 30 * 1000);
    }
    else if (c == "shot" && g_screen) {
        std::string f; in >> f; ui::Canvas cv(ui::Screen::W, ui::Screen::H); g_screen->render(cv, nowSec());
        std::printf(ui::writePng(cv, f) ? "screen saved to %s\n" : "cannot write %s\n", f.c_str());
    }
#ifndef OCTAVIO_NO_DEVICES
    else if (c == "devices") devices();
#endif
    else std::printf("unknown command \"%s\" (type help)\n", c.c_str());
    return true;
}

static void writeWav(const std::string& file, const std::vector<float>& s, int rate) {
    std::ofstream f(file, std::ios::binary); auto u32 = [&](uint32_t v) { f.write((const char*) &v, 4); }; auto u16 = [&](uint16_t v) { f.write((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) (s.size() * 2);
    f.write("RIFF", 4); u32(36 + bytes); f.write("WAVEfmt ", 8); u32(16); u16(1); u16(2); u32((uint32_t) rate); u32((uint32_t) rate * 4); u16(4); u16(16);
    f.write("data", 4); u32(bytes);
    for (float x : s) { const int16_t v = (int16_t) std::lround(std::max(-1.f, std::min(1.f, x)) * 32767); f.write((const char*) &v, 2); }
}

int main(int argc, char** argv) {
    int rate = 48000, phones = 48, rotate = 90, screenMode = -1; unsigned buffer = 128, device = 0, inDevice = 0; float zoom = 1;
    bool noInput = false, splashOnly = false; std::string lineFile;
    int threads = -1;   // helper threads for the parts (-1: the cores minus 2, at most 3: 2 on the Pi 5)
    std::string renderTo, first, serial = "/dev/ttyAMA2", fonts = OCTAVIO_FONT_DIR;
    std::string presets = std::string(std::getenv("HOME") ? std::getenv("HOME") : ".") + "/octavio";
    for (int a = 1; a < argc; ++a) {
        const std::string s = argv[a]; auto next = [&] { return a + 1 < argc ? std::string(argv[++a]) : std::string(); };
        if (s == "--rate") rate = std::stoi(next());
        else if (s == "--buffer") buffer = (unsigned) std::stoi(next());
        else if (s == "--device") device = (unsigned) std::stoi(next());
        else if (s == "--input") { const std::string v = next(); if (v == "off") noInput = true; else inDevice = (unsigned) std::stoi(v); }
        else if (s == "--line-in") lineFile = next();
        else if (s == "--render") renderTo = next();
        else if (s == "--script") first = next();
        else if (s == "--serial") serial = next();
        else if (s == "--presets") presets = next();
        else if (s == "--phones") phones = std::stoi(next());
        else if (s == "--screen") screenMode = 1;
        else if (s == "--no-screen") screenMode = 0;
        else if (s == "--rotate") rotate = std::stoi(next());
        else if (s == "--splash") splashOnly = true;
        else if (s == "--zoom") zoom = std::stof(next());
        else if (s == "--fonts") fonts = next();
        else if (s == "--threads") threads = std::stoi(next());
        else { std::printf("octavio-box [--rate 48000] [--buffer 128] [--device n] [--input n|off] [--script file] [--render out.wav]\n"
                           "            [--line-in file.wav (what LINE IN plays in a --render)]\n"
                           "            [--serial /dev/ttyAMA2] [--presets dir] [--phones 0-63]\n"
                           "            [--screen | --no-screen] [--rotate 90|270] [--zoom 1] [--fonts dir] [--threads 0-3]\n"
                           "            [--splash (only the start-up picture, until stopped: octavio-splash.service)]\n"); return 1; }
    }
#ifndef OCTAVIO_NO_DEVICES
    if (splashOnly) {   // while Linux starts: the picture on the display, held until octavio-box takes over (stops us)
        ui::Canvas c(1280, 720); ui::drawSplash(c);
        for (int k = 0; k < 100; ++k) {   // the display driver may still be loading: try for 20 s
            std::string why; auto d = ui::openDrm(rotate, why);
            if (d) { d->show(c); d->show(c); for (;;) std::this_thread::sleep_for(std::chrono::seconds(60)); }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return 1;
    }
#endif
    std::printf("Octavio: building the 4 parts...\n");
    Box box(rate); Cpu cpu;

    struct Host : PanelHost {
        App* a = nullptr;
        void send(int t, Msg m) override { std::lock_guard<std::recursive_mutex> l(appMx); a->sendTo(t, m); }
        void command(const std::string& line) override { std::lock_guard<std::recursive_mutex> l(appMx); a->command(line); }
    };
    mkdir(presets.c_str(), 0755);

    if (!renderTo.empty()) {   // offline: the commands drive a render, no device is opened
        App app(box, cpu, true);
        if (!lineFile.empty()) { std::string err; if (!box::loadWav(lineFile, app.lineIn, err)) { std::printf("--line-in %s: %s\n", lineFile.c_str(), err.c_str()); return 1; } }
        Host host; host.a = &app;
        Panel panel(box, host, [](const std::vector<uint8_t>&) {}, presets, appMx); g_panel = &panel;
        ui::Screen screen(box, panel, host); screen.cpu = &cpu.load;
        if (screen.loadFonts(fonts)) g_screen = &screen; else std::printf("no fonts in %s: no screen pictures\n", fonts.c_str());
        if (!first.empty()) { std::ifstream f(first); app.script(f); }
        app.script(std::cin);
        const auto t0 = std::chrono::steady_clock::now();
        writeWav(renderTo, app.wav, rate);
        (void) t0; std::printf("wrote %s (%.1f s)\n", renderTo.c_str(), app.wav.size() / 2.0 / rate);
        return 0;
    }
#ifdef OCTAVIO_NO_DEVICES
    std::printf("built without audio devices: use --render\n"); return 1;
#else
    struct Ctx { Box* box; Cpu* cpu; } ctx { &box, &cpu };
    RtAudio audio;
    // no sound card that works (no HAT yet, HDMI only): the synth still runs, silent, paced by a clock, so the screen,
    // the panel and the MIDI work for tests
    bool silent = audio.getDeviceIds().empty();
    if (silent) std::printf("no audio device: running silent\n");
    RtAudio::StreamParameters out; out.deviceId = silent ? 0 : (device ? device : audio.getDefaultOutputDevice()); out.nChannels = 2;
    RtAudio::StreamOptions opt; opt.flags = RTAUDIO_NONINTERLEAVED | RTAUDIO_SCHEDULE_REALTIME | RTAUDIO_MINIMIZE_LATENCY; opt.priority = 80; opt.streamName = "Octavio Box";
    // the line input (LINE IN parts): the output device's own inputs (the HAT's ADC on the same card), else the default
    // input, else none; --input n picks one, --input off none
    RtAudio::StreamParameters in; in.nChannels = 2; bool haveIn = false;
    if (!noInput && !silent) {
        auto ins = [&](unsigned d) { const auto i = audio.getDeviceInfo(d); return i.inputChannels; };
        unsigned d = inDevice ? inDevice : (ins(out.deviceId) >= 1 ? out.deviceId : audio.getDefaultInputDevice());
        if (d && ins(d) >= 1) { in.deviceId = d; in.nChannels = std::min(2u, ins(d)); haveIn = true; }
    }
    struct In { unsigned ch = 2; } inCfg { in.nChannels };
    static In* g_in = nullptr; g_in = &inCfg;
    auto cb = [](void* o, void* ib, unsigned n, double, RtAudioStreamStatus, void* u) -> int {
        auto& c = *(Ctx*) u; float* L = (float*) o; float* R = L + n;
        const auto t0 = std::chrono::steady_clock::now();
        const float* il = (const float*) ib; const float* ir = il ? (g_in->ch > 1 ? il + n : il) : nullptr;
        c.box->process(L, R, (int) n, il, ir);
        const double used = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / (n / c.box->fs) * 100;
        c.cpu->load.store((float) (c.cpu->load.load() * .95 + used * .05)); if (used > c.cpu->peak.load()) c.cpu->peak.store((float) used);
        return 0;
    };
    if (haveIn && audio.openStream(&out, &in, RTAUDIO_FLOAT32, (unsigned) rate, &buffer, cb, &ctx, &opt) != RTAUDIO_NO_ERROR) {
        std::printf("line input not opened (%s): output only\n", audio.getErrorText().c_str()); haveIn = false;
    }
    if (!silent && !haveIn && audio.openStream(&out, nullptr, RTAUDIO_FLOAT32, (unsigned) rate, &buffer, cb, &ctx, &opt) != RTAUDIO_NO_ERROR) {
        std::printf("cannot open audio (%s): running silent\n", audio.getErrorText().c_str()); silent = true;
    }
    if (haveIn) std::printf("line in: %s, %u channel%s\n", audio.getDeviceInfo(in.deviceId).name.c_str(), in.nChannels, in.nChannels > 1 ? "s" : "");
    {   // the parts in parallel when the box is busy (box/PartPool.h); one core is left to the screen and the MIDI
        const int hw = (int) std::thread::hardware_concurrency(); const int h = threads >= 0 ? std::min(threads, 3) : std::clamp(hw - 2, 0, 3);
        box.pool.start(h, 70); std::printf("parts: %d helper thread%s (%d cores)\n", h, h == 1 ? "" : "s", hw);
    }
    if (!silent) {
        audio.startStream();
        std::printf("audio: %s, %d Hz, %u samples per block (%.1f ms)\n", audio.getDeviceInfo(out.deviceId).name.c_str(), rate, buffer, buffer * 1000.0 / rate);
    } else std::thread([cb, &ctx, buffer, rate] {   // silent: the same callback, on a clock
        std::vector<float> o(buffer * 2); auto next = std::chrono::steady_clock::now();
        const auto blk = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>((double) buffer / rate));
        for (;;) { cb(o.data(), nullptr, buffer, 0, 0, &ctx); next += blk; std::this_thread::sleep_until(next); }
    }).detach();

    MidiIn midiIn(box);   // MIDI bytes go straight to the box's queue
    auto isPanel = [](const std::string& n) { return n.find("Octavio Facade") != std::string::npos; };
    // every MIDI port but the front panel is opened, and looked for again every 2 s (a device plugged in or paired
    // later). A port is USB (src 2), or Bluetooth (src 3) when it is named after a device BlueZ knows (BLE MIDI);
    // the virtual "Octavio Box" ports (for a DAW on the Pi) count as USB.
    struct InPort { std::unique_ptr<RtMidiIn> p; MidiIn* m; int src; };
    static std::map<std::string, std::unique_ptr<InPort>> ins;
    static std::map<std::string, std::pair<std::unique_ptr<RtMidiOut>, int>> outs;   // name -> port, src
    static std::mutex outsMx;
    auto inCb = [](double, std::vector<unsigned char>* msg, void* u) { auto* q = (InPort*) u; q->m->bytes(msg->data(), msg->size(), q->src, nowSec()); };
    { auto q = std::make_unique<InPort>(); q->m = &midiIn; q->src = 2;
      try { q->p = std::make_unique<RtMidiIn>(); q->p->openVirtualPort("Octavio Box"); q->p->ignoreTypes(true, false, true); q->p->setCallback(inCb, q.get());
            std::printf("MIDI in: virtual port \"Octavio Box\" (a DAW can send to it)\n"); ins["(virtual)"] = std::move(q); } catch (...) {} }
    { try { auto v = std::make_unique<RtMidiOut>(); v->openVirtualPort("Octavio Box"); outs["(virtual)"] = { std::move(v), 2 }; } catch (...) {} }
    auto scanPorts = [&midiIn, isPanel, inCb](bool verbose) {
        const auto bt = bluetoothNames();
        auto srcOf = [&](const std::string& n) { for (auto& b : bt) if (n.find(b) != std::string::npos) return 3; return 2; };
        auto skip = [&](const std::string& n) { return isPanel(n) || n.find("Midi Through") != std::string::npos || n.find("Octavio Box") != std::string::npos || n.find("Octavio Panel") != std::string::npos; };
        try {
            RtMidiIn probe; std::set<std::string> now;
            for (unsigned k = 0; k < probe.getPortCount(); ++k) {
                const std::string n = probe.getPortName(k); if (skip(n)) continue; now.insert(n);
                if (ins.count(n)) continue;
                auto q = std::make_unique<InPort>(); q->m = &midiIn; q->src = srcOf(n);
                q->p = std::make_unique<RtMidiIn>(); q->p->openPort(k, "Octavio Box"); q->p->ignoreTypes(true, false, true); q->p->setCallback(inCb, q.get());
                std::printf("%sMIDI in: %s%s\n", verbose ? "" : "\n", n.c_str(), q->src == 3 ? " (Bluetooth)" : ""); ins[n] = std::move(q);
            }
            for (auto it = ins.begin(); it != ins.end();) it = it->first != "(virtual)" && !now.count(it->first) ? ins.erase(it) : std::next(it);
        } catch (...) {}
        try {
            RtMidiOut probe; std::set<std::string> now;
            for (unsigned k = 0; k < probe.getPortCount(); ++k) {
                const std::string n = probe.getPortName(k); if (skip(n)) continue; now.insert(n);
                std::lock_guard<std::mutex> l(outsMx); if (outs.count(n)) continue;
                auto o = std::make_unique<RtMidiOut>(); o->openPort(k, "Octavio Box"); outs[n] = { std::move(o), srcOf(n) };
            }
            std::lock_guard<std::mutex> l(outsMx);
            for (auto it = outs.begin(); it != outs.end();) it = it->first != "(virtual)" && !now.count(it->first) ? outs.erase(it) : std::next(it);
        } catch (...) {}
    };
    scanPorts(true);
    std::thread([scanPorts] { for (;;) { std::this_thread::sleep_for(std::chrono::seconds(2)); scanPorts(false); } }).detach();

    // DIN MIDI on the main board: input feeds the box and is echoed to MIDI OUT (thru, if on; without its clock and
    // start / stop when the box sends its own there)
    const int din = serialMidiOpen(serial.c_str());
    if (din >= 0) {
        std::printf("MIDI DIN: %s\n", serial.c_str());
        std::thread([din, &midiIn, &box] {
            uint8_t b[64], o[64], msg[3]; int status = 0, nd = 0;
            for (;;) {
                const int n = serialRead(din, b, sizeof b); if (n <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
                const double now = nowSec();
                if (box.sync.thru.load()) {
                    const bool own = box.sync.txClock.load() & 1; int k = 0;
                    for (int i = 0; i < n; ++i) if (!(own && (b[i] == 0xF8 || b[i] == 0xFA || b[i] == 0xFB || b[i] == 0xFC))) o[k++] = b[i];
                    if (k) serialWrite(din, o, k);
                }
                for (int i = 0; i < n; ++i) {
                    const uint8_t x = b[i];
                    if (x >= 0xF8) { midiIn.bytes(&x, 1, 1, now); continue; }   // realtime: clock, start, stop
                    if (x & 0x80) { status = x < 0xF0 ? x : 0; nd = 0; continue; }
                    if (!status) continue;
                    msg[0] = (uint8_t) status; msg[1 + nd++] = x;
                    const int need = (status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0 ? 1 : 2;
                    if (nd >= need) { midiIn.bytes(msg, (size_t) need + 1, 1, now); nd = 0; }
                }
            }
        }).detach();
    }
    // MIDI clock out (TEMPO & SYNCHRO): 24 ticks per beat at the box's tempo, and start / stop, to DIN MIDI OUT, to the
    // USB MIDI outputs (and the virtual "Octavio Box" output for a DAW) and / or to the Bluetooth ones
    std::thread([din, &box] {
        auto out = [&](uint8_t x, int to) {   // to: 1 DIN, 2 USB, 4 Bluetooth
            if ((to & 1) && din >= 0) serialWrite(din, &x, 1);
            if (to & 6) { std::vector<unsigned char> v { x }; std::lock_guard<std::mutex> l(outsMx);
                for (auto& o : outs) if (to & (o.second.second == 3 ? 4 : 2)) try { o.second.first->sendMessage(&v); } catch (...) {} }
        };
        bool was = box.playing.load(); double next = nowSec();
        for (;;) {
            const int to = box.sync.txClock.load(); const bool pl = box.playing.load();
            if (pl != was) { if (to && box.sync.txStart.load()) out(pl ? 0xFA : 0xFC, to); was = pl; if (pl) next = nowSec(); }
            if (to) out(0xF8, to);
            next += 60.0 / (std::max(20.f, box.bpm.load()) * 24);
            const double now = nowSec(); if (next < now - .05) next = now;   // late (a stall): no burst of ticks
            std::this_thread::sleep_for(std::chrono::duration<double>(next - now));
        }
    }).detach();
    if (phonesSet(phones)) std::printf("headphones: volume %d/63\n", phones);

    App app(box, cpu, false);
    static Host host; host.a = &app;
    app.confFile = presets + "/reglages.txt"; app.sessionFile = presets + "/.derniere-session";
    { std::ifstream f(app.confFile); if (f) { ++app.loading; app.script(f); --app.loading; } }

    // the front panel: found by name, re-probed every 2 s so it can be plugged in later
    static std::unique_ptr<RtMidiOut> panelOut; static std::unique_ptr<RtMidiIn> panelIn; static std::mutex outMx;
    static Panel panel(box, host, [](const std::vector<uint8_t>& m) {
        std::lock_guard<std::mutex> l(outMx);
        if (panelOut) { try { std::vector<unsigned char> v(m.begin(), m.end()); panelOut->sendMessage(&v); } catch (...) {} }
    }, presets, appMx);
    g_panel = &panel;

    // the screen: the Pi's display (KMS) when there is no desktop, else a window if built with SDL2
    static ui::Screen screen(box, panel, host); screen.cpu = &cpu.load;
    std::unique_ptr<ui::Display> display;
    if (screenMode != 0) {
        if (!screen.loadFonts(fonts)) std::printf("screen: no fonts in %s (--fonts)\n", fonts.c_str());
        else {
            std::string why, why2;
            const bool desktop = std::getenv("DISPLAY") || std::getenv("WAYLAND_DISPLAY");
#ifdef __APPLE__
            display = ui::openSdl(zoom, why);
#else
            if (!desktop) display = ui::openDrm(rotate, why);
            if (!display) display = ui::openSdl(zoom, why2);
#endif
            if (display) {
                g_screen = &screen; std::printf("screen: %s\n", display->name());
                ui::Canvas sp(1280, 720); ui::drawSplash(sp); display->show(sp); display->show(sp);   // the start-up picture a moment
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            else if (screenMode == 1) std::printf("screen: %s%s%s\n", why.c_str(), why2.empty() ? "" : "; ", why2.c_str());
        }
    }
    if (!first.empty()) { std::lock_guard<std::recursive_mutex> l(appMx); std::ifstream f(first); app.script(f); }
    else { std::lock_guard<std::recursive_mutex> l(appMx); std::ifstream f(app.sessionFile); if (f) app.script(f); }   // back where it was switched off
    std::thread([&isPanel] {
        int tick = 0;
        for (;; ++tick) {
            if (!panelIn && tick % 60 == 0) {
                try {
                    RtMidiIn pi; RtMidiOut po; int ii = -1, oo = -1;
                    for (unsigned k = 0; k < pi.getPortCount(); ++k) if (isPanel(pi.getPortName(k))) ii = (int) k;
                    for (unsigned k = 0; k < po.getPortCount(); ++k) if (isPanel(po.getPortName(k))) oo = (int) k;
                    if (ii >= 0 && oo >= 0) {
                        auto in = std::make_unique<RtMidiIn>(); in->openPort((unsigned) ii, "Octavio Panel"); in->ignoreTypes(false, true, true);
                        in->setCallback([](double, std::vector<unsigned char>* m, void*) { panel.midi(m->data(), m->size()); }, nullptr);
                        { std::lock_guard<std::mutex> l(outMx); panelOut = std::make_unique<RtMidiOut>(); panelOut->openPort((unsigned) oo, "Octavio Panel"); }
                        panelIn = std::move(in); std::printf("\nfront panel connected\n");
                    }
                } catch (...) {}
            }
            if (panelIn && !panel.connected && tick % 30 == 0) panel.hello();
#ifdef __linux__
            if (tick % 60 == 30 && g_screen) {   // a computer on the USB-C port (USB gadget, systeme/usb-gadget.sh): say where the files are
                static bool was = false; bool now = false;
                if (DIR* d = opendir("/sys/class/udc")) {
                    while (dirent* e = readdir(d)) { if (e->d_name[0] == '.') continue; std::ifstream f(std::string("/sys/class/udc/") + e->d_name + "/state"); std::string st; f >> st; now = now || st == "configured"; }
                    closedir(d);
                }
                if (now != was) { std::lock_guard<std::recursive_mutex> l(appMx); g_screen->notify(now ? "Computer connected: files at http://octavio.local" : "Computer disconnected", nowSec()); was = now; }
            }
#endif
            panel.refresh();
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    }).detach();

    std::printf("Ready. Parts 1-4: MINI, PRO70, VACT, DR8 on MIDI channels 1-4. Type help.\n");
    static std::atomic<bool> quit { false };
    auto terminal = [&app] {   // the terminal; when it closes (a service has none) the synth keeps running if it has a screen
        std::string line;
        while (std::printf("octavio t%d> ", app.box.selected.load() + 1), std::fflush(stdout), std::getline(std::cin, line)) {
            std::lock_guard<std::recursive_mutex> l(appMx);
            if (!app.command(line)) { quit = true; return; }
        }
        if (!g_screen) quit = true;
    };
    if (display) {   // the screen runs on the main thread (a window needs it on a Mac), the terminal on its own
        std::thread(terminal).detach();
        ui::Canvas canvas(ui::Screen::W, ui::Screen::H);
        while (!quit) {
            const double t = nowSec();
            if (!display->poll([&](int what, float x, float y) { if (what == 3) screen.key((int) x); else screen.touch(what, x, y, t); })) break;
            screen.render(canvas, t);
            display->show(canvas);   // the display paces the loop (vertical blank); cap it at ~30 frames per second
            const double left = 1 / 30.0 - (nowSec() - t);
            if (left > 0) std::this_thread::sleep_for(std::chrono::duration<double>(left));
        }
    } else terminal();
    if (!silent) { audio.stopStream(); audio.closeStream(); }
    std::_Exit(0);   // detached threads (panel, MIDI, terminal) are still running
#endif
}
