// Text for the canvas: stb_truetype, one glyph bitmap cache per font and size.
#include "Canvas.h"
#include "Font.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb/stb_truetype.h"
#include <cstdio>
#include <fstream>
#include <iterator>

namespace ui {

struct Font::Impl {
    std::vector<unsigned char> data; stbtt_fontinfo info {}; bool ok = false;
    struct Glyph { int w = 0, h = 0, xo = 0, yo = 0; float adv = 0; std::vector<unsigned char> a; };
    std::map<std::pair<int, uint32_t>, Glyph> cache;   // (size in 1/4 px, codepoint)
    const Glyph& glyph(float size, uint32_t cp) {
        const auto key = std::make_pair((int) std::lround(size * 4), cp);
        auto it = cache.find(key); if (it != cache.end()) return it->second;
        Glyph g;
        if (ok) {
            const float sc = stbtt_ScaleForPixelHeight(&info, size);
            int adv, lsb; stbtt_GetCodepointHMetrics(&info, (int) cp, &adv, &lsb); g.adv = adv * sc;
            unsigned char* b = stbtt_GetCodepointBitmap(&info, sc, sc, (int) cp, &g.w, &g.h, &g.xo, &g.yo);
            if (b) { g.a.assign(b, b + (size_t) g.w * g.h); stbtt_FreeBitmap(b, nullptr); }
        }
        return cache.emplace(key, std::move(g)).first->second;
    }
};

Font::Font() : d(new Impl) {}
Font::~Font() = default;
bool Font::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    d->data.assign(std::istreambuf_iterator<char>(f), {});
    d->ok = !d->data.empty() && stbtt_InitFont(&d->info, d->data.data(), stbtt_GetFontOffsetForIndex(d->data.data(), 0));
    d->cache.clear(); return d->ok;
}
bool Font::loadMemory(const unsigned char* data, size_t size) {
    d->data.assign(data, data + size);
    d->ok = !d->data.empty() && stbtt_InitFont(&d->info, d->data.data(), stbtt_GetFontOffsetForIndex(d->data.data(), 0));
    d->cache.clear(); return d->ok;
}
bool Font::ok() const { return d->ok; }

// UTF-8 to code points
static std::vector<uint32_t> utf8(const std::string& s) {
    std::vector<uint32_t> v;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char) s[i];
        uint32_t cp = c; int n = 0;
        if (c >= 0xF0) { cp = c & 7; n = 3; } else if (c >= 0xE0) { cp = c & 15; n = 2; } else if (c >= 0xC0) { cp = c & 31; n = 1; }
        ++i; for (int k = 0; k < n && i < s.size(); ++k, ++i) cp = cp << 6 | ((unsigned char) s[i] & 63);
        v.push_back(cp);
    }
    return v;
}

float Canvas::textWidth(Font& f, float size, const std::string& s) {
    float x = 0; for (uint32_t cp : utf8(s)) x += f.d->glyph(size, cp).adv; return x;
}

float Canvas::text(Font& f, float size, float x, float y, const std::string& s, Px c, int align, float alpha) {
    y += ty;
    const float wdt = textWidth(f, size, s);
    if (align == 1) x -= wdt / 2; else if (align == 2) x -= wdt;
    float pen = x;
    for (uint32_t cp : utf8(s)) {
        const auto& g = f.d->glyph(size, cp);
        const int gx = (int) std::lround(pen) + g.xo, gy = (int) std::lround(y) + g.yo;
        for (int j = 0; j < g.h; ++j)
            for (int i = 0; i < g.w; ++i) {
                const unsigned char a = g.a[(size_t) j * g.w + i];
                if (a) blend(gx + i, gy + j, c, a / 255.f * alpha);
            }
        pen += g.adv;
    }
    return wdt;
}

std::string Canvas::fit(Font& f, float size, const std::string& s, float maxW) {
    if (textWidth(f, size, s) <= maxW) return s;
    std::string t = s;
    while (!t.empty()) {
        do t.pop_back(); while (!t.empty() && ((unsigned char) t.back() & 0xC0) == 0x80);   // whole UTF-8 characters
        if (!t.empty() && ((unsigned char) t.back() & 0xC0) == 0xC0) t.pop_back();
        if (textWidth(f, size, t + "…") <= maxW) return t + "…";
    }
    return "…";
}

} // namespace ui
