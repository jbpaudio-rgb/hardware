// Octavio FX: the 16 effects (state, per-block values, audio). Port of fx-web/dsp.js initFx / fxCtrl / fx.
#pragma once
namespace oct {

inline void FxUnit::initFx(int f) {
    const double fs_ = fs;
    switch (f) {
    case 16: scE = 0; scR = false; scSt = -1; scK = 0; break;   // SIDECHAIN
        case 0: case 1:
            for (auto& s : od) s = ODS(); cEnv = gEnv = 0; gG = 1;
            for (auto& a : phA) for (auto& x : a) x = 0; for (auto& a : phB) for (auto& x : a) x = 0;
            phYB[0] = phYB[1] = phCB[0] = phCB[1] = 0; phL1 = phL2 = 0; phY[0] = phY[1] = 0; phL = 0; phC[0] = phC[1] = 0; break;
        case 2: for (auto& d : flD) d.init(iround(fs_ * .03)); flL = 0; flC[0] = flC[1] = fs_ * .002; flA = 0; flAt = 1; flE = 0; break;
        case 3: for (auto& b : chB) b.init(iround(fs_ * .04)); chL = chRT = chRV = 0; break;
        case 4: for (auto& s : mpS) s.init(iround(fs_ * .1)); for (auto& d : mpD) d.init(iround(fs_ * .25)); mpY[0] = mpY[1] = 0; mpRt = mpRv = 0; mpRc = 0; mpT[0] = mpT[1] = 0; break;
        case 5:
            for (auto& d : dlD) d.init(iround(fs_ * 3.2)); dlT[0] = dlT[1] = fs_ * .3; dlF[0] = dlF[1] = dlH[0] = dlH[1] = dlB[0] = dlB[1] = 0; dlU = fs_ * .1;
            dlP = dlW = dlWf = 0; dlRc = 0; dlRs = 0; for (auto& s : dlX) s.init(iround(fs_ * .1)); dlRv.init(fs_, 4); dlRo[0] = dlRo[1] = 0; break;
        case 6: rvF.init(fs_); rvSp.clear(); rvSm = -1; for (auto& d : rvPd) d.init(iround(fs_ * .42)); for (auto& x : rvIn) x = 0; rvO[0] = rvO[1] = 0; rvW = 0; rvD[0] = rvD[1] = 0; break;
        case 7:
            grB.init(iround(fs_ * 3.4)); for (int k = 0; k < 16; ++k) { grP[k] = grR[k] = grL[k] = grT[k] = grG[k] = 0; grA[k] = 0; } grC = grS = grY = 0; grF[0] = grF[1] = SVF(); break;
        case 8: for (auto& d : fkD) d.init(iround(fs_ * .07)); fkF[0] = fkF[1] = fkR[0] = fkR[1] = SVF(); fkY[0] = fkY[1] = 0; break;
        case 9: case 10:
            bcH[0] = bcH[1] = 0; bcC = 0; rmP = 0; rmL = 0; bcF[0] = bcF[1] = SVF(); bcE = 0; bcG = 1; for (auto& x : bcSeq) x = 1;
            for (auto& d : bcB) d.init(iround(fs_ * .6)); bcSt = false; bcSs = 0; bcSi = 0; bcSl = 1; bcDr = 0; bcDg = 1; for (auto& z : bcZ) z = 0; rmH[0] = rmH[1] = Hilbert(); break;
        case 11:
            for (auto& d : stB) d.init(iround(fs_ * 4.2)); stOn = false; stStart = 0; stLen = 1; stLenT = 0; stI = 0; stX = 0; stRep = 0; stPos = 0; stG = -1; stGt = 0; stPrevOn = false; stHold = 0; break;
        case 12: for (auto& d : reB) d.init(iround(fs_ * 4.5)); reH[0] = Head(); reH[1] = Head(); reH[1].half = true; reL = 0; reF[0] = reF[1] = 0; reHold = -1; break;
        case 13:
            for (auto& d : fzH) d.init(iround(fs_ * 1.2));
            for (auto& L : fzLy) { L.b[0].assign((size_t) iround(fs_ * .8), 0.f); L.b[1].assign((size_t) iround(fs_ * .8), 0.f); L.n = 0; L.env = 0; L.st = 0; L.h[0] = Head(); L.h[1] = Head(); L.h[0].d = L.h[1].d = 1; L.h[0].r = L.h[1].r = 0; L.h[1].half = true; }
            fzOn = false; fzE = fzT = 0; fzR = 1; fzF[0] = fzF[1] = SVF(); fzNext = 0; break;
        case 14:
            for (auto& d : tpR) d.init(iround(fs_ * .012)); for (auto& d : tpD) d.init(iround(fs_ * .56)); for (auto& d : tpC) d.init(iround(fs_ * .03));
            tpW = tpF = tpF2 = tpN = tpWb = tpWt = 0; tpLg = -1; for (int c = 0; c < 2; ++c) { tpY[c] = tpYl[c] = tpT[c] = tpL1[c] = tpL2[c] = tpHP[c] = 0; tpB[c] = tpH[c] = SVF(); } break;
        case 15:
            shF.init(fs_); shW.init(fs_); shSp[0].init(fs_, 33, 12); shSp[1].init(fs_, 41, 12); for (auto& d : shPd) d.init(iround(fs_ * .52));
            for (auto& v3 : shS) for (auto& s : v3) s.init(iround(fs_ * .12)); for (int c = 0; c < 2; ++c) { shY[c] = shO[c] = shWo[c] = shL[c] = shH[c] = 0; shLF[c] = shOF[c] = SVF(); } shRp = 0; break;
    }
}

inline void FxUnit::fxCtrl(int f, double dt, double bpm, double beat) {
    using namespace fxp;
    const double env_ = R->env; const bool edge = R->edge; const int step = R->step;
    auto rv = [&](int l) { return iround(P(l)); };
    switch (f) {
    case 16: {   // SIDECHAIN: source, key, rate, the 16 steps, attack, decay, amount
        QSC Q; Q.src = rv(L::f16src); Q.key = rv(L::f16key); Q.rate = .25 * std::pow(2.0, std::clamp(rv(L::f16rate), 0, 4)); Q.bpm = bpm;
        Q.mask = 0; for (int k = 0; k < 16; ++k) if (P(L::f16st0 + k) > .5) Q.mask |= 1 << k;
        const double att = V(L::f16att) * 300 * fs / 1000, dec = 10 * std::pow(200.0, V(L::f16dec)) * fs / 1000;
        Q.up = 1 / std::max(1.0, att); Q.down = std::exp(-4.6 / std::max(1.0, dec)); Q.amt = V(L::f16amt);
        SC = Q; scBeat = R->clock / 4; scK = 0;   // the beat now (the rack's clock counts sixteenths); fx() runs on from it
        break;
    }
    case 0: {
        const int md = rv(L::f0model); auto a = [&](double hz) { return 1 - std::exp(-TP * hz / fs); }; QOD Q; Q.md = md;
        if (md == 0) {
            const double dr = V(L::f0vdrive); Q.g = 1 + dr * dr * (P(L::f0vx10) > .5 ? 160 : 24); Q.b = V(L::f0vbias) * .7; Q.t = rv(L::f0vtype); Q.aL = a(140); Q.aH = a(2800);
            Q.lo = dbg(V(L::f0vlow) * 9) - 1; Q.hi = dbg(V(L::f0vhigh) * 9) - 1; Q.out = dbg((V(L::f0vout) - .5) * 24) * .55 / (1 + dr * .6);
            Q.y0 = Q.t == 0 ? triode(Q.b) : Q.t == 1 ? 2 / PI * std::atan(Q.b * 1.8) : std::max(-1.0, std::min(1.0, Q.b * 1.5));
        } else if (md == 1) {
            const double sat = V(L::f0hsat), den = V(L::f0hdens); Q.gs = 1 + sat * sat * 14; Q.gd = 1 + den * 5; Q.den = den; Q.alt = P(L::f0halt) > .5; Q.aA = a(9000);
            Q.air = dbg(V(L::f0hair) * 10) - 1; Q.bl = V(L::f0hblend); Q.out = dbg((V(L::f0hout) - .5) * 24) * .5 / (1 + sat * .5 + den * .3); Q.mg = std::tan(PI * 1200 / fs);
        } else {
            const double dr = V(L::f0bdrive), fv = V(L::f0bfilt); const double fc = fv < 0 ? 18000 * std::pow(2.0, fv * 7) : 20 * std::pow(2.0, fv * 8);
            Q.g = 1 + dr * dr * 30; Q.m = rv(L::f0bmode); Q.comp = V(L::f0bcomp); Q.thr = dbg(-V(L::f0bcomp) * 30); Q.fg = std::tan(PI * std::min(fc, fs * .45) / fs);
            Q.hp = fv > .03; Q.lp = fv < -.03; Q.gate = V(L::f0bgate) * .12; Q.out = dbg((V(L::f0bout) - .5) * 24) * .6 / (1 + dr * .5);
        }
        OD = Q; break;
    }
    case 1: {
        const int pm = rv(L::f1model); double hz = 0, fmin = 0, oct_ = 0, fb = 0, drive = 0; int n = 0;
        if (pm == 0) { hz = .08 * std::pow(300.0, V(L::f1mrate)); n = rv(L::f1mstages) ? 12 : 6; fb = V(L::f1mres) * .9; fmin = 110; oct_ = V(L::f1msweep) * 6.2; drive = V(L::f1mdrive); }
        else if (pm == 1) { hz = .06 * std::pow(140.0, V(L::f1xspeed)); const int it = rv(L::f1xint); const int NN[4] = { 4, 6, 8, 10 }; const double FB[4] = { .12, .3, .45, .62 }; n = NN[it]; fb = FB[it]; fmin = 230; oct_ = 4.4; }
        else if (pm == 2) { hz = .02 * std::pow(500.0, V(L::f1irate)); n = 8; fb = V(L::f1ifb) * .9; fmin = 55; oct_ = 7.2; }
        else {
            const double st = V(L::f1stereo); const int sh[2] = { rv(L::f1bshape1), rv(L::f1bshape2) };
            phL1 += .03 * std::pow(400.0, V(L::f1brate1)) * dt; phL2 += .03 * std::pow(400.0, V(L::f1brate2)) * dt; phL1 -= std::floor(phL1); phL2 -= std::floor(phL2);
            auto osc = [&](int k, double ph) { const double s = std::sin(TP * ph); return .5 + .5 * (sh[k] ? std::tanh(s * 4) / std::tanh(4.0) : s); };
            const int srcB = rv(L::f1bsrcb); auto coef = [&](double fr) { const double t = std::tan(PI * cl(fr, 20, fs * .45) / fs); return (t - 1) / (t + 1); };
            for (int ch = 0; ch < 2; ++ch) {
                const double o = ch * st * .5; const double la = osc(0, phL1 + o), lb = srcB ? osc(1, phL2 + o) : osc(0, phL1 + o);
                phC[ch] = coef(90 * std::pow(2.0, 6 * (.5 + (la - .5) * V(L::f1bdepa)))); phCB[ch] = coef(90 * std::pow(2.0, 6 * (.5 + (lb - .5) * V(L::f1bdepb))));
            }
            QPH Q; Q.bi = true; Q.fa = V(L::f1bfba) * .88; Q.fbb = V(L::f1bfbb) * .88; Q.par = rv(L::f1broute) == 1; Q.w = V(L::f1width) * 2; PH = Q;
        }
        if (pm != 3) {
            phL += hz * dt; phL -= std::floor(phL); const int sm = rv(L::f1isrc); const double stv = V(L::f1stereo);
            for (int ch = 0; ch < 2; ++ch) {
                const double ph = phL + ch * stv * .5; double l;
                if (pm == 1) { const double t = 1 - 2 * std::fabs(std::fmod(ph, 1.0) - .5); l = std::pow(t, 1.6); } else l = .5 + .5 * std::sin(TP * ph);
                if (pm == 2) { const double dep = V(L::f1idepth); l = sm == 0 ? .5 + (l - .5) * dep + (V(L::f1iman) - .5) * (1 - dep) : sm == 1 ? cl(V(L::f1iman) * .3 + env_ * V(L::f1isens) * 6 * dep, 0, 1) : V(L::f1iman); if (sm != 0 && ch) l = l + (1 - 2 * l) * stv; }
                const double fr = cl(fmin * std::pow(2.0, oct_ * l), 20, fs * .45); const double t = std::tan(PI * fr / fs); phC[ch] = (t - 1) / (t + 1);
            }
            QPH Q; Q.fb = fb; Q.n = n; Q.drive = drive; Q.w = V(L::f1width) * 2; PH = Q;
        }
        break;
    }
    case 2: {
        const int fm = rv(L::f2model); const double ms = fs / 1000, st = V(L::f2stereo), env = std::min(1.0, env_ * 3); QFL Q; Q.m = fm; Q.w = V(L::f2width) * 2;
        double hz = 0, lo = 1, hi = 1, pos[2] = { 0, 0 };
        auto sinp = [&](int k) { return .5 + .5 * std::sin(TP * (flL + k * st * .5)); };
        auto trip = [&](int k) { double q_ = flL + k * st * .5; q_ -= std::floor(q_); return 1 - 2 * std::fabs(q_ - .5); };
        if (fm == 0) {
            hz = .03 * std::pow(300.0, V(L::f2urate)); const double c = .5 + V(L::f2utime) * 11.5, a = V(L::f2uamt); lo = c * (1 - a * .9); hi = c * (1 + a * 1.5);
            Q.fb = V(L::f2ufb) * .93; Q.dr = V(L::f2udrive); const int sq = rv(L::f2ushape);
            for (int k = 0; k < 2; ++k) { const double s = sinp(k); pos[k] = sq ? .5 + .5 * std::tanh((s - .5) * 12) / std::tanh(6.0) : s; }
        } else if (fm == 1) {
            hz = .02 * std::pow(500.0, V(L::f2irate)); lo = .05; hi = 8; Q.fb = V(L::f2ifb) * .9; Q.tz = P(L::f2itzf) > .5; const int sm = rv(L::f2isrc); const double dep = V(L::f2idepth), man = V(L::f2iman);
            for (int k = 0; k < 2; ++k) { double l = sm == 0 ? .5 + (sinp(k) - .5) * dep + (man - .5) * (1 - dep) : sm == 1 ? cl(man * .3 + env * V(L::f2isens) * 2 * dep, 0, 1) : man; if (sm != 0 && k) l = l + (1 - 2 * l) * st; pos[k] = l; }
        } else if (fm == 2) {
            hz = .03 * std::pow(300.0, V(L::f2hrate)); lo = .2; hi = 6.5; Q.fb = V(L::f2hres) * .9 * (rv(L::f2hpol) ? -1 : 1); Q.pol = rv(L::f2hpol) ? -1 : 1; Q.bl = V(L::f2hblend);
            const double dep = V(L::f2hdepth), man = V(L::f2hman); for (int k = 0; k < 2; ++k) pos[k] = cl(man + (trip(k) - .5) * dep, 0, 1);
        } else if (fm == 3) {
            hz = .03 * std::pow(300.0, V(L::f2srate)); lo = .3; hi = 9; Q.fb = V(L::f2sregen) * .92; const int md = rv(L::f2smode); const double dep = V(L::f2sdepth), man = V(L::f2sman), e = env * V(L::f2ssens) * 2;
            for (int k = 0; k < 2; ++k) { const double lf = (sinp(k) - .5) * dep; pos[k] = cl(man + (md == 0 ? lf : md == 1 ? e * dep : lf * .6 + e * dep * .7), 0, 1); }
        } else {
            hz = .03 * std::pow(300.0, V(L::f2aspeed)); lo = .25; hi = .25 + 8.75 * V(L::f2arange); Q.fb = V(L::f2aenh) * .97; const int md = rv(L::f2amode); const double man = V(L::f2aman);
            if (md == 2) { const double th = .02 + V(L::f2athr) * .5; if (env > th && flE <= th && flAt >= 1) flAt = 0; flE = env; if (flAt < 1) flAt += hz * dt; flA = flAt < 1 ? std::sin(PI * flAt) : 0; }
            for (int k = 0; k < 2; ++k) pos[k] = md == 0 ? trip(k) : md == 1 ? man : cl(man + (1 - man) * flA, 0, 1);
        }
        flL += hz * dt; flL -= std::floor(flL);
        for (int k = 0; k < 2; ++k) Q.d[k] = cl((lo * std::pow(hi / lo, pos[k])) * ms, 1, fs * .028);
        Q.c = 4 * ms; FL = Q; break;
    }
    case 3: {
        const int cm = rv(L::f3model); const double ms = fs / 1000; auto ap = [&](double hz) { return 1 - std::exp(-TP * hz / fs); }; double hz = 0; QCH C_; C_.m = cm; C_.w = V(L::f3width) * 2;
        if (cm == 0) { hz = .1 * std::pow(90.0, V(L::f3crate)); C_.base = 1.6 * ms; C_.dep = V(L::f3cdepth) * 4.2 * ms; C_.ap = ap(7000); }
        else if (cm == 1) { const int md = rv(L::f3dmode); const double HZ[4] = { .25, .25, .5, .5 }, DP[4] = { .35, .7, .8, 1.4 }; hz = HZ[md]; C_.base = 3.2 * ms; C_.dep = DP[md] * ms; C_.ap = ap(9000); }
        else if (cm == 2) { hz = .05 * std::pow(120.0, V(L::f3trate)); C_.base = 5 * ms; C_.dep = V(L::f3tdepth) * 6 * ms; C_.ap = ap(3000 * std::pow(4.0, V(L::f3ttone))); }
        else if (cm == 4) { const int md = std::min(2, rv(L::f3dmode)); const double HZ[3] = { .513, .863, 9.75 }; hz = HZ[md]; C_.base = (md == 2 ? 3.3 : 3.5) * ms; C_.dep = (md == 2 ? .25 : 1.85) * ms; C_.ap = ap(5200); }   // J8 (the Jupiter-8 / Juno BBD): I, II, I+II
        else { hz = .05 * std::pow(200.0, V(L::f3jrate)); C_.base = (1.5 + V(L::f3jlag) * 13.5) * ms; C_.dep = V(L::f3jdepth) * 5 * ms; C_.ap = ap(10000); const double dv = V(L::f3jdpv); C_.dg = dv < .5 ? 1 : (1 - dv) * 2; C_.wg = dv < .5 ? dv * 2 : 1; C_.ph = V(L::f3jphase) * .5; C_.sh = rv(L::f3jshape); }
        chL += hz * dt; if (chL >= 1) { chL -= 1; chRT = rnd(); } if (cm == 3 && C_.sh == 2) chRV += (chRT - chRV) * std::min(1.0, hz * dt * 6);
        CH = C_; break;
    }
    case 4: {
        const int hm = rv(L::f4model); const double ms = fs / 1000;
        const double pitch = hm ? V(L::f4npitch) : V(L::f4tpitch), det = hm ? V(L::f4ndet) : V(L::f4tdet), del = hm ? V(L::f4ndel) : V(L::f4tdel), fbv = hm ? V(L::f4nfb) : V(L::f4tfb);
        const double st = jround((pitch - .5) * 24), dc = det * 50; QMP Q; Q.m = hm;
        Q.r[0] = std::pow(2.0, (st * 100 + dc) / 1200); Q.r[1] = std::pow(2.0, (st * 100 - dc) / 1200); Q.del = std::max(1.0, del * (hm ? 200 : 112.5) * ms); Q.fb = fbv * .85;
        Q.w = V(L::f4width) * 2; Q.win = jround((hm ? .06 : .04) * fs); Q.rand = hm ? V(L::f4nrand) * 15 * ms : 0;
        if (Q.rand != 0) { if (++mpRc >= iround(fs * .18 / CT)) { mpRc = 0; mpRt = rnd(); } mpRv += (mpRt - mpRv) * .05; }
        MP = Q; break;
    }
    case 5: {
        const int dm = rv(L::f5model); const bool sync = P(L::f5sync) > .5; auto ap = [&](double hz) { return 1 - std::exp(-TP * hz / fs); };
        auto T = [&](double v_) { return cl((sync ? NOTEB[iround(v_ * 12)] * 60 / bpm : .02 * std::pow(75.0, v_)) * fs, 8, fs * 3); }; QDL Q; Q.m = dm;
        auto heads = [&](std::initializer_list<int> h) { Q.nh = 0; for (int x : h) Q.h[Q.nh++] = x; };
        if (dm == 0) { Q.t[0] = T(V(L::f5pta)); Q.t[1] = T(V(L::f5ptb)); Q.fb = V(L::f5pfb) * .95; Q.mr = .05 * std::pow(120.0, V(L::f5pmr)); Q.md = V(L::f5pmd) * .004 * fs; Q.lp = ap(1500 * std::pow(8.0, V(L::f5ptone))); }
        else if (dm == 1) {
            const double t = T(V(L::f5ltime)); Q.type = rv(L::f5ltype); Q.t[0] = t; Q.t[1] = Q.type == 1 ? t * .75 : t; Q.fb = V(L::f5lrep) * .97; Q.mr = .6; Q.md = V(L::f5lmod) * .0025 * fs;
            const double fv = V(L::f5lfilt); Q.lp = fv < 0 ? ap(16000 * std::pow(2.0, fv * 6)) : ap(16000); Q.hp = fv > 0 ? ap(20 * std::pow(2.0, fv * 8)) : 0; Q.grit = V(L::f5lgrit);
        } else if (dm == 2) {
            Q.t[0] = T(V(L::f5xtl)); Q.t[1] = T(V(L::f5xtr)); Q.fb = V(L::f5xfb) * .92; const double st = jround((V(L::f5xpitch) - .5) * 24), dc = V(L::f5xdet) * 50;
            Q.r[0] = std::pow(2.0, (st + dc / 100) / 12); Q.r[1] = std::pow(2.0, (st - dc / 100) / 12); Q.lp = ap(15600);
        } else if (dm == 3) { const double t = T(V(L::f5stime)); Q.t[0] = Q.t[1] = t; Q.fb = V(L::f5sfb) * .95; Q.mr = .05 * std::pow(120.0, V(L::f5srate)); Q.md = V(L::f5smod) * .004 * fs; Q.lp = ap(16000 * std::pow(2.0, -V(L::f5sdamp) * 4)); Q.dr = V(L::f5sdrive); }
        else if (dm == 4) {
            Q.u = fs * (.12 - .07 * V(L::f5espeed));
            static const std::vector<std::vector<int>> H = { { 1 }, { 2 }, { 3 }, { 4 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 1, 3 }, { 2, 4 }, { 1, 2, 3, 4 } };
            const auto& hh = H[(size_t) rv(L::f5eheads)]; Q.nh = 0; for (int x : hh) Q.h[Q.nh++] = x;
            Q.mode = rv(L::f5emode); Q.fb = V(L::f5eswell) * .9; const double tn = V(L::f5etone); Q.lp = ap(5200 * std::pow(2.0, tn)); Q.wow = .15;
        } else if (dm == 5) { Q.u = fs * (.06 + V(L::f5htime) * .69); heads({ 1 }); Q.mode = 1; Q.fb = V(L::f5hsus) * 1.05; Q.lp = ap(6500); Q.pre = V(L::f5hpre); Q.wow = V(L::f5hwow); }
        else if (dm == 7) {
            Q.u = fs * (.11 - V(L::f5cspeed) * .06); const int CHN[5] = { L::f5ch1, L::f5ch2, L::f5ch3, L::f5ch4, L::f5ch5 }; Q.nh = 0;
            for (int k = 1; k <= 5; ++k) if (P(CHN[k - 1]) > .5) Q.h[Q.nh++] = k; if (!Q.nh) heads({ 1 });
            Q.mode = 2; Q.fb = V(L::f5cswell) * .8; Q.lp = ap(5000 * std::pow(2.0, V(L::f5ctone))); Q.pre = V(L::f5cdrive); Q.wow = V(L::f5cwow);
        } else {
            Q.u = fs * (.18 - V(L::f5rrate) * .12);
            static const std::vector<std::vector<int>> H = { { 1 }, { 2 }, { 3 }, { 2, 3 }, { 1, 3 }, { 1, 2 }, { 1, 2, 3 } };
            const auto& hh = H[(size_t) rv(L::f5rmode)]; Q.nh = 0; for (int x : hh) Q.h[Q.nh++] = x;
            Q.mode = 2; Q.fb = V(L::f5rint) * .82; Q.lp = ap(4200); Q.bass = V(L::f5rbass); Q.treb = V(L::f5rtreb); Q.rev = V(L::f5rrev); Q.wow = V(L::f5rwow); Q.aB = ap(250); Q.aT = ap(2500);
        }
        DLq = Q; break;
    }
    case 6: {
        const int rm = rv(L::f6model); const RvMod& M = RVMOD()[(size_t) rm]; auto ap = [&](double hz) { return 1 - std::exp(-TP * std::min(hz, fs * .45) / fs); };
        QRV Q; Q.m = rm; Q.M = &M; Q.w = V(L::f6width) * 2;
        if (M.k == 1) {
            if (rvSm != rm) { rvSm = rm; rvSp.assign(M.sp.size(), Spring()); for (size_t k = 0; k < M.sp.size(); ++k) rvSp[k].init(fs, M.sp[k], M.st); }
            Q.dr = 1 + V(L::f6sdwell) * 6 * M.dr; Q.g = .62 + V(L::f6sdecay) * .35; Q.a = ap(M.lp * std::pow(2.0, V(L::f6stone) * 1.3)); Q.c = M.c; Q.wob = M.wob * .004;
        } else {
            double rt, pre, tone = 0, mic = .5, size = M.size, damp = M.damp, low = M.low, mod = M.mod;
            if (rm <= 3) { rt = .3 * std::pow(40.0, V(L::f6ddecay)); pre = V(L::f6dpre); size *= .5 + V(L::f6dsize); damp *= std::pow(2.0, -V(L::f6ddamp) * 3); low = .5 * std::pow(4.0, V(L::f6dlow)); mod *= V(L::f6dmod) * 2; }
            else if (rm <= 13) { rt = .8 * std::pow(6.0, V(L::f6cdecay)) * M.rt; pre = V(L::f6cpre); tone = V(L::f6ctone); mic = V(L::f6cmic); }
            else if (rm <= 18) { rt = .5 * std::pow(12.0, V(L::f6pdecay)); pre = V(L::f6ppre); tone = V(L::f6ptone); }
            else { rt = .2 * std::pow(12.0, V(L::f6odecay)) * M.rt; pre = V(L::f6opre); tone = V(L::f6otone); mic = V(L::f6odist); }
            damp *= std::pow(2.0, tone * 1.5); rvF.set(size, rt * 1.35, low, damp, M.diff, mod);
            Q.pre = pre * .25 * fs; const bool er = !M.er.empty(); Q.erG = er ? 1.1 - mic * .8 : 0; Q.lateG = er ? .55 + mic * .7 : 1; Q.bw = ap(M.bw * std::pow(2.0, tone)); Q.hp = ap(M.hp);
        }
        RV = Q; break;
    }
    case 7: {
        const int gm = rv(L::f7model); QGR Q; Q.m = gm; Q.len = iround(fs * .015 * std::pow(20.0, V(L::f7size))); Q.st = iround((V(L::f7pitch) - .5) * 24); Q.spray = V(L::f7spray) * .5 * fs; Q.frz = P(L::f7freeze) > .5;
        const double RT[5] = { .125, .25, .5, 1, 2 }; Q.intv = gm == 3 ? RT[iround(V(L::f7rate) * 4)] * 60 / bpm * fs : fs / (1 + V(L::f7dens) * 39); Q.sync = gm == 3;
        Q.pos = gm == 3 ? .05 * fs : V(L::f7pos) * 1.6 * fs; Q.win = gm == 1 || gm == 2 ? V(L::f7tex) : gm == 3 ? V(L::f7contour) : .5; Q.rev = gm == 1 ? V(L::f7rev) : 0; Q.fb = gm == 1 ? V(L::f7fb) * .8 : 0;
        Q.scan = gm == 2 ? V(L::f7scan) : 0; Q.spread = gm == 2 ? V(L::f7spread) : .7; Q.warp = gm == 3 ? V(L::f7warp) : 0; Q.pat = rv(L::f7pat); Q.dr = gm == 0 ? V(L::f7drive) : 0;
        const double fv = gm == 0 || gm == 2 ? V(L::f7filt) : 0; const double fc = fv < 0 ? 18000 * std::pow(2.0, fv * 7) : 20 * std::pow(2.0, fv * 8);
        Q.fg = std::tan(PI * std::min(fc, fs * .45) / fs); Q.lp = fv < -.03; Q.hp = fv > .03;
        grS += Q.scan * dt * .4; grS -= std::floor(grS); GR = Q; break;
    }
    case 8: {
        const int fm = rv(L::f8model); QFK Q; Q.m = fm;
        if (fm == 0) {
            const double fr = 30 * std::pow(70.0, V(L::f8rtune)), sp = V(L::f8rspread) * 50 / 1200; Q.d2[0] = fs / (fr * std::pow(2.0, sp)); Q.d2[1] = fs / (fr * std::pow(2.0, -sp));
            const double F0 = fr * std::pow(2.0, sp), F1 = fr * std::pow(2.0, -sp); Q.f[0] = std::tan(PI * std::min(F0, fs * .45) / fs); Q.f[1] = std::tan(PI * std::min(F1, fs * .45) / fs);
            Q.fb = V(L::f8rfb) * 1.02; Q.dr = V(L::f8rdrive); Q.res = rv(L::f8rmode) == 1; const double fv = V(L::f8rtone); const double fc = fv < 0 ? 16000 * std::pow(2.0, fv * 6) : 20 * std::pow(2.0, fv * 9);
            Q.tg = std::tan(PI * std::min(fc, fs * .45) / fs); Q.lp = fv < -.03; Q.hp = fv > .03;
        } else { Q.d1 = fs * (1 + V(L::f8tloop) * 59) / 1000; Q.fb = P(L::f8thold) > .5 ? 1.6 : V(L::f8tfb) * 1.35; Q.bg = std::tan(PI * (250 * std::pow(24.0, V(L::f8ttone))) / fs); Q.vol = V(L::f8tvol) * .6; Q.kill = P(L::f8tkill) > .5; }
        FK = Q; break;
    }
    case 9: {
        const int bm = rv(L::f9model); QBC Q; Q.m = bm; Q.q = std::pow(2.0, 16 - V(L::f9crush) * 14 - 1); Q.div = 1 + std::pow(V(L::f9rate), 2) * 49;
        if (bm == 0) { const double fv = V(L::f9bfilt); const double fc = fv < 0 ? 18000 * std::pow(2.0, fv * 7) : 20 * std::pow(2.0, fv * 8); Q.fg = std::tan(PI * std::min(fc, fs * .45) / fs); Q.lp = fv < -.03; Q.hp = fv > .03; Q.gate = V(L::f9bgate) * .15; }
        else if (bm == 1) {
            Q.fg = std::tan(PI * std::min(200 * std::pow(90.0, V(L::f9ofilt)), fs * .45) / fs); Q.lp = true; Q.k = .7;
            if (edge) {
                if (step == 0) for (int k = 0; k < 16; ++k) bcSeq[k] = (float) std::pow(2.0, (rnd() * 2 - 1) * 3);
                if (rnd() < V(L::f9ostut)) { bcSt = true; bcSl = std::max(32, iround(60 / bpm * fs / 8)); bcSs = bcB[0].w - bcSl; bcSi = 0; } else bcSt = false;
            }
            Q.div *= std::pow((double) bcSeq[step], V(L::f9oseq));
        } else if (bm == 2) { Q.vol = V(L::f9jvol) * 2; Q.fg = std::tan(PI * 5000 / fs); Q.lp = P(L::f9jfilt) > .5; }
        else { Q.jit = V(L::f9djit); Q.noise = V(L::f9dnoise) * .02; Q.drop = V(L::f9ddrop); Q.drive = V(L::f9ddrive); if (Q.drop > 0 && rnd() < Q.drop * .05) bcDr = iround(fs * (.005 + rnd() * .04)); }
        BC = Q; break;
    }
    case 10: {
        const int rm = rv(L::f10model); const bool lohi = rm == 0 || rm == 2; const bool lo = lohi && rv(L::f10range) == 0; const double v_ = V(L::f10freq);
        double fr = lo ? .5 * std::pow(160.0, v_) : lohi ? 30 * std::pow(133.0, v_) : 15 * std::pow(200.0, v_);
        QRM Q; Q.m = rm; Q.sq = (rm == 0 || rm == 3) && rv(L::f10wave) == 1;
        if (rm == 0) { rmL += .03 * std::pow(300.0, V(L::f10rlrate)) * dt; Q.lfo = V(L::f10rldepth); }
        else if (rm == 1) { fr *= std::pow(2.0, V(L::f10xfine) / 12); Q.mode = rv(L::f10xmode); Q.sp = V(L::f10xspread); }
        else if (rm == 2) { rmL += .03 * std::pow(300.0, V(L::f10mlrate)) * dt; Q.lfo = V(L::f10mlamt); Q.lsq = rv(L::f10lshape) == 1; Q.drive = V(L::f10mdrive); }
        else Q.vol = V(L::f10jvol) * 2;
        rmL = rmL - std::floor(rmL);
        if (Q.lfo != 0) { const double s = std::sin(TP * rmL); const double l = Q.lsq ? std::tanh(s * 6) : s; fr *= std::pow(2.0, l * Q.lfo * 2); }
        Q.f = fr; RM = Q; break;
    }
    case 11: {
        const int sm = rv(L::f11model); const double DIV[6] = { 2, 1, .5, .25, .125, .0625 }; const bool on = P(L::f11on) > .5, live = true; const double maxL = stB[0].n - 16; QST Q; Q.m = sm; Q.gate = V(L::f11gate);
        auto Ln = [&](double v_) { return std::min(maxL, std::max(64.0, jround(DIV[iround(v_ * 5)] * beat))); };
        if (sm == 0) {
            double len = Ln(V(L::f11rrate)); if (P(L::f11rtrip) > .5) len = jround(len * 2 / 3); Q.len = len; Q.pitch = (V(L::f11rpitch) - .5) * 4; Q.fade = V(L::f11rfade);
            if (edge && live) {
                const bool rnd_ = rv(L::f11rmode) == 1; bool want = on;
                if (rnd_) { if (stHold > 0) stHold--; else { want = on || rnd() < V(L::f11rchance); stHold = want ? 3 : 0; } if (stHold > 0) want = true; }
                if (want && !stOn) { stOn = true; stStart = stB[0].w - len; stI = 0; stPos = 0; stRep = 0; stLen = len; } else if (!want) stOn = false;
            }
        } else {
            const double DUR[6] = { .5, 1, 2, 4, 8, 16 }; const double durB = DUR[iround(V(L::f11sdur) * 5)]; const int mode = rv(L::f11smode);
            Q.l0 = Ln(V(L::f11sstart)); Q.l1 = Ln(V(L::f11send)); Q.cv = V(L::f11scurve); Q.dur = durB * beat; Q.pitch = 0; Q.fade = 0;
            if (edge && live) {
                const bool start = mode == 2 ? (step == 0 && on) : (on && !stPrevOn) || (mode == 1 && on && stG < 0);
                if (mode == 2 && step == 0 && (on || rnd() < .5)) stG = 0; else if (start && mode != 2) stG = 0;
                if (stG == 0 && !stOn) { stOn = true; stStart = stB[0].w - Q.l0; stI = 0; stPos = 0; stRep = 0; stLen = Q.l0; stGt = 0; }
                if (!on && mode != 2) { stG = -1; stOn = false; } stPrevOn = on;
            }
        }
        ST = Q; break;
    }
    case 12: {
        const int rm = rv(L::f12model); QRE Q; Q.m = rm; Q.T = jround(fs * .08 * std::pow(25.0, V(L::f12time)));
        if (rm == 0) { double s = (V(L::f12tspeed) - .5) * 4; if (std::fabs(s) < .13) s = s < 0 ? -.13 : .13; Q.rate = s; Q.rand = V(L::f12trand); Q.hold = P(L::f12thold) > .5; Q.fb = 0; }
        else if (rm == 1) { Q.fb = V(L::f12fb) * .85; Q.md = V(L::f12smod) * .004 * fs; Q.mr = .6; Q.a = 1 - std::exp(-TP * 1200 * std::pow(12.0, V(L::f12stone)) / fs); }
        else { Q.fb = V(L::f12fb) * .85; Q.md = V(L::f12dtweak) * .006 * fs; Q.mr = .1 * std::pow(60.0, V(L::f12dtweez)); Q.a = 1; }
        RE = Q; break;
    }
    case 13: {
        const int zm = rv(L::f13model); auto ft = [](double v_) { return .005 * std::pow(600.0, v_); }; auto k = [&](double t) { return 1 - std::exp(-dt / t); };
        QFZ Q; Q.m = zm; Q.on = P(L::f13on) > .5;
        if (zm == 0) {
            Q.n = jround(fs * .003 * std::pow(40.0, V(L::f13wsize))); Q.G = Q.n; Q.rate = std::pow(2.0, jround((V(L::f13wpitch) - .5) * 24) / 12); Q.att = k(.01); Q.rel = k(.05); Q.dec = 1; Q.lvl = 1; Q.single = true;
            const double fv = V(L::f13wtone); const double fc = fv < 0 ? 16000 * std::pow(2.0, fv * 6) : 20 * std::pow(2.0, fv * 9); Q.tg = std::tan(PI * std::min(fc, fs * .45) / fs); Q.lp = fv < -.03; Q.hp = fv > .03;
            fzR += (Q.rate - fzR) * (1 - std::exp(-dt / (.001 + V(L::f13wglide) * 1.5)));
        } else if (zm == 1) { Q.n = jround(fs * .35); Q.G = jround(fs * .12); Q.att = k(ft(V(L::f13pin))); Q.rel = k(ft(V(L::f13pout))); Q.dec = 1; Q.lvl = V(L::f13plevel) * 1.4; Q.single = true; Q.autom = rv(L::f13pmode) == 1; fzR = 1; }
        else { Q.n = jround(fs * .35); Q.G = jround(fs * .12); Q.att = k(ft(V(L::f13xrise))); Q.rel = k(ft(V(L::f13xtail))); const double sv = V(L::f13xsus); Q.dec = sv > .97 ? 1 : std::exp(-dt / (1 + 29 * sv * sv)); Q.lvl = V(L::f13xlevel) * 1.4; Q.single = rv(L::f13xmode) == 0; fzR = 1; }
        // triggers: the FREEZE switch, or (DEEP FREEZE in AUTO) each new note
        const double env = std::min(1.0, env_ * 3); bool cap = false, rel = false;
        if (Q.autom) { fzT += dt; if (env > .05 && env > fzE * 1.8 + .01 && fzT > .15) { cap = true; fzT = 0; } fzE += (env - fzE) * k(.3); if (Q.on && !fzOn) cap = true; }
        else { if (Q.on && !fzOn) cap = true; if (!Q.on && fzOn) rel = true; }
        fzOn = Q.on;
        if (cap) {
            if (Q.single) for (auto& Ly : fzLy) if (Ly.st == 1) Ly.st = 2;
            Layer& Ly = fzLy[fzNext]; fzNext = (fzNext + 1) % 3;
            const int n = (int) std::min(Q.n, (double) Ly.b[0].size());
            for (int i = 0; i < n; ++i) { Ly.b[0][(size_t) i] = (float) fzH[0].at(fzH[0].w - n + i); Ly.b[1][(size_t) i] = (float) fzH[1].at(fzH[1].w - n + i); }
            Ly.n = n; Ly.st = 1; Ly.env = 0; for (auto& h : Ly.h) { h.c = 0; h.go = false; }
        }
        if (rel) for (auto& Ly : fzLy) if (Ly.st == 1) Ly.st = 2;
        FZ = Q; break;
    }
    case 14: {
        const int tm = rv(L::f14model); auto op = [&](double fr) { return 1 - std::exp(-TP * fr / fs); }; auto db = [](double v_) { return std::pow(10.0, (v_ - .5) * 24 / 20); }; QTP Q; Q.m = tm;
        if (tm == 0) {
            Q.son = P(L::f14dson) > .5; Q.dbl = P(L::f14ddbl) > .5; Q.g = .5 * std::pow(10.0, V(L::f14dsat) * 1.2); Q.out = db(V(L::f14dvol)) / (Q.son ? std::pow(Q.g, .6) : 1); Q.t = V(L::f14dtone); Q.ta = op(700);
            const double lv = V(L::f14dlag); const double ms = lv < .3 ? -4 + lv / .3 * 14 : 10 * std::pow(50.0, (lv - .3) / .7); Q.ref = fs * .0045; Q.lag = Q.ref + ms * fs / 1000;
            Q.wob = V(L::f14dwob) * fs * .0016; const double b = V(L::f14dblend); Q.gr = std::min(1.0, 2 - 2 * b); Q.gl = std::min(1.0, 2 * b); Q.ty = rv(L::f14dtype); Q.s = Q.ty == 1 ? -1 : 1; Q.fb = Q.ty == 2 ? .5 : 0;
            Q.wide = P(L::f14dwide) > .5; Q.nm = 1 / std::max(1.0, (Q.gr + Q.gl) * .75); Q.la = op(9000);
        } else if (tm == 1) {
            const int ch = rv(L::f14ctype); Q.g = .6 * std::pow(10.0, V(L::f14cdrive) * 1.1); Q.h = ch ? 1 : .8; Q.out = 1 / std::pow(Q.g, .55); Q.wow = V(L::f14cwow); Q.fl = V(L::f14cflut); Q.hiss = V(L::f14chiss) * (ch ? .012 : .018);
            Q.t = V(L::f14ctone); Q.ta = op(900); Q.wear = V(L::f14cwear); Q.la = op((ch ? 13500 : 9000) * (1 - .65 * Q.wear)); Q.ha = op(45);
        } else {
            const int sp = rv(L::f14aspeed), tp = rv(L::f14atape); const double b = V(L::f14abias); Q.gi = .25 * std::pow(16.0, V(L::f14ain)); Q.out = db(V(L::f14aout)) / std::pow(Q.gi, .55);
            const double TPH[4] = { .75, 1, 1.3, 1.6 }, SPH[3] = { .85, 1, 1.1 }, HF[3] = { 13000, 18000, 22000 }, BF[3] = { 45, 70, 110 }, BM[3] = { .45, .32, .22 }, HS[3] = { 1.4, 1, .7 };
            Q.h = TPH[tp] * SPH[sp] * (1 + .35 * b); const double hf = std::min(HF[sp] * (1 - .35 * b), fs * .45); Q.lg = std::tan(PI * hf / fs);
            Q.bg = std::tan(PI * BF[sp] / fs); Q.bk = .8; Q.bm = BM[sp] * .8; Q.hiss = V(L::f14ahiss) * .006 * HS[sp]; Q.as = .05 + .1 * std::max(0.0, -b); Q.ha = op(18);
        }
        TPq = Q; break;
    }
    case 15: {
        const int sm = rv(L::f15model); auto op = [&](double fr) { return 1 - std::exp(-TP * fr / fs); }; QSH Q; Q.m = sm; Q.wl = 0; FDN& F = shF;
        auto setv = [&](std::initializer_list<double> l) { Q.nv = 0; for (double x : l) Q.v[Q.nv++] = x; };
        if (sm == 0) {
            const double rt = std::pow(40.0, V(L::f15bdecay)), damp = 6000 * std::pow(3.0, V(L::f15btone)); Q.pre = V(L::f15bpre) * .5 * fs; F.set(1.25, rt, 1.1, damp, .7, V(L::f15bmod) * 1.5);
            switch (rv(L::f15bpitch)) { case 0: setv({ 2 }); break; case 1: setv({ 1.5 }); break; case 2: setv({ 2, 1.5 }); break; case 3: setv({ .5 }); break; default: setv({ 2, .5 }); }
            Q.amt = V(L::f15bamt); Q.la = op(damp); Q.out = .9;
        } else if (sm == 1) {
            const double rt = std::pow(40.0, V(L::f15alen)); Q.pre = .02 * fs; F.set(1.5, rt, 1.2, 7000, .75, 0); F.mr = (.08 + 3 * V(L::f15arate)) / fs; F.md = V(L::f15adepth) * .004 * fs;
            switch (rv(L::f15amode)) { case 0: setv({ 2 }); break; case 1: setv({ .5 }); break; case 2: setv({ 2, .5 }); break; case 3: setv({ 1.5, 2 }); break; case 4: setv({ 4 }); break; default: setv({ 2, 1.5, .5 }); }
            Q.amt = V(L::f15ashim); Q.la = op(9000); Q.out = .95;
            shRp += ((P(L::f15aramp) > .5 ? 1 : 0) - shRp) * (1 - std::exp(-dt / 1.2)); if (shRp > 1e-3) for (int j = 0; j < Q.nv; ++j) Q.v[j] = std::pow(Q.v[j], 1 - shRp);
            const double fv = V(L::f15afilt); Q.fl = fv < -.03 ? 1 : fv > .03 ? 2 : 0; Q.fg = std::tan(PI * std::min(fv < 0 ? 16000 * std::pow(2.0, fv * 5) : 20 * std::pow(2.0, fv * 6), fs * .45) / fs);
        } else {
            const double rt = std::pow(40.0, V(L::f15kdecay)), damp = 5000 * std::pow(3.0, V(L::f15ktone)); Q.pre = V(L::f15kpre) * .5 * fs; F.set(1.3, rt, 1.1, damp, .72, .6);
            setv({ 2 }); Q.amt = V(L::f15kmod); Q.la = op(damp); Q.out = .6; const double dw = V(L::f15kdwell); Q.dw = .6 + 4 * dw; Q.dwn = std::tanh(Q.dw); Q.wl = V(L::f15kworld); Q.wt = rv(L::f15kworldt); Q.rt = rv(L::f15kroute);
            const double wr = rt * .6; if (Q.wt == 0) shW.set(1.6, wr, 1.3, damp * 1.2, .7, .5); else if (Q.wt == 1) shW.set(.7, wr, 1.0, damp * 2, .8, .2);
            else { Q.sg[0] = std::pow(10.0, -3 * 33.0 / 1000 / std::max(.3, wr)); Q.sg[1] = std::pow(10.0, -3 * 41.0 / 1000 / std::max(.3, wr)); Q.sa = op(std::min(damp, 5000.0)); }
        }
        Q.gn = 1 / std::sqrt((double) Q.nv); Q.ha = op(180); Q.win = jround(fs * .1); SH = Q; break;
    }
    }
}

inline void FxUnit::fx(int f, double l, double r, double* o) {
    switch (f) {
    case 16: {   // SIDECHAIN: ducks on another part (its sound, its notes, one of its drum tracks / pads), on an LFO (a
                 // smooth pump on the tempo, deepest on the beat) or on a 16-step sequence; attack, decay, amount
        const QSC& Q = SC; double& e = scE; const int i = R->curI;
        if (Q.src >= 4) {
            const double pos = (scBeat + (double) scK++ * Q.bpm / 60 / fs) * Q.rate;
            if (Q.src == 4) e = .5 + .5 * std::cos(TP * (pos - std::floor(pos)));
            else {
                const int st = (int) std::floor(pos) & 15;
                if (st != scSt) { scSt = st; if ((Q.mask >> st) & 1) scR = true; }
                if (scR) { e += Q.up; if (e >= 1) { e = 1; scR = false; } } else e *= Q.down;
            }
        } else {
            const SideKey* S = R->side; const int p = Q.src;
            if (!S || i >= S->n) e *= Q.down;
            else if (Q.key == 0) {   // the part's sound: a level follower, full ducking at about -12 dB
                const double x = std::min(1.0, (std::fabs((double) S->L[p][i]) + std::fabs((double) S->R[p][i])) * 2);
                e = x > e ? e + (x - e) * std::min(1.0, 3 * Q.up) : x + (e - x) * Q.down;
            } else {
                if ((Q.key == 1 && i == 0 && S->note[p]) || (Q.key >= 2 && S->hit[p][std::min(Q.key - 2, 7)] == i)) scR = true;
                if (scR) { e += Q.up; if (e >= 1) { e = 1; scR = false; } } else e *= Q.down;
            }
        }
        const double g = 1 - Q.amt * e; o[0] = l * g; o[1] = r * g;
        break;
    }
    case 0: {
        const QOD& Q = OD;
        if (Q.md == 0) {
            for (int ch = 0; ch < 2; ++ch) {
                auto& S = od[ch]; const double u = (ch ? r : l) * Q.g + Q.b;
                auto curve = [&Q](double u) { double y;
                    if (Q.t == 0) y = triode(u); else if (Q.t == 1) y = 2 / PI * std::atan(u * 1.8);
                    else { y = std::max(-1.0, std::min(1.0, u * 1.5)); const double gt = std::fabs(Q.b) * .6; if (gt > 0 && std::fabs(u - Q.b) < gt) y = Q.b * 1.5 + (y - Q.b * 1.5) * std::fabs(u - Q.b) / gt; }
                    return y; };
                double y = classicSound() ? curve(u) : adaa(curve, S.ax, u); S.ax = u;
                y -= Q.y0; const double dc = y - S.dx + .995 * S.dy; S.dx = y; S.dy = dc; y = dc;
                const double lo = S.lo.lp(y, Q.aL), hi = y - S.hi.lp(y, Q.aH); y += lo * Q.lo + hi * Q.hi; o[ch] = y * Q.out;
            }
        } else if (Q.md == 1) {
            for (int ch = 0; ch < 2; ++ch) {
                auto& S = od[ch]; const double x = ch ? r : l; double w = x;
                if (Q.alt) { S.mid.run(w, Q.mg, .7); w += S.mid.bpo * 1.5; }
                auto curve = [&Q](double w) { w = std::tanh(w * Q.gs); const double t = w * Q.gd; return t / (1 + std::fabs(t)) + .18 * Q.den * t * t / (1 + t * t); };
                { const double w0 = w; w = classicSound() ? curve(w) : adaa(curve, S.ax, w); S.ax = w0; }
                const double dc = w - S.dx + .995 * S.dy; S.dx = w; S.dy = dc; w = dc; w += (w - S.air.lp(w, Q.aA)) * Q.air;
                o[ch] = (x + (w * 1.3 - x) * Q.bl) * Q.out;
            }
        } else {
            const double lvl = std::max(std::fabs(l), std::fabs(r)); gEnv += (lvl > gEnv ? .02 : .0004) * (lvl - gEnv); const double gt = gEnv > Q.gate || Q.gate <= 0 ? 1 : 0; gG += (gt - gG) * (gt != 0 ? .01 : .0006);
            double y[2];
            for (int ch = 0; ch < 2; ++ch) {
                const double u = (ch ? r : l) * Q.g;
                auto curve = [&Q](double u) { double s;
                    if (Q.m == 0) s = std::tanh(u); else if (Q.m == 1) s = u > 0 ? 1 - std::exp(-u) : -(1 - std::exp(u * 1.6)) / 1.6; else { s = std::tanh(u * 6); s = sgn(s) * std::pow(std::fabs(s), .5); }
                    return s; };
                y[ch] = classicSound() ? curve(u) : adaa(curve, od[ch].ax, u); od[ch].ax = u;
            }
            const double e = std::max(std::fabs(y[0]), std::fabs(y[1])); cEnv += (e > cEnv ? .004 : .00012) * (e - cEnv); double cg = 1; if (cEnv > Q.thr) cg = std::pow(Q.thr / cEnv, .75); cg *= 1 + Q.comp * 2;
            for (int ch = 0; ch < 2; ++ch) {
                auto& S = od[ch]; double s = y[ch] * cg; S.f.run(s, Q.fg, .9); if (Q.lp) s = S.f.lpo; else if (Q.hp) s = S.f.hpo;
                const double dc = s - S.dx + .995 * S.dy; S.dx = s; S.dy = dc; o[ch] = dc * Q.out * gG;
            }
        }
        break;
    }
    case 1: {
        const QPH& Q = PH;
        if (Q.bi) {
            auto ap = [](double x, float* st, double c) { for (int k = 0; k < 6; ++k) { const double z = c * x + st[k]; st[k] = (float) (x - c * z); x = z; } return x; };
            for (int ch = 0; ch < 2; ++ch) {
                const double x = ch ? r : l; double ya = x + std::tanh(phY[ch] * Q.fa); ya = ap(ya, phA[ch], phC[ch]); phY[ch] = ya; const double a = .5 * (x + ya);
                const double xb = Q.par ? x : a; double yb = xb + std::tanh(phYB[ch] * Q.fbb); yb = ap(yb, phB[ch], phCB[ch]); phYB[ch] = yb; const double b = .5 * (xb + yb);
                o[ch] = Q.par ? (ch ? b : a) : b;
            }
        } else {
            for (int ch = 0; ch < 2; ++ch) {
                double x = ch ? r : l; if (Q.drive > 0) x = std::tanh(x * (1 + Q.drive * 6)) / (1 + Q.drive * 2);
                double y = x + std::tanh(phY[ch] * Q.fb); float* st = phA[ch]; const double c = phC[ch];
                for (int k = 0; k < Q.n; ++k) { const double z = c * y + st[k]; st[k] = (float) (y - c * z); y = z; }
                phY[ch] = y; o[ch] = .5 * (x + y);
            }
        }
        const double mid = .5 * (o[0] + o[1]), sd = .5 * (o[0] - o[1]) * Q.w; o[0] = mid + sd; o[1] = mid - sd; break;
    }
    case 2: {
        const QFL& Q = FL;
        for (int ch = 0; ch < 2; ++ch) {
            double x = ch ? r : l; if (Q.dr != 0) x = std::tanh(x * (1 + Q.dr * 5)) / (1 + Q.dr * 1.5); DL& dl = flD[ch]; flC[ch] += (Q.d[ch] - flC[ch]) * .02;
            double y, dry = x;
            if (Q.tz) { y = dl.get(Q.c + (flC[ch] - 4 * fs / 1000) * 1.0); dry = dl.get(Q.c); dl.put(x + std::tanh(y * Q.fb)); o[ch] = .5 * (dry - y); }
            else { y = dl.get(flC[ch]); dl.put(x + std::tanh(y * Q.fb)); o[ch] = (dry * (1 - Q.bl * .5) + Q.pol * y * Q.bl * .5); }
        }
        const double mid = .5 * (o[0] + o[1]), sd = .5 * (o[0] - o[1]) * Q.w; o[0] = mid + sd; o[1] = mid - sd; break;
    }
    case 3: {
        const QCH& C_ = CH; auto tri = [](double ph) { ph -= std::floor(ph); return 1 - 2 * std::fabs(ph - .5); }; const double m = .5 * (l + r); const double ph = chL;
        if (C_.m == 0) { const double y = chB[0].run(m, C_.base + C_.dep * tri(ph), C_.ap); o[0] = o[1] = .5 * (m + y) * 1.3; }
        else if (C_.m == 1) { const double a = chB[0].run(m, C_.base + C_.dep * tri(ph), C_.ap), b = chB[1].run(m, C_.base + C_.dep * (1 - tri(ph)), C_.ap); o[0] = (m + .7 * a - .3 * b) * .75; o[1] = (m + .7 * b - .3 * a) * .75; }
        else if (C_.m == 4) { const double tw = 2 * tri(ph) - 1, a = chB[0].run(m, C_.base + C_.dep * tw, C_.ap), b = chB[1].run(m, C_.base - C_.dep * tw, C_.ap); o[0] = (m + a) * .75; o[1] = (m + b) * .75; }
        else if (C_.m == 2) {
            double y[3]; for (int k = 0; k < 3; ++k) y[k] = chB[k].run(m, C_.base + C_.dep * (.5 + .5 * std::sin(TP * (ph + k / 3.0))), C_.ap);
            o[0] = .45 * m + .65 * y[0] + .35 * y[1]; o[1] = .45 * m + .65 * y[2] + .35 * y[1];
        } else {
            auto lf = [&](int k) { const double q_ = ph + k * C_.ph; return C_.sh == 0 ? .5 + .5 * std::sin(TP * q_) : C_.sh == 1 ? tri(q_) : (k ? 1 - chRV : chRV); };
            for (int ch = 0; ch < 2; ++ch) { const double x = ch ? r : l; const double y = chB[ch].run(x, C_.base + C_.dep * lf(ch), C_.ap); o[ch] = (x * C_.dg + y * C_.wg) / (C_.dg + C_.wg); }
        }
        const double mid = .5 * (o[0] + o[1]), sd = .5 * (o[0] - o[1]) * C_.w; o[0] = mid + sd; o[1] = mid - sd; break;
    }
    case 4: {
        const QMP& Q = MP; const double m = .5 * (l + r);
        for (int ch = 0; ch < 2; ++ch) {
            const double t = Q.del + (Q.rand != 0 ? Q.rand * (ch ? 1 - mpRv : mpRv) : 0); mpT[ch] += (t - mpT[ch]) * .002; DL& dl = mpD[ch]; dl.put(m + mpY[ch] * Q.fb); const double x = dl.get(mpT[ch]);
            double y = Q.m ? mpS[ch].run(x, Q.r[ch], Q.win) : mpS[ch].runEdge(x, Q.r[ch], Q.win, .06); if (!Q.m) y = qz(y, 4096); mpY[ch] = y; o[ch] = y;
        }
        const double mid = .5 * (o[0] + o[1]), sd = .5 * (o[0] - o[1]) * Q.w; o[0] = mid + sd; o[1] = mid - sd; break;
    }
    case 5: {
        const QDL& Q = DLq; DL* B = dlD; double* F = dlF;
        if (Q.m <= 3) {
            for (int c = 0; c < 2; ++c) dlT[c] += (Q.t[c] - dlT[c]) * .0004; double md0 = 0, md1 = 0;
            if (Q.md != 0) { dlP += Q.mr / fs; dlP -= std::floor(dlP); md0 = std::sin(TP * dlP) * Q.md; md1 = -md0; }
            if (Q.m == 0) { const double m = .5 * (l + r); const double a = B[0].get(dlT[0] + md0), b = B[0].get(dlT[1] + md1); F[0] += Q.lp * (a - F[0]); B[0].put(qz(m + F[0] * Q.fb, 4096)); o[0] = a; o[1] = b; }
            else if (Q.m == 1) {
                if (Q.type == 3) {
                    const int T_ = std::max(64, iround(dlT[0])); if (++dlRc >= T_) { dlRc = 0; dlRs = B[0].w; } const int c = dlRc, c2 = (c + T_ / 2) % T_;
                    for (int ch = 0; ch < 2; ++ch) {
                        DL& b = B[ch]; const double y = b.at(dlRs - 1 - 2.0 * c) * hann((double) c / T_) + b.at(dlRs - 1 - 2.0 * c2 + T_) * hann((double) c2 / T_);
                        F[ch] += Q.lp * (y - F[ch]); b.put((ch ? r : l) + F[ch] * Q.fb * .8); o[ch] = y;
                    }
                } else {
                    double y[2] = { B[0].get(dlT[0] + md0), B[1].get(dlT[1] + md1) };
                    for (int ch = 0; ch < 2; ++ch) {
                        double s = y[ch]; F[ch] += Q.lp * (s - F[ch]); s = F[ch]; if (Q.hp != 0) { dlH[ch] += Q.hp * (s - dlH[ch]); s -= dlH[ch]; }
                        if (Q.grit > 0) { s = std::tanh(s * (1 + Q.grit * 4)) / (1 + Q.grit); const double qq = std::pow(2.0, 12 - Q.grit * 7); s = jround(s * qq) / qq; }
                        y[ch] = s;
                    }
                    const bool pp = Q.type == 2; B[0].put(l + (pp ? y[1] : y[0]) * Q.fb); B[1].put((pp ? 0 : r) + (pp ? y[0] : y[1]) * Q.fb);
                    o[0] = B[0].get(dlT[0] + md0); o[1] = B[1].get(dlT[1] + md1);
                    if (Q.type == 1) { const double o0 = .6 * o[0] + .4 * B[1].get(dlT[1]); const double o1 = .6 * o[1] + .4 * B[0].get(dlT[1]); o[0] = o0; o[1] = o1; }
                }
            } else if (Q.m == 2) {
                for (int ch = 0; ch < 2; ++ch) { const double y = B[ch].get(dlT[ch]); const double s = dlX[ch].run(y, Q.r[ch], jround(fs * .06)); F[ch] += Q.lp * (s - F[ch]); B[ch].put((ch ? r : l) + F[ch] * Q.fb); o[ch] = s; }
            } else {
                for (int ch = 0; ch < 2; ++ch) { const double u = (ch ? r : l) * (1 + Q.dr * 6); const double x = (classicSound() ? std::tanh(u) : adaa([](double v) { return std::tanh(v); }, dlA[ch], u)) / (1 + Q.dr * 2); dlA[ch] = u; const double y = B[ch].get(dlT[ch] + (ch ? md1 : md0)); F[ch] += Q.lp * (y - F[ch]); B[ch].put(x + F[ch] * Q.fb); o[ch] = y; }
            }
            break;
        }
        // tape: one loop, several playback heads, wow and flutter, saturation on record, darker and thinner repeats
        dlU += (Q.u - dlU) * .00025; dlW += .55 / fs; dlWf += 6.3 / fs; dlW -= std::floor(dlW); dlWf -= std::floor(dlWf);
        const double wob = 1 + Q.wow * (.004 * std::sin(TP * dlW) + .0012 * std::sin(TP * dlWf)); const double m = .5 * (l + r); double inx = m;
        if (Q.pre != 0) { const double u = m * (1 + Q.pre * 3); inx = (classicSound() ? std::tanh(u) : adaa([](double v) { return std::tanh(v); }, dlA[2], u)) / (1 + Q.pre * .8); dlA[2] = u; inx += Q.pre * .3 * (inx - dlB[1]); dlB[1] = inx; }
        const int n = Q.nh; double sl = 0, sr = 0, last = 0, sum = 0;
        for (int k = 0; k < n; ++k) { const double y = B[0].get(Q.h[k] * dlU * wob); const double pa = n > 1 ? (double) k / (n - 1) : .5; sl += y * (1 - pa * .7); sr += y * (.3 + pa * .7); sum += y; last = y; }
        const double fbs = Q.mode == 0 ? 0 : Q.mode == 1 ? last : sum / std::sqrt((double) n); F[0] += Q.lp * (fbs - F[0]); dlH[0] += .004 * (F[0] - dlH[0]);
        { const double u = (inx + (F[0] - dlH[0]) * Q.fb) * 1.2; B[0].put((classicSound() ? std::tanh(u) : adaa([](double v) { return std::tanh(v); }, dlA[3], u)) / 1.2); dlA[3] = u; }   // the tape's saturation on record
        double ol = sl / std::sqrt((double) n), orr = sr / std::sqrt((double) n);
        if (!classicSound()) {   // the playback head's bump: a couple of dB around 60-150 Hz, as magnetic heads do
            const double a1 = 1 - std::exp(-TP * 150 / fs), a2 = 1 - std::exp(-TP * 60 / fs);
            dlHB[0] += a1 * (ol - dlHB[0]); dlHB[1] += a2 * (ol - dlHB[1]); dlHB[2] += a1 * (orr - dlHB[2]); dlHB[3] += a2 * (orr - dlHB[3]);
            ol += .3 * (dlHB[0] - dlHB[1]); orr += .3 * (dlHB[2] - dlHB[3]);
        }
        if (Q.m == 6) {
            dlB[0] += Q.aB * (ol - dlB[0]); const double lo = dlB[0]; dlH[1] += Q.aT * (ol - dlH[1]); const double hi = ol - dlH[1];
            auto g = [&](double x) { return x + lo * Q.bass * .6 + hi * Q.treb * .6; }; ol = g(ol); orr = g(orr);
            if (Q.rev > 0) { dlRv.run(l, r, .8, .35, dlRo); ol += dlRo[0] * Q.rev * 2.5; orr += dlRo[1] * Q.rev * 2.5; }
        }
        o[0] = ol + (rnd() - .5) * .0015; o[1] = orr + (rnd() - .5) * .0015; break;
    }
    case 6: {
        const QRV& Q = RV; const RvMod& M = *Q.M;
        if (M.k == 1) {
            const double m = std::tanh(.5 * (l + r) * Q.dr) / std::sqrt(Q.dr); rvW += 1 / fs * .7; const double wob = Q.wob * std::sin(TP * rvW); double L_ = 0, R_ = 0;
            const int ns = (int) rvSp.size();
            for (int k = 0; k < ns; ++k) { const double y = rvSp[(size_t) k].run(m * .6, Q.c * (1 + k * .02), Q.g, Q.a, wob * (k ? 1 : -1)); if (k & 1) R_ += y; else L_ += y; }
            if (ns == 1) R_ = L_; const double nn = std::sqrt(ns / 2.0); L_ /= nn; R_ /= nn;
            if (M.sdiff) { rvD[0] += .5 * (L_ - rvD[0]); rvD[1] += .5 * (R_ - rvD[1]); const double a = .75 * L_ + .25 * rvD[1], b = .75 * R_ + .25 * rvD[0]; L_ = a; R_ = b; }
            if (M.lofi) { const double qq = 64; L_ = jround(L_ * qq) / qq; R_ = jround(R_ * qq) / qq; }
            o[0] = L_ * .8; o[1] = R_ * .8;
        } else {
            for (int ch = 0; ch < 2; ++ch) { double x = ch ? r : l; rvIn[ch] += Q.bw * (x - rvIn[ch]); x = rvIn[ch]; rvIn[2 + ch] += Q.hp * (x - rvIn[2 + ch]); x -= rvIn[2 + ch]; rvPd[ch].put(x); }
            const double il = rvPd[0].get(Q.pre + 1), ir = rvPd[1].get(Q.pre + 1); rvF.run(il, ir, rvO); double ol = rvO[0] * Q.lateG, orr = rvO[1] * Q.lateG;
            if (Q.erG != 0 && !M.er.empty()) for (const auto& e : M.er) { const double t = Q.pre + e.ms * fs / 1000; const double s = (rvPd[0].get(t) + rvPd[1].get(t)) * .5 * e.g * Q.erG; ol += s * (1 - e.pan) * .7; orr += s * (1 + e.pan) * .7; }
            if (M.bits) { const double qq = std::pow(2.0, M.bits + 1); ol = qz(ol, qq); orr = qz(orr, qq); }
            o[0] = ol; o[1] = orr;
        }
        const double mid = .5 * (o[0] + o[1]), sd = .5 * (o[0] - o[1]) * Q.w; o[0] = mid + sd; o[1] = mid - sd; break;
    }
    case 7: {
        const QGR& G = GR; const double m = .5 * (l + r); if (!G.frz) grB.put(m + grY * G.fb);
        auto win = [&](double t) {
            const double h = hann(t); if (G.win < .5) { const double tr = std::min({ 1.0, t / .15, (1 - t) / .15 }); const double k = G.win * 2; return tr * (1 - k) + h * k; }
            const double k = (G.win - .5) * 2; const double pc = std::min(1.0, t / .02) * std::exp(-5 * t); return h * (1 - k) + pc * k;
        };
        grC -= 1;
        if (grC <= 0) {
            grC += G.sync ? G.intv : G.intv * (.6 + .8 * rnd());
            for (int k = 0; k < grN; ++k) if (!grA[k]) {
                double st = G.st; if (G.warp != 0) st += jround((rnd() * 2 - 1) * G.warp * 12); double rate = std::pow(2.0, st / 12);
                bool rvs = G.rev != 0 && rnd() < G.rev; if (G.sync) rvs = G.pat == 1 || (G.pat == 2 && rnd() < .5); if (rvs) rate = -rate;
                const double back = G.pos + G.len * std::max(1.0, std::fabs(rate)) + rnd() * G.spray + (G.scan != 0 ? grS * fs * 1.6 : 0) + 4;
                const double room = grB.n - G.len * std::fabs(rate) - 16; const double bk = std::min(back, room);
                grA[k] = 1; grL[k] = (float) G.len; grT[k] = 0; grR[k] = (float) rate; grP[k] = (float) (grB.w - bk + (rate < 0 ? G.len * std::fabs(rate) : 0)); grG[k] = (float) (.5 + (rnd() - .5) * G.spread); break;
            }
        }
        double yl = 0, yr = 0; int n = 0;
        for (int k = 0; k < grN; ++k) {
            if (!grA[k]) continue; const double t = (double) grT[k] / grL[k]; if (t >= 1) { grA[k] = 0; continue; }
            const double pos = grP[k] + (double) grT[k] * grR[k]; const double i = std::floor(pos), fr = pos - i;
            const double s = (grB.at(i) * (1 - fr) + grB.at(i + 1) * fr) * win(t); const double pa = grG[k]; yl += s * (1 - pa); yr += s * pa; grT[k] = (float) (grT[k] + 1); n++;
        }
        const double gn = (G.sync ? 2.6 : 1.6) / std::max(1.0, std::sqrt((double) n)); yl *= gn; yr *= gn; grY = .5 * (yl + yr);
        if (G.dr != 0) { yl = std::tanh(yl * (1 + G.dr * 6)) / (1 + G.dr * 1.5); yr = std::tanh(yr * (1 + G.dr * 6)) / (1 + G.dr * 1.5); }
        if (G.lp || G.hp) { grF[0].run(yl, G.fg, .9); grF[1].run(yr, G.fg, .9); yl = G.lp ? grF[0].lpo : grF[0].hpo; yr = G.lp ? grF[1].lpo : grF[1].hpo; }
        o[0] = yl; o[1] = yr; break;
    }
    case 8: {
        const QFK& K = FK;
        if (K.m == 0) {
            for (int ch = 0; ch < 2; ++ch) {
                const double x = ch ? r : l; double y;
                if (K.res) { SVF& F = fkR[ch]; F.run(x * .5 + fkY[ch] * K.fb * .9, K.f[ch], .04 + .5 * (1 - std::min(1.0, K.fb))); y = F.bpo; }
                else y = fkD[ch].get(K.d2[ch]);
                double s = y; if (K.lp || K.hp) { SVF& F = fkF[ch]; F.run(s, K.tg, .9); s = K.lp ? F.lpo : F.hpo; } s = std::tanh(s * (1 + K.dr * 6)) / (1 + K.dr * 1.5);
                if (!K.res) fkD[ch].put(x * .6 + s * K.fb); fkY[ch] = s; o[ch] = s * .5;
            }
        } else {
            for (int ch = 0; ch < 2; ++ch) {
                const double x = K.kill ? (rnd() - .5) * 1e-4 : (ch ? r : l); DL& dl = fkD[ch]; const double v_ = dl.get(K.d1 * (ch ? 1.013 : 1));
                SVF& F = fkF[ch]; F.run(v_, K.bg, .7); const double loop = std::tanh((F.bpo * 1.6 + v_ * .25) * K.fb); dl.put(x + loop); o[ch] = loop * K.vol;
            }
        }
        break;
    }
    case 9: {
        const QBC& Q = BC; double x0 = l, x1 = r;
        if (Q.m == 3 && Q.drive != 0) { const double g = 1 + Q.drive * 6; x0 = std::tanh(x0 * g) / (1 + Q.drive * 3); x1 = std::tanh(x1 * g) / (1 + Q.drive * 3); }
        bcC += 1; const double per = Q.m == 3 && Q.jit != 0 ? Q.div * (1 + (rnd() * 2 - 1) * Q.jit * .8) : Q.div;
        if (bcC >= per) { bcC -= per; if (bcC >= per) bcC = 0; bcH[0] = (std::floor(x0 * Q.q) + .5) / Q.q; bcH[1] = (std::floor(x1 * Q.q) + .5) / Q.q; }
        double y0 = bcH[0], y1 = bcH[1];
        { double* z = bcZ; const double a0 = y0 - z[0] + .999 * z[1]; z[0] = y0; z[1] = a0; const double a1 = y1 - z[2] + .999 * z[3]; z[2] = y1; z[3] = a1; y0 = a0; y1 = a1; }
        if (Q.m == 0 && Q.gate > 0) { const double e = std::max(std::fabs(l), std::fabs(r)); bcE += (e > bcE ? .02 : .0005) * (e - bcE); const double t = bcE > Q.gate ? 1 : 0; bcG += (t - bcG) * (t != 0 ? .02 : .002); y0 *= bcG; y1 *= bcG; }
        if (Q.m == 1) {
            bcB[0].put(y0); bcB[1].put(y1);
            if (bcSt) { const int n = bcSl, i = bcSi; const double fd = std::min({ 1.0, i / 48.0, (n - i) / 48.0 }); y0 = bcB[0].at(bcSs + i) * fd; y1 = bcB[1].at(bcSs + i) * fd; if (++bcSi >= n) bcSi = 0; }
        }
        if ((Q.m == 0 && (Q.lp || Q.hp)) || Q.m == 1 || (Q.m == 2 && Q.lp)) {
            const double kk = Q.k != 0 ? Q.k : .9; bcF[0].run(y0, Q.fg, kk); bcF[1].run(y1, Q.fg, kk); y0 = Q.hp ? bcF[0].hpo : bcF[0].lpo; y1 = Q.hp ? bcF[1].hpo : bcF[1].lpo;
        }
        if (Q.m == 2) { y0 *= Q.vol; y1 *= Q.vol; }
        if (Q.m == 3) { if (bcDr > 0) { bcDr--; bcDg += (0 - bcDg) * .05; } else bcDg += (1 - bcDg) * .01; const double nz = (rnd() - .5) * Q.noise; y0 = (y0 + nz) * bcDg; y1 = (y1 + nz) * bcDg; }
        o[0] = y0; o[1] = y1; break;
    }
    case 10: {
        const QRM& Q = RM; rmP += Q.f / fs; rmP -= std::floor(rmP);
        if (Q.m == 1 && Q.mode > 0) {
            const double dir = Q.mode == 1 ? -1 : 1;
            for (int ch = 0; ch < 2; ++ch) { const double ph = rmP * (ch ? 1 + Q.sp * .5 : 1); const double c = std::cos(TP * ph), s = std::sin(TP * ph); rmH[ch].run(ch ? r : l); o[ch] = (rmH[ch].re * c - dir * rmH[ch].im * s); }
            break;
        }
        const double s = std::sin(TP * rmP); const double c = Q.sq ? std::tanh(s * 10) : s;
        double x0 = l, x1 = r; if (Q.drive != 0) { const double g = 1 + Q.drive * 5; x0 = std::tanh(x0 * g) / (1 + Q.drive * 1.5); x1 = std::tanh(x1 * g) / (1 + Q.drive * 1.5); }
        if (Q.m == 1 && Q.sp > 0) { const double c2 = std::sin(TP * rmP * (1 + Q.sp * .5)); o[0] = x0 * c; o[1] = x1 * c2; } else { o[0] = x0 * c * Q.vol; o[1] = x1 * c * Q.vol; }
        break;
    }
    case 11: {
        const QST& Q = ST; stB[0].put(l); stB[1].put(r); stX += ((stOn ? 1 : 0) - stX) * .004; if (stX < 1e-4) { o[0] = l; o[1] = r; break; }
        if (Q.m == 1 && stOn) {
            stGt++; const double g = std::min(1.0, stGt / Q.dur);
            if (g >= 1) { const int mode = iround(P(fxp::L::f11smode)); if (mode == 1 && P(fxp::L::f11on) > .5) { stGt = 0; stStart = stB[0].w - Q.l0; stLen = Q.l0; stLenT = Q.l0; stI = 0; stPos = 0; stRep = 0; } else { stOn = false; stG = -1; } }
            const double k = Q.cv < .5 ? std::pow(g, .4 + Q.cv * 1.2) : std::pow(g, 1 + (Q.cv - .5) * 4); stLenT = std::max(64.0, jround(Q.l0 + (Q.l1 - Q.l0) * k));
        }
        const double n = stLen, i = stI; const double play = n * Q.gate; const double gain = std::pow(1 - Q.fade * .5, stRep);
        double yl = 0, yr = 0;
        if (i < play) {
            const double pos = stPos; const double fade = std::min({ 1.0, i / 64, (play - i) / 64 }); const double j = std::floor(pos), fr = pos - j; const double s0 = stStart;
            yl = (stB[0].at(s0 + j) * (1 - fr) + stB[0].at(s0 + j + 1) * fr) * fade * gain; yr = (stB[1].at(s0 + j) * (1 - fr) + stB[1].at(s0 + j + 1) * fr) * fade * gain;
        }
        stPos += std::pow(2.0, Q.pitch * stRep / 12);
        if (++stI >= n) { stI = 0; stPos = 0; stRep++; if (Q.m == 1 && stLenT != 0) stLen = std::min(stLenT, (double) stB[0].n - 16); }
        o[0] = l + (yl - l) * stX; o[1] = r + (yr - r) * stX; break;
    }
    case 12: {
        const QRE& Q = RE; DL* B = reB;
        if (Q.m == 0) {
            if (!Q.hold) { B[0].put(l); B[1].put(r); reHold = -1; } else if (reHold < 0) reHold = B[0].w;
            const double anchor = reHold < 0 ? B[0].w : reHold; double yl = 0, yr = 0;
            for (auto& h : reH) {
                if (h.c <= 0 || h.c >= h.d) {
                    if (h.half && !h.go) { h.go = true; h.c = -jround(Q.T / std::fabs(Q.rate) / 2); }
                    if (h.c >= h.d || h.c == 0) {
                        double rt = Q.rate; if (Q.rand != 0 && rnd() < Q.rand) { static const double S[6] = { -2, -1, -.5, .5, 1, 2 }; rt = S[(int) std::floor(rnd() * 6)]; }
                        h.r = rt; h.d = std::max(64.0, jround(Q.T / std::fabs(rt))); h.p = rt < 0 ? anchor - 1 : anchor - Q.T; h.c = 0;
                    }
                }
                if (h.c >= 0) { const double g = hann(h.c / h.d); const double j = std::floor(h.p), fr = h.p - j; yl += (B[0].at(j) * (1 - fr) + B[0].at(j + 1) * fr) * g; yr += (B[1].at(j) * (1 - fr) + B[1].at(j + 1) * fr) * g; h.p += h.r; }
                h.c++;
            }
            o[0] = yl; o[1] = yr; break;
        }
        // reverse delay: two heads, each plays the last TIME window backward, offset by half a window so their fades overlap
        const double T = Q.T; double md = 0; if (Q.md != 0) { reL += Q.mr / fs; reL -= std::floor(reL); md = std::sin(TP * reL) * Q.md; }
        double yl = 0, yr = 0;
        for (int k = 0; k < 2; ++k) {
            Head& h = reH[k]; if (h.r != -2) { h.r = -2; h.c = k ? jround(T / 2) : 0; h.s = B[0].w; } if (h.c >= T) { h.c = 0; h.s = B[0].w; }
            const double g = hann(h.c / T); const double pos = h.s - 1 - h.c + md * g; const double j = std::floor(pos), fr = pos - j;
            yl += (B[0].at(j) * (1 - fr) + B[0].at(j + 1) * fr) * g; yr += (B[1].at(j) * (1 - fr) + B[1].at(j + 1) * fr) * g; h.c++;
        }
        reF[0] += Q.a * (yl - reF[0]); reF[1] += Q.a * (yr - reF[1]); B[0].put(l + reF[0] * Q.fb); B[1].put(r + reF[1] * Q.fb); o[0] = yl; o[1] = yr; break;
    }
    case 13: {
        const QFZ& Q = FZ; fzH[0].put(l); fzH[1].put(r); double yl = 0, yr = 0;
        for (auto& Ly : fzLy) {
            if (Ly.st == 0 || Ly.n < 8) continue;
            if (Ly.st == 1) { Ly.env += (1 - Ly.env) * Q.att; Ly.env *= Q.dec; } else { Ly.env -= Ly.env * Q.rel; if (Ly.env < 1e-4) { Ly.st = 0; continue; } }
            const int n = Ly.n; const double G = std::min(Q.G, (double) n);
            for (auto& h : Ly.h) {
                if (h.half && !h.go) { h.go = true; h.c = -jround(G / 2); }
                if (h.c >= 0 && (h.c == 0 || h.c >= G)) { h.c = 0; h.s = Q.m == 0 ? 0 : rnd() * (n - G); h.p = 0; }
                if (h.c >= 0) {
                    const double g = hann(h.c / G); double pos = h.s + h.p; pos = std::fmod(pos, (double) n); const int j = (int) std::floor(pos); const double fr = pos - j; const int j2 = j + 1 >= n ? 0 : j + 1;
                    yl += (Ly.b[0][(size_t) j] * (1 - fr) + Ly.b[0][(size_t) j2] * fr) * g * Ly.env; yr += (Ly.b[1][(size_t) j] * (1 - fr) + Ly.b[1][(size_t) j2] * fr) * g * Ly.env; h.p += fzR;
                }
                h.c++;
            }
        }
        yl *= Q.lvl; yr *= Q.lvl;
        if (Q.m == 0 && (Q.lp || Q.hp)) { fzF[0].run(yl, Q.tg, .9); fzF[1].run(yr, Q.tg, .9); yl = Q.lp ? fzF[0].lpo : fzF[0].hpo; yr = Q.lp ? fzF[1].lpo : fzF[1].hpo; }
        o[0] = l + yl; o[1] = r + yr; break;
    }
    case 14: {
        const QTP& Q = TPq; auto ts = [](double u, double a) { return std::tanh(u + a) - std::tanh(a); };
        if (Q.m == 0) {
            if (tpLg < 0) tpLg = Q.lag; tpLg += (Q.lag - tpLg) * .0004;
            tpW += .7 / fs; tpW -= std::floor(tpW); tpWt += 1 / fs; if (tpWt > .25) { tpWt = 0; tpN = rnd() * 2 - 1; } tpWb += (tpN - tpWb) * .00012;
            const double dl = tpLg + Q.wob * (.6 * std::sin(TP * tpW) + .6 * tpWb); const double X[2] = { l, r }; double R_[2] = { 0, 0 }, G[2] = { 0, 0 };
            for (int ch = 0; ch < 2; ++ch) {
                double y = X[ch]; if (Q.son) y = ts(y * Q.g, .05); y *= Q.out; tpT[ch] += Q.ta * (y - tpT[ch]); y = tpT[ch] * std::pow(1.6, -Q.t * .5) + (y - tpT[ch]) * std::pow(4.0, Q.t);
                if (!Q.dbl) { o[ch] = y; continue; }
                tpR[ch].put(y); tpD[ch].put(y + tpY[ch] * Q.fb); R_[ch] = tpR[ch].get(Q.ref); double g = tpD[ch].get(dl); tpYl[ch] += Q.la * (g - tpYl[ch]); g = tpYl[ch]; tpY[ch] = g; G[ch] = g;
            }
            if (Q.dbl) {
                if (Q.wide) { o[0] = (Q.gr * R_[0] + .3 * Q.s * Q.gl * G[0]) * Q.nm; o[1] = (Q.s * Q.gl * G[1] + .3 * Q.gr * R_[1]) * Q.nm; }
                else { o[0] = (Q.gr * R_[0] + Q.s * Q.gl * G[0]) * Q.nm; o[1] = (Q.gr * R_[1] + Q.s * Q.gl * G[1]) * Q.nm; }
            }
        } else if (Q.m == 1) {
            tpW += .55 / fs; tpF += 7.5 / fs; tpF2 += 11.3 / fs; tpW -= std::floor(tpW); tpF -= std::floor(tpF); tpF2 -= std::floor(tpF2);
            tpWt += 1 / fs; if (tpWt > .4) { tpWt = 0; tpN = rnd() * 2 - 1; } tpWb += (tpN - tpWb) * .00008;
            const double d = fs * (.012 + Q.wow * .004 * (.7 * std::sin(TP * tpW) + .5 * tpWb) + Q.fl * .00035 * (std::sin(TP * tpF) + .5 * std::sin(TP * tpF2)));
            const double drop = cl(1 - Q.wear * 1.6 * std::max(0.0, std::fabs(tpWb) - .35), .25, 1); const double X[2] = { l, r };
            for (int ch = 0; ch < 2; ++ch) {
                double y = Q.h * ts(X[ch] * Q.g / Q.h, .08) * Q.out; tpC[ch].put(y); y = tpC[ch].get(d) * drop + (rnd() - .5) * Q.hiss;
                tpL1[ch] += Q.la * (y - tpL1[ch]); tpL2[ch] += Q.la * (tpL1[ch] - tpL2[ch]); y = tpL2[ch]; tpHP[ch] += Q.ha * (y - tpHP[ch]); y -= tpHP[ch];
                tpT[ch] += Q.ta * (y - tpT[ch]); o[ch] = tpT[ch] * std::pow(1.6, -Q.t * .5) + (y - tpT[ch]) * std::pow(4.0, Q.t);
            }
        } else {
            const double X[2] = { l, r };
            for (int ch = 0; ch < 2; ++ch) {
                double y = X[ch] * Q.gi; y = Q.h * ts(y / Q.h, Q.as); SVF& B = tpB[ch]; B.run(y, Q.bg, Q.bk); y += B.bpo * Q.bm;
                y += (rnd() - .5) * Q.hiss; SVF& H = tpH[ch]; y = H.run(y, Q.lg, 1.3); tpHP[ch] += Q.ha * (y - tpHP[ch]); o[ch] = (y - tpHP[ch]) * Q.out;
            }
        }
        break;
    }
    case 15: {
        const QSH& Q = SH; shPd[0].put(l); shPd[1].put(r); double il = shPd[0].get(Q.pre + 1), ir = shPd[1].get(Q.pre + 1);
        if (Q.m == 2) { il = std::tanh(il * Q.dw) / Q.dwn; ir = std::tanh(ir * Q.dw) / Q.dwn; }
        auto world = [&](double a, double b) -> double* {
            double* W = shWo; if (Q.wt == 2) { W[0] = shSp[0].run(a * .5, .6, Q.sg[0], Q.sa, 0) * 1.4; W[1] = shSp[1].run(b * .5, .6, Q.sg[1], Q.sa, 0) * 1.4; } else shW.run(a, b, W); return W; };
        if (Q.m == 2 && Q.rt == 2 && Q.wl > 0) { const double* W = world(il, ir); il += (W[0] * 1.6 - il) * Q.wl; ir += (W[1] * 1.6 - ir) * Q.wl; }
        shF.run(il + shY[0], ir + shY[1], shO);
        // the shimmer: the reverb output, pitch shifted, filtered and fed back into the reverb
        for (int ch = 0; ch < 2; ++ch) {
            double s = 0; for (int k = 0; k < Q.nv; ++k) s += shS[k][ch].run(shO[ch], Q.v[k], Q.win); s *= Q.gn;
            shL[ch] += Q.la * (s - shL[ch]); s = shL[ch]; shH[ch] += Q.ha * (s - shH[ch]); s -= shH[ch];
            if (Q.fl) { SVF& F = shLF[ch]; F.run(s, Q.fg, 1.2); s = Q.fl == 1 ? F.lpo : F.hpo; } shY[ch] = std::tanh(s * Q.amt * 1.1);
        }
        double ol = shO[0] * Q.out, orr = shO[1] * Q.out;
        if (Q.m == 1 && Q.fl) { shOF[0].run(ol, Q.fg, 1.2); shOF[1].run(orr, Q.fg, 1.2); ol = Q.fl == 1 ? shOF[0].lpo : shOF[0].hpo; orr = Q.fl == 1 ? shOF[1].lpo : shOF[1].hpo; }
        if (Q.m == 2 && Q.rt != 2 && Q.wl > 0) {
            const double* W = Q.rt == 0 ? world(shO[0], shO[1]) : world(il, ir);
            if (Q.rt == 0) { ol += (W[0] * Q.out * 1.3 - ol) * Q.wl; orr += (W[1] * Q.out * 1.3 - orr) * Q.wl; } else { ol += W[0] * Q.wl * .8; orr += W[1] * Q.wl * .8; }
        }
        o[0] = std::tanh(ol * .7) / .7; o[1] = std::tanh(orr * .7) / .7; break;
    }
    }
}

} // namespace oct
