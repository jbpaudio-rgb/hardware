// All Octavio synth engines, by the id the app uses.
#pragma once
#include "Vact.h"
#include "Mini.h"
#include "Pro70.h"
#include "A26.h"
#include "Poly.h"
#include "Vc3.h"
#include "M20.h"
#include "Easel.h"
#include "Dr8.h"
#include "Sp64.h"
#include <cstring>
namespace oct {
inline EngineBase* makeEngine(const char* id) {
    if (!std::strcmp(id, "vact")) return new Vact();
    if (!std::strcmp(id, "mini")) return new Mini();
    if (!std::strcmp(id, "pro70")) return new Pro70();
    if (!std::strcmp(id, "a26")) return new A26();
    if (!std::strcmp(id, "j8")) return new J8();
    if (!std::strcmp(id, "cs8")) return new CS8();
    if (!std::strcmp(id, "vc3")) return new Vc3();
    if (!std::strcmp(id, "m20")) return new M20();
    if (!std::strcmp(id, "easel")) return new Easel();
    if (!std::strcmp(id, "dr8")) return new Dr8();
    if (!std::strcmp(id, "sp64")) return new Sp64();
    return nullptr;
}
}
