// The C++ engines (the plugin's, ported line for line from the web engines) as one WebAssembly module, for the web apps.
// Built with wasi-sdk by tools/build_wasm.sh; used by web/octavio-app/wasm_worklet.js (one AudioWorklet processor for every engine).
// Everything is plain C: the worklet writes parameters straight into each instance's parameter array and reads 128-sample blocks back.
#include "synth/Engines.h"
#include "FxEngine.h"
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {
struct Inst {
    std::unique_ptr<oct::EngineBase> e;
    std::unique_ptr<oct::FxRack> fx;
    float L[128] {}, R[128] {}, inL[128] {}, inR[128] {}, m[16] {};
};
}

#define EXPORT(name) __attribute__((export_name(#name)))

extern "C" {

EXPORT(oct_alloc) void* oct_alloc (int bytes) { return std::malloc ((size_t) bytes); }
EXPORT(oct_free) void oct_free (void* p) { std::free (p); }

// a synth engine ("vact" ... "sp64") or the FX rack ("fx")
EXPORT(oct_new) Inst* oct_new (const char* id, double sr)
{
    auto* x = new Inst();
    if (! std::strcmp (id, "fx")) { x->fx = std::make_unique<oct::FxRack>(); x->fx->prepare (sr); }
    else { x->e.reset (oct::makeEngine (id)); if (! x->e) { delete x; return nullptr; } x->e->prepare (sr); }
    return x;
}
EXPORT(oct_delete) void oct_delete (Inst* x) { delete x; }

EXPORT(oct_params) float* oct_params (Inst* x) { return x->fx ? x->fx->p : x->e->p.data(); }
EXPORT(oct_nparams) int oct_nparams (Inst* x) { return x->fx ? fxp::NP : x->e->np; }
EXPORT(oct_out) float* oct_out (Inst* x, int ch) { return ch ? x->R : x->L; }
EXPORT(oct_in) float* oct_in (Inst* x, int ch) { return ch ? x->inR : x->inL; }
EXPORT(oct_meter) float* oct_meter (Inst* x) { return x->m; }

// n samples (up to 128); clock: 1 = follow the beat position ppq (quarter notes at the block start)
EXPORT(oct_process) void oct_process (Inst* x, int n, int clock, double ppq, int withInput)
{
    if (x->fx) { x->fx->hostClock = clock != 0; x->fx->hostPpq = ppq; x->fx->process (withInput ? x->inL : nullptr, withInput ? x->inR : nullptr, x->L, x->R, n); return; }
    x->e->hostClock = clock != 0; x->e->hostPpq = ppq; x->e->process (x->L, x->R, n);
}

EXPORT(oct_note) void oct_note (Inst* x, int n, double v, int on)
{
    if (x->fx) { if (on) x->fx->noteOn (n, v); else x->fx->noteOff (n); return; }   // the FX rack's own keyboard synth (FX app)
    if (on) x->e->noteOn (n, v); else x->e->noteOff (n);
}
EXPORT(oct_alloff) void oct_alloff (Inst* x) { if (x->fx) x->fx->allOff(); else x->e->allOff(); }
EXPORT(oct_bend) void oct_bend (Inst* x, double semis) { if (x->e) x->e->bend = semis; }
EXPORT(oct_pad) void oct_pad (Inst* x, int k, double v, int on) { if (! x->e) return; if (on) x->e->padOn (k, v); else x->e->padOff (k); }
EXPORT(oct_hit) void oct_hit (Inst* x, int t) { if (x->e) x->e->pad (t); }

// what the panel's meters show: synth engines fill m (the layout is the engine's own); the FX rack gives env and the 8 mixes
EXPORT(oct_meters) int oct_meters (Inst* x)
{
    if (x->fx) { x->m[0] = (float) std::min (1.0, x->fx->env * 3); for (int q = 0; q < 8; ++q) x->m[1 + q] = x->fx->mixS[q]; return 9; }
    return x->e->meters (x->m);
}
EXPORT(oct_step) int oct_step (Inst* x) { return x->fx ? x->fx->step : x->e->step(); }

// FX boxes moved: new slot k gets old slot map[k]
EXPORT(oct_perm) void oct_perm (Inst* x, const int* map) { if (x->fx) x->fx->permute (map); }

// SAMPLER: a slot gets a sample (copied), is cleared (n = 0, user = 1), or gets its kit sound back (user = 0)
EXPORT(oct_sample) void oct_sample (Inst* x, int slot, const float* L, int n, const float* R, int nR, double sr, int user)
{
    auto* s = x->e ? dynamic_cast<oct::Sp64*> (x->e.get()) : nullptr; if (! s) return;
    if (! user) { s->setSlot (slot, nullptr, false); return; }   // the engine remakes its kit sound
    if (n <= 0) { s->setSlot (slot, nullptr, true); return; }
    auto d = std::make_shared<oct::SmpData>(); d->sr = sr; d->L.assign (L, L + n); if (R && nR == n) d->R.assign (R, R + nR);
    s->setSlot (slot, d, true);
}

} // extern "C"
