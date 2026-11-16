// J8 (8-voice poly after the Jupiter-8, with arpeggiator and chorus) and CS8 (8-voice dual-layer poly after the CS-80),
// on the shared PolyBase of web/poly-web (juno.html, cs80.html). Ported line for line.
#pragma once
#include "LadderTune.h"
#include "EngineBase.h"
#include "J8Params.h"
#include "Cs8Params.h"

namespace oct {

namespace poly {
inline double th(double x) { if (x > 3) return 1; if (x < -3) return -1; const double x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
inline double blep(double t, double dt) { return oct::blepAny(t, dt); }
struct Dec {
    static constexpr int N = 32; float h[N] {}, b[2 * N] {}; int p = 0;
    explicit Dec(int os = 2) {
        const double fc = .42 / os; double s = 0;
        for (int i = 0; i < N; ++i) { const double m = i - (N - 1) / 2.0; double v = m == 0 ? 2 * fc : std::sin(TP * fc * m) / (PI * m); v *= .42 - .5 * std::cos(TP * i / (N - 1)) + .08 * std::cos(2 * TP * i / (N - 1)); h[i] = (float) v; s += v; }
        for (int i = 0; i < N; ++i) h[i] = (float) (h[i] / s);
    }
    void push(double x) { b[p] = b[p + N] = (float) x; p = (p + 1) % N; }
    double out() const { double a = 0; for (int i = 0; i < N; ++i) a += (double) h[i] * b[p + i]; return a; }
};
struct ADSR {
    double v = 0; int st = 0; bool lin = false;
    void on() { st = 1; } void off() { if (st) st = 4; }
    double tick(double dt, double a, double d, double s, double r) {
        if (st == 1) { if (lin) v += dt / a; else v += (1.3 - v) * (1 - std::exp(-dt / (a / 1.2))); if (v >= 1) { v = 1; st = 2; } }
        else if (st == 2) { v += (s - v) * (1 - std::exp(-dt / (d / 4))); }
        else if (st == 4) { v += (0 - v) * (1 - std::exp(-dt / (r / 4))); if (v < 1e-5) { v = 0; st = 0; } }
        return v;
    }
};
// BBD-style stereo chorus: two taps on one delay line, triangle LFO in opposite phase for L and R
struct Chorus {
    double fs = 48000; float b[4096] {}; int i = 0; double ph = 0, lp[2] {};
    void init(double sr) { fs = sr; for (auto& x : b) x = 0; i = 0; ph = 0; lp[0] = lp[1] = 0; }
    void run(double x, double rate, double centre, double depth, double* out) {
        const int N = 4096; b[i] = (float) x; ph += rate / fs; if (ph >= 1) ph -= 1; const double tri = 1 - 4 * std::fabs(ph - .5);
        for (int c = 0; c < 2; ++c) {
            const double dms = centre + (c ? -tri : tri) * depth; const double d = dms * .001 * fs; double r = i - d; while (r < 0) r += N; const int i0 = (int) std::floor(r); const double f = r - i0;
            const double v = b[i0 % N] * (1 - f) + b[(i0 + 1) % N] * f; lp[c] += .55 * (v - lp[c]); out[c] = lp[c];
        }
        i = (i + 1) % N;
    }
};
} // namespace poly

// The shared poly base: voice allocation, sequencer trigger, LFOs, S&H, mod matrix. D = the engine, Vc = its voice.
template <class D_, class Vc>
struct PolyBase : EngineBase {
    std::vector<Vc> v; std::vector<int> held; int clockAge = 0; double bendB = 0;   // bendB: the base's own bend (the app routes bend to TUNE instead)
    std::vector<float> src; double lp[4] {}, lr[4] {}, shp = 0, sht = 0, shv = 0, sh2p = 0, sh2t = 0, sh2v = 0, clock = 0; int stp = 0;
    double ms = 0, dl[2] {}, xl[2] {}; long mc = 0; bool seqGate = false; int lastNote = 60; double lastVel = .8; double out2[2] {};
    Vc* lv = nullptr;
    int nv = 8, nsrc = 18; uint32_t seed = 192837465;
    int iS[6][16] {}, iSdst[6] {}, iSamt[6] {}, iM[8][3] {}, iL[4][2] {}, iBpm = 0, iTrig = 0, iMaster = 0, iShrate = 0, iShsm = 0, iSh2rate = 0, iSh2sm = 0;
    D_& self() { return *static_cast<D_*>(this); }

    void setupBase(const PDef* d, const int* dstp, int n) {
        setTable(d, dstp, n);
        for (int k = 0; k < 6; ++k) { for (int j = 0; j < 16; ++j) iS[k][j] = id("s" + std::to_string(k) + "_" + std::to_string(j)); iSdst[k] = id("s" + std::to_string(k) + "dst"); iSamt[k] = id("s" + std::to_string(k) + "amt"); }
        for (int m = 0; m < 8; ++m) { const std::string x = "m" + std::to_string(m); iM[m][0] = id(x + "src"); iM[m][1] = id(x + "dst"); iM[m][2] = id(x + "amt"); }
        for (int i = 0; i < 4; ++i) { iL[i][0] = id("l" + std::to_string(i) + "rate"); iL[i][1] = id("l" + std::to_string(i) + "shape"); }
        iBpm = id("bpm"); iTrig = id("trig"); iMaster = id("master"); iShrate = id("shrate"); iShsm = id("shsm"); iSh2rate = id("sh2rate"); iSh2sm = id("sh2sm");
    }
    void resetBase() {
        rs = seed; v.clear(); for (int i = 0; i < nv; ++i) v.push_back(self().makeVoice(i)); held.clear(); clockAge = 0; bendB = 0;
        src.assign((size_t) nsrc, 0.f); const double L0[4] = { 0, .33, .66, .5 }; for (int i = 0; i < 4; ++i) { lp[i] = L0[i]; lr[i] = 0; } shp = sht = shv = sh2p = sh2t = sh2v = clock = 0; stp = 0;
        ms = 0; dl[0] = dl[1] = xl[0] = xl[1] = 0; mc = 0; seqGate = false; lastNote = 60; lastVel = .8; out2[0] = out2[1] = 0; lv = nullptr;
    }
    int step() const override { return stp; }
    int meters(float* m) override { m[0] = src[11]; m[1] = src[12]; int k = 2; for (const auto& x : v) m[k++] = x.active() ? 1.f : 0.f; return k; }
    bool isHeld(int n) const { return std::find(held.begin(), held.end(), n) != held.end(); }
    void trig(Vc& x, int n, double vel) { x.tgtN = n; if (!x.active()) x.curN = n; x.note = n; x.vel = vel; x.gate = true; x.age = ++clockAge; self().start(x); }
    void release(Vc& x) { x.gate = false; self().stop(x); }
    template <class Pred> Vc* oldest(Pred pr) { Vc* r = nullptr; for (auto& x : v) if (pr(x) && (!r || x.age < r->age)) r = &x; return r; }
    void baseNoteOn(int n, double vel) {
        held.erase(std::remove(held.begin(), held.end(), n), held.end()); held.push_back(n); lastNote = n; lastVel = vel;
        Vc* x = nullptr; for (auto& y : v) if (y.note == n && y.active()) { x = &y; break; }
        if (!x) x = oldest([](const Vc& y) { return !y.active(); });
        if (!x) x = oldest([](const Vc& y) { return !y.gate; });
        if (!x) x = oldest([](const Vc&) { return true; });
        trig(*x, n, vel);
    }
    void baseNoteOff(int n) { held.erase(std::remove(held.begin(), held.end(), n), held.end()); for (auto& x : v) if (x.note == n && x.gate) release(x); }
    void allOff() override { held.clear(); for (auto& x : v) release(x); }

    void ctrl() {
        const double dt = CTV / fs; float* s = src.data();
        clock = advance(clock, g(iBpm), dt); const int ns = ((int) std::floor(clock)) & 15; const bool edge = ns != stp; stp = ns; const bool half = std::fmod(clock, 1.0) < .5;
        for (int k = 0; k < 6; ++k) s[5 + k] = p[(size_t) iS[k][stp]];
        const int ts = ri(iTrig);
        if (ts > 0) {
            const float sv = p[(size_t) iS[ts - 1][stp]]; const bool on = sv != 0 && half;
            if (edge && sv != 0) { bool anyGate = false; for (auto& x : v) if (x.gate) anyGate = true; if (held.empty() && !anyGate) trig(v[0], lastNote, lastVel); for (auto& x : v) if (x.gate || isHeld(x.note)) self().start(x); }
            if (seqGate && !on) for (auto& x : v) { self().stop(x); if (!isHeld(x.note)) x.gate = false; }
            seqGate = on;
        }
        for (int i = 0; i < 4; ++i) {
            lp[i] = lfoAdv(i, lp[i], val(iL[i][0]), dt); if (lp[i] >= 1) { lp[i] -= 1; lr[i] = rnd() * 2 - 1; } const double ph = lp[i];
            const double W[5] = { std::sin(TP * ph), 1 - 4 * std::fabs(ph - .5), 2 * ph - 1, ph < .5 ? 1.0 : -1.0, lr[i] }; s[i < 3 ? 1 + i : 15] = (float) W[ri(iL[i][1])];
        }
        shp += lfoHz(val(iShrate)) * dt; if (shp >= 1) { shp -= 1; sht = rnd() * 2 - 1; }
        const double sm = g(iShsm); shv += (sm <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm * .5))) * (sht - shv); s[4] = (float) shv;
        sh2p += lfoHz(val(iSh2rate)) * dt; if (sh2p >= 1) { sh2p -= 1; sh2t = rnd() * 2 - 1; }
        const double sm2 = g(iSh2sm); sh2v += (sm2 <= 0 ? 1 : 1 - std::exp(-dt / (.002 + sm2 * .5))) * (sh2t - sh2v); s[16] = (float) sh2v;
        Vc* l = &v[0]; for (auto& x : v) if (x.age > l->age) l = &x; lv = l; s[13] = (float) l->vel; s[14] = (float) ((l->note - 60) / 24.0); s[0] = 0;
        std::fill(off.begin(), off.end(), 0.f);
        for (int m = 0; m < 8; ++m) addMod(s, ri(iM[m][0]), ri(iM[m][1]), g(iM[m][2]));
        for (int k = 0; k < 6; ++k) addMod(s, 5 + k, ri(iSdst[k]), g(iSamt[k]));
        self().ectrl(dt);
    }
    static constexpr int CTV = 16;
    void process(float* L, float* R, int n) override {
        double* out = out2;
        for (int i = 0; i < n; ++i) {
            curI = i; if (mc % CTV == 0) ctrl(); mc++;
            self().render(out); ms += .004 * (val(iMaster) - ms); const double sl = out[0] * ms, sr = out[1] * ms;
            const double yl = sl - xl[0] + .9995 * dl[0]; xl[0] = sl; dl[0] = yl; const double yr = sr - xl[1] + .9995 * dl[1]; xl[1] = sr; dl[1] = yr;
            L[i] = (float) std::tanh(yl); R[i] = (float) std::tanh(yr);
        }
    }
};

// ---------------- J8 ----------------
struct J8Voice { int note = 60; bool gate = false; int age = 0; double curN = 60, tgtN = 60, vel = .8; poly::ADSR e1, e2; double e1S = 0, e2S = 0, p1 = 0, p2 = 0, s[4] {}, zs[4] {}, hp = 0, uoff = 0, tC = 0, tF = 0, tE = 0; bool active() const { return e2.st != 0; } };   // tC tF tE: this voice's tolerances (tuning, filter, envelopes)
struct J8 : PolyBase<J8, J8Voice> {
    static constexpr int OS = 2;
    static double envS(double k) { return .0015 * std::pow(6000.0, k); }
    static double jlfo(double k) { return .1 * std::pow(300.0, k); }
    static double glideS(double k) { return k <= 0 ? 0 : .003 * std::pow(2000.0, k); }
    poly::Dec dec { OS }; poly::Chorus ch; double chOut[2] {}, jph = 0, jdelT = 0, jr = 0, jl = 0;
    std::vector<int> arpNotes; double arpPh = 1; int arpIdx = -1, arpCur = -1, arpDir = 1;   // arpCur -1 = none
    struct TV { double tune = 0, mlfo = 0, menv = 0, pwm = 0, cross = 0, fine2 = 0, mix = 0, hpf = 0, freq = 0, res = 0, venv = 0, vlfo = 0, kybd = 0, valfo = 0, lvl = 0, udet = 0; } T, S; bool hasS = false;
    int r1 = 0, r2 = 0, w1 = 0, w2 = 0, md = 0, pws = 0, es = 0, chm = 0; bool sync = false, s24 = false; double pol = 1;

    double dk = 0;
    J8() { nv = 8; nsrc = 18; seed = 192837465; setupBase(j8::PDEFS, j8::DSTP, j8::NP); prepare(48000); }
    J8Voice makeVoice(int) { J8Voice x; x.p1 = rnd(); x.p2 = rnd(); x.tC = (rnd() - .5) * 2; x.tF = (rnd() - .5) * 2; x.tE = (rnd() - .5) * 2; return x; }
    void prepare(double sr) override {
        fs = sr; resetBase(); dec = poly::Dec(OS); ch.init(fs); chOut[0] = chOut[1] = 0; jph = jdelT = jr = jl = 0; dk = 1 - std::exp(-1 / (.001 * fs));
        arpNotes.clear(); arpPh = 1; arpIdx = -1; arpCur = -1; arpDir = 1; hasS = false;
    }
    void start(J8Voice& x) { x.e1.on(); x.e2.on(); }
    void stop(J8Voice& x) { x.e1.off(); x.e2.off(); }
    int mode() const { return ri(j8::assign); }
    void noteOn(int n, double vel) override {
        lastVel = vel;
        if (ri(j8::arp) > 0) { if (std::find(arpNotes.begin(), arpNotes.end(), n) == arpNotes.end()) { arpNotes.push_back(n); std::sort(arpNotes.begin(), arpNotes.end()); } if (arpNotes.size() == 1) { arpPh = 1; arpIdx = -1; jdelT = 0; } return; }
        const int m = mode(); bool anyGate = false; for (auto& x : v) if (x.gate) anyGate = true; if (!anyGate) jdelT = 0;
        if (m == 0) { baseNoteOn(n, vel); return; }
        held.erase(std::remove(held.begin(), held.end(), n), held.end()); held.push_back(n); lastNote = n;
        const int cnt = m == 1 ? (int) v.size() : 1; bool legato = false; for (int i = 0; i < cnt; ++i) if (v[(size_t) i].gate) legato = true;
        for (int i = 0; i < cnt; ++i) {
            auto& x = v[(size_t) i]; x.uoff = m == 1 ? (i - (cnt - 1) / 2.0) / ((cnt - 1) / 2.0) : 0;
            if (legato) { x.tgtN = n; x.note = n; x.gate = true; } else trig(x, n, vel);
        }
    }
    void noteOff(int n) override {
        if (ri(j8::arp) > 0) { arpNotes.erase(std::remove(arpNotes.begin(), arpNotes.end(), n), arpNotes.end()); if (arpNotes.empty() && arpCur >= 0) { baseNoteOff(arpCur); arpCur = -1; } return; }
        const int m = mode(); if (m == 0) { baseNoteOff(n); return; }
        held.erase(std::remove(held.begin(), held.end(), n), held.end()); const int cnt = m == 1 ? (int) v.size() : 1;
        if (!held.empty()) { const int last = held.back(); for (int i = 0; i < cnt; ++i) { v[(size_t) i].tgtN = last; v[(size_t) i].note = last; } }
        else for (int i = 0; i < cnt; ++i) release(v[(size_t) i]);
    }
    void arpTick(double dt) {
        const int am = ri(j8::arp);
        if (am == 0) { if (arpCur >= 0) { baseNoteOff(arpCur); arpCur = -1; } return; }
        if (arpNotes.empty()) return;
        const int DIV[4] = { 1, 2, 4, 8 }; const int div = DIV[ri(j8::arprate)]; arpPh += g(j8::bpm) / 60 * div * dt;
        if (arpPh >= .5 && arpCur >= 0 && arpPh < 1) { baseNoteOff(arpCur); arpCur = -1; }
        if (arpPh >= 1) {
            arpPh -= std::floor(arpPh);
            const int oct = ri(j8::arprng) + 1; std::vector<int> seq; for (int o = 0; o < oct; ++o) for (int nn : arpNotes) seq.push_back(nn + 12 * o);
            const int L_ = (int) seq.size(); int idx;
            if (am == 1) idx = (arpIdx + 1) % L_; else if (am == 2) idx = arpIdx <= 0 ? L_ - 1 : arpIdx - 1;
            else if (am == 3) { if (L_ < 2) idx = 0; else { idx = arpIdx + arpDir; if (idx >= L_) { arpDir = -1; idx = L_ - 2; } else if (idx < 0) { arpDir = 1; idx = 1; } } }
            else idx = (int) std::floor(rnd() * L_);
            if (am == 2 && arpIdx < 0) idx = L_ - 1; arpIdx = idx;
            if (arpCur >= 0) baseNoteOff(arpCur); arpCur = seq[(size_t) idx]; baseNoteOn(arpCur, lastVel);
        }
    }
    void ectrl(double dt) {
        using namespace j8; float* s = src.data();
        arpTick(dt);
        jph += jlfo(val(jrate)) * dt; if (jph >= 1) { jph -= 1; jr = rnd() * 2 - 1; } jdelT += dt;
        const double dly = g(jdelay) * 3, fade = dly <= 0 ? 1 : std::min(1.0, jdelT / dly), ph = jph;
        const double JW[4] = { std::sin(TP * ph), 2 * ph - 1, ph < .5 ? 1.0 : -1.0, jr }; jl = JW[ri(jwave)] * fade; s[17] = (float) jl;
        T.tune = val(tune) + bend; T.mlfo = val(j8::mlfo); T.menv = val(j8::menv); T.pwm = val(j8::pwm); T.cross = val(j8::cross); T.fine2 = val(j8::fine2); T.mix = val(j8::mix); T.hpf = val(j8::hpf);
        T.freq = val(j8::freq); T.res = val(j8::res); T.venv = val(j8::venv); T.vlfo = val(j8::vlfo); T.kybd = val(j8::kybd); T.valfo = val(j8::valfo); T.lvl = val(vcalvl); T.udet = val(j8::udet);
        static const int FEETS[6] = { -36, -24, -12, 0, 12, 24 };
        r1 = FEETS[ri(j8::r1)]; r2 = FEETS[ri(j8::r2)]; w1 = ri(j8::w1); w2 = ri(j8::w2); sync = g(j8::sync) > .5;
        md = ri(mdest); pws = ri(pwmsrc); s24 = ri(slope) == 1; es = ri(envsel);
        pol = ri(e1pol) ? -1 : 1; chm = ri(chorus);
        const double E1[4] = { envS(val(e1a)), envS(val(e1d)), val(e1s), envS(val(e1r)) }, E2[4] = { envS(val(e2a)), envS(val(e2d)), val(e2s), envS(val(e2r)) };
        const double k1 = g(e1kf), k2 = g(e2kf); const double gl = glideS(val(j8::glide)); const double ga = gl <= 0 ? 1 : 1 - std::exp(-dt / gl);
        for (auto& x : v) {
            if (!x.active() && !x.gate) continue; const double kn = (x.note - 60) / 24.0, f1 = std::pow(2.0, -k1 * kn), f2 = std::pow(2.0, -k2 * kn);
            { const double te = 1 + x.tE * vintage * .25; const double g1 = f1 * te, g2 = f2 * te;   // and its own envelope speed (up to 25 %)
              x.e1.tick(dt, E1[0] * g1, E1[1] * g1, E1[2], E1[3] * g1); x.e2.tick(dt, E2[0] * g2, E2[1] * g2, E2[2], E2[3] * g2); } x.curN += ga * (x.tgtN - x.curN);
        }
        s[11] = (float) lv->e1.v; s[12] = (float) lv->e2.v;
        if (!hasS) { S = T; hasS = true; }
    }
    void render(double* out) {
        using poly::th; using poly::blep;
        const double k = .003; double* Tp = &T.tune; double* Sp = &S.tune; for (int j = 0; j < 16; ++j) Sp[j] += k * (Tp[j] - Sp[j]);
        const double fsO = fs * OS; double acc[2] = { 0, 0 }; const double ma = std::cos(S.mix * PI / 2) * 1.2, mb = std::sin(S.mix * PI / 2) * 1.2, hpg = S.hpf > 0 ? 1 - std::exp(-TP * 20 * std::pow(100.0, S.hpf) / fsO) : 0;
        for (auto& x : v) {
            if (!x.active()) continue;
            x.e1S += dk * (x.e1.v - x.e1S); x.e2S += dk * (x.e2.v - x.e2S); /* envelopes step every CTV samples: smoothed per sample */ const double e1 = x.e1S * pol, e2 = x.e2S; const double vm = S.mlfo * jl * 2 + S.menv * e1 * 24;
            const double base = x.curN + S.tune + bendB + x.uoff * S.udet * .35 + x.tC * (.03 + vintage * .12);   // each voice its own tuning (VINTAGE: up to ~15 cents)
            const double n1 = base + r1 + (md != 1 ? vm : 0), n2 = base + r2 + S.fine2 + (md != 0 ? vm : 0);
            const double f1b = 440 * std::pow(2.0, (n1 - 69) / 12), f2 = 440 * std::pow(2.0, (n2 - 69) / 12), d2 = f2 / fsO;
            const double pw = cl(.5 - .46 * S.pwm * (pws == 0 ? (jl * .5 + .5) : pws == 1 ? 1 : x.e1S), .04, .96);
            const double ev = es == 0 ? e1 : e2;
            const double oct = S.freq * 9.97 + S.venv * ev * 7 + S.vlfo * jl * 2.5 + S.kybd * (x.curN - 60) / 12 + x.tC * .05 + x.tF * vintage * .25;   // and its own filter (up to a quarter octave)
            double fc = 20 * std::pow(2.0, oct); fc = std::min(fc, fsO * .22); double kc = 1; const double gg = s24 ? ladderG(fc, fsO, 4, &kc) : 1 - std::exp(-TP * fc / fsO); const double kq = S.res * (s24 ? 4.1 : 1.95) * kc; const double zg = zdfG(fc, fsO);
            const double amp = e2 * S.lvl * (1 - S.valfo * .5 * (1 - jl));
            for (int q = 0; q < OS; ++q) {
                x.p2 += d2; bool wrap2 = false; if (x.p2 >= 1) { x.p2 -= 1; wrap2 = true; } const double p2 = x.p2; double o2;
                if (w2 == 0) o2 = std::sin(TP * p2); else if (w2 == 1) o2 = 2 * p2 - 1 - blep(p2, d2);
                else if (w2 == 2) { double t = p2 - pw; if (t < 0) t += 1; o2 = (p2 < pw ? 1 : -1) + blep(p2, d2) - blep(t, d2); } else o2 = rnd() * 2 - 1;
                const double f1 = f1b * std::pow(2.0, S.cross * (w2 == 3 ? 0 : o2) * 3), d1 = std::fabs(f1) / fsO; x.p1 += d1; if (x.p1 >= 1) x.p1 -= 1; if (sync && wrap2) x.p1 = 0; const double p1 = x.p1; double o1;
                if (w1 == 0) o1 = 1 - 4 * std::fabs(p1 - .5); else if (w1 == 1) o1 = 2 * p1 - 1 - blep(p1, d1);
                else { const double w = w1 == 2 ? pw : .5; double t = p1 - w; if (t < 0) t += 1; o1 = (p1 < w ? 1 : -1) + blep(p1, d1) - blep(t, d1); }
                double xin = o1 * ma + o2 * mb;
                if (hpg != 0) { x.hp += hpg * (xin - x.hp); xin -= x.hp; }
                double* st = x.s;
                if (classicSound() || !s24) { const double fbk = s24 ? st[3] : st[1]; const double inp = th(xin * .5 - kq * fbk + (rnd() - .5) * 2e-4);
                    st[0] += gg * (inp - st[0]); st[1] += gg * (th(st[0]) - st[1]); if (s24) { st[2] += gg * (st[1] - st[2]); st[3] += gg * (th(st[2]) - st[3]); } }
                else zdfLadder<4>(x.zs, st, xin * .5 + (rnd() - .5) * 2e-4, zg, S.res * 4.1, [](double u) { return th(u); });   // 24 dB: the IR3109's four poles
                acc[q] += (s24 ? st[3] : st[1]) * amp;
            }
        }
        dec.push(acc[0]); dec.push(acc[1]); const double y = dec.out() * 1.1;
        if (chm > 0) { const double RR[4] = { 0, .513, .863, 9.75 }; const double C = chm == 3 ? 3.3 : 3.5, Dp = chm == 3 ? .25 : 1.85; ch.run(y, RR[chm], C, Dp, chOut); out[0] = y * .75 + chOut[0] * .75; out[1] = y * .75 + chOut[1] * .75; }
        else out[0] = out[1] = y;
    }
};

// ---------------- CS8 ----------------
struct ILAL {   // filter envelope: starts at IL, moves to AL during the attack, decays to the knob setting, releases to IL
    double v = 0; int st = 0;
    void on(double il) { v = il; st = 1; } void off() { if (st) st = 4; }
    double tick(double dt, double il, double al, double a, double d, double r) {
        if (st == 1) { v += (al - v) * (1 - std::exp(-dt / (a / 3))); if (std::fabs(al - v) < .01) st = 2; }
        else if (st == 2) v += (0 - v) * (1 - std::exp(-dt / (d / 4)));
        else if (st == 4) { v += (il - v) * (1 - std::exp(-dt / (r / 4))); }
        return v;
    }
};
struct CS8Layer { double ph = 0, h1 = 0, h2 = 0, l1 = 0, l2 = 0; ILAL fe; poly::ADSR ae; double hc[4] { 0, 0, 0, 2 }, lc[4] { 0, 0, 0, 2 }, amp = 0, ampS = 0, tP = 0, f = 0; bool hpOn = false; };
struct CS8Voice { int note = 60; bool gate = false; int age = 0; double curN = 60, tgtN = 60, vel = .8; CS8Layer L[2]; poly::ADSR re; bool active() const { return L[0].ae.st != 0 || L[1].ae.st != 0; } };
struct CS8 : PolyBase<CS8, CS8Voice> {
    static double envS(double k) { return .002 * std::pow(5000.0, k); }
    static double subHz(double k) { return .1 * std::pow(300.0, k); }
    static double ringHz(double k) { return .5 * std::pow(2000.0, k); }
    static double glideS(double k) { return k <= 0 ? 0 : .003 * std::pow(2000.0, k); }
    static void svfC(double fc, double fs, double res, double* o) { const double g = std::tan(PI * std::min(fc, fs * .45) / fs), k = 2 - 1.94 * res; const double a1 = 1 / (1 + g * (g + k)); o[0] = a1; o[1] = g * a1; o[2] = g * g * a1; o[3] = k; }
    poly::Chorus ch; double chOut[2] {}, pwmPh = 0, soPh = 0, soRnd = 0, so = 0, rmPh = 0, pwmL = 0, rmF = 0, rmD = 0; int chm = 0;
    struct LP { double feet, pw, pwm, sq, saw, sine, noise, hpf, hres, lpf, lres, il, al, fa, fd, fr, aa, ad, as, ar, brill, lvl; } Lp[2] {};
    int iq[2][22] {};

    CS8() {
        nv = 8; nsrc = 18; seed = 192837465; setupBase(cs8::PDEFS, cs8::DSTP, cs8::NP);
        const char* K[22] = { "feet", "pw", "pwm", "sq", "saw", "sine", "noise", "hpf", "hres", "lpf", "lres", "il", "al", "fa", "fd", "fr", "aa", "ad", "as", "ar", "brill", "lvl" };
        for (int c = 0; c < 2; ++c) for (int j = 0; j < 22; ++j) iq[c][j] = id(std::string(c ? "b" : "a") + K[j]);
        prepare(48000);
    }
    CS8Voice makeVoice(int) { CS8Voice x; for (auto& l : x.L) { l.ph = rnd(); l.tP = (rnd() - .5) * 2; } return x; }
    void prepare(double sr) override { fs = sr; resetBase(); ch.init(fs); chOut[0] = chOut[1] = 0; pwmPh = soPh = soRnd = so = rmPh = pwmL = rmF = rmD = 0; chm = 0; }
    void start(CS8Voice& x) { for (int c = 0; c < 2; ++c) { x.L[c].fe.on(val(iq[c][11])); x.L[c].ae.on(); } x.re.on(); }
    void stop(CS8Voice& x) { for (auto& l : x.L) { l.fe.off(); l.ae.off(); } x.re.off(); }
    void noteOn(int n, double vel) override { baseNoteOn(n, vel); }
    void noteOff(int n) override { baseNoteOff(n); }
    void ectrl(double dt) {
        using namespace cs8; float* s = src.data();
        pwmPh += lfoHz(val(pwmspd)) * 4 * dt; if (pwmPh >= 1) pwmPh -= 1; pwmL = std::sin(TP * pwmPh);
        soPh += subHz(val(sospd)) * dt; if (soPh >= 1) { soPh -= 1; soRnd = rnd() * 2 - 1; } const double ph = soPh;
        const double SO[5] = { std::sin(TP * ph), 2 * ph - 1, 1 - 2 * ph, ph < .5 ? 1.0 : -1.0, soRnd }; so = SO[ri(sofn)]; s[17] = (float) so;
        rmF = ringHz(val(rmspd)); rmD = val(rmdep);
        const double gl = glideS(val(porta)); const double ga = gl <= 0 ? 1 : 1 - std::exp(-dt / gl);
        const double tb = val(tibr), tl = val(tilvl), sovcf_ = val(sovcf), bendR = bendB * g(ribrng);
        for (int c = 0; c < 2; ++c) {
            const int* I = iq[c]; auto& P2 = Lp[c]; const double FEET[3] = { -12, 0, 12 };
            P2.feet = FEET[ri(I[0])]; P2.pw = val(I[1]); P2.pwm = val(I[2]); P2.sq = val(I[3]); P2.saw = val(I[4]); P2.sine = val(I[5]); P2.noise = val(I[6]); P2.hpf = val(I[7]); P2.hres = val(I[8]);
            P2.lpf = val(I[9]); P2.lres = val(I[10]); P2.il = val(I[11]); P2.al = val(I[12]); P2.fa = envS(val(I[13])); P2.fd = envS(val(I[14])); P2.fr = envS(val(I[15]));
            P2.aa = envS(val(I[16])); P2.ad = envS(val(I[17])); P2.as = val(I[18]); P2.ar = envS(val(I[19])); P2.brill = val(I[20]); P2.lvl = val(I[21]);
        }
        const double ra = envS(val(rmatt)), rd = envS(val(rmdec));
        for (auto& x : v) {
            if (!x.active() && !x.gate) continue; x.curN += ga * (x.tgtN - x.curN); x.re.tick(dt, ra, rd, 0, rd);
            const double touchB = tb * (x.vel - .5) * 2, touchL = 1 - tl + tl * x.vel;
            for (int c = 0; c < 2; ++c) {
                auto& l = x.L[c]; const auto& P2 = Lp[c];
                const double fe = l.fe.tick(dt, P2.il, P2.al, P2.fa, P2.fd, P2.fr); const double ae = l.ae.tick(dt, P2.aa, P2.ad, P2.as, P2.ar);
                const double oct = P2.lpf * 9.97 + fe * 5 + (P2.brill + touchB) * 2 + so * sovcf_ * 2; svfC(20 * std::pow(2.0, oct), fs, P2.lres, l.lc);
                l.hpOn = P2.hpf > 0; if (l.hpOn) svfC(20 * std::pow(200.0, P2.hpf) * std::pow(2.0, (P2.brill + touchB) * .5), fs, P2.hres, l.hc);
                l.amp = ae * P2.lvl * touchL * (1 - val(sovca) * .5 * (1 - so));
                l.f = 440 * std::pow(2.0, (x.curN + P2.feet + (val(tune) + bend) + bendR + (c ? val(det) * .5 : 0) + l.tP * (.04 + vintage * .1) + so * val(sovco) * 1.2 - 69) / 12);
            }
        }
        s[11] = (float) lv->L[0].fe.v; s[12] = (float) lv->L[0].ae.v; chm = ri(chorus);
    }
    void render(double* out) {
        using poly::th; using poly::blep;
        double y = 0; rmPh += rmF / fs; if (rmPh >= 1) rmPh -= 1; const double rsn = std::sin(TP * rmPh);
        for (auto& x : v) {
            if (!x.active()) continue; double vs = 0;
            for (int c = 0; c < 2; ++c) {
                auto& l = x.L[c]; const auto& P2 = Lp[c]; l.ampS += .01 * (l.amp - l.ampS); if (l.ampS < 1e-5 && l.amp <= 0) continue;
                const double dt = l.f / fs; l.ph += dt; if (l.ph >= 1) l.ph -= 1; const double pp = l.ph;
                double xs = 0; if (P2.saw != 0) xs += (2 * pp - 1 - blep(pp, dt)) * P2.saw;
                if (P2.sq != 0) { const double pw = cl(P2.pw + P2.pwm * pwmL * .42, .04, .96); double t2 = pp - pw; if (t2 < 0) t2 += 1; xs += ((pp < pw ? 1 : -1) + blep(pp, dt) - blep(t2, dt)) * P2.sq; }
                if (P2.sine != 0) xs += std::sin(TP * pp) * P2.sine; if (P2.noise != 0) xs += (rnd() * 2 - 1) * P2.noise * .7;
                if (l.hpOn) { const double a1 = l.hc[0], a2 = l.hc[1], a3 = l.hc[2], k = l.hc[3]; const double v3 = xs - l.h2, v1 = a1 * l.h1 + a2 * v3, v2 = l.h2 + a2 * l.h1 + a3 * v3; l.h1 = 2 * v1 - l.h1; l.h2 = 2 * v2 - l.h2; xs = xs - k * v1 - v2; }
                { const double a1 = l.lc[0], a2 = l.lc[1], a3 = l.lc[2]; const double xi = th(xs * .6) * 1.5; const double v3 = xi - l.l2, v1 = a1 * l.l1 + a2 * v3, v2 = l.l2 + a2 * l.l1 + a3 * v3; l.l1 = 2 * v1 - l.l1; l.l2 = 2 * v2 - l.l2; xs = v2; }
                vs += xs * l.ampS;
            }
            const double rd = rmD * x.re.v; vs = vs * (1 - rd) + vs * rsn * rd;
            y += vs;
        }
        y *= .32;
        if (chm > 0) { const double R = chm == 1 ? .55 : 6.2, C = chm == 1 ? 3.6 : 3, Dp = chm == 1 ? 1.8 : .35; ch.run(y, R, C, Dp, chOut); out[0] = y * .75 + chOut[0] * .75; out[1] = y * .75 + chOut[1] * .75; }
        else out[0] = out[1] = y;
    }
};

} // namespace oct
