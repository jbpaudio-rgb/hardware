// Reads a WAV file into a sampler sample (oct::SmpData): PCM 8/16/24/32-bit, float 32/64-bit, plain or
// WAVE_FORMAT_EXTENSIBLE, mono or stereo (the first two channels of more), any rate (the sampler plays it at its rate).
#pragma once
#include "../core/synth/Sp64.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace box {

inline bool loadWav(const std::string& path, oct::SmpData& d, std::string& err, double maxSeconds = 120) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot read the file"; return false; }
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto u16 = [&](size_t p) { return (uint32_t) (b[p] | b[p + 1] << 8); };
    auto u32 = [&](size_t p) { return (uint32_t) (b[p] | b[p + 1] << 8 | b[p + 2] << 16 | (uint32_t) b[p + 3] << 24); };
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) { err = "not a WAV file"; return false; }
    int fmt = 0, ch = 0, bits = 0; uint32_t rate = 0; size_t data = 0, len = 0;
    for (size_t p = 12; p + 8 <= b.size();) {
        const uint32_t n = u32(p + 4); const size_t body = p + 8;
        if (!std::memcmp(&b[p], "fmt ", 4) && body + 16 <= b.size()) {
            fmt = (int) u16(body); ch = (int) u16(body + 2); rate = u32(body + 4); bits = (int) u16(body + 14);
            if (fmt == 0xFFFE && n >= 26 && body + 26 <= b.size()) fmt = (int) u16(body + 24);   // EXTENSIBLE: the sub-format
        } else if (!std::memcmp(&b[p], "data", 4)) { data = body; len = std::min<size_t>(n, b.size() - body); break; }
        p = body + n + (n & 1);
    }
    if (!data || !ch || !rate || !bits) { err = "WAV file cut short"; return false; }
    if (!(fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) && !(fmt == 3 && (bits == 32 || bits == 64))) { err = "WAV format not supported"; return false; }
    const size_t bps = (size_t) bits / 8, frame = bps * (size_t) ch;
    size_t frames = len / frame; frames = std::min(frames, (size_t) (maxSeconds * rate));
    if (!frames) { err = "empty WAV file"; return false; }
    auto sample = [&](size_t q) -> float {
        const uint8_t* s = &b[q];
        if (fmt == 3) { if (bits == 32) { float x; std::memcpy(&x, s, 4); return x; } double x; std::memcpy(&x, s, 8); return (float) x; }
        switch (bits) {
        case 8: return (s[0] - 128) / 128.f;
        case 16: return (int16_t) (s[0] | s[1] << 8) / 32768.f;
        case 24: return (int32_t) ((uint32_t) (s[0] << 8 | s[1] << 16 | (uint32_t) s[2] << 24)) / 2147483648.f;
        default: return (int32_t) (s[0] | s[1] << 8 | s[2] << 16 | (uint32_t) s[3] << 24) / 2147483648.f;
        }
    };
    d.sr = rate; d.L.resize(frames); d.R.clear(); if (ch > 1) d.R.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        const size_t q = data + i * frame;
        d.L[i] = sample(q); if (ch > 1) d.R[i] = sample(q + bps);
    }
    return true;
}

} // namespace box
