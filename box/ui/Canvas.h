// A small software canvas for the synth's screen: 32-bit pixels (0xRRGGBB), anti-aliased rounded boxes, rings and
// text (stb_truetype). No GPU, no toolkit: the same pixels go to the Pi's display (KMS), a desktop window (SDL) or a PNG.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

using Px = uint32_t;
constexpr Px rgb(int r, int g, int b) { return (Px) ((r & 255) << 16 | (g & 255) << 8 | (b & 255)); }
inline Px mix(Px a, Px b, float t) {   // a -> b
    t = std::clamp(t, 0.f, 1.f);
    auto c = [&](int s) { const float x = (float) ((a >> s) & 255), y = (float) ((b >> s) & 255); return (Px) std::lround(x + (y - x) * t) << s; };
    return c(16) | c(8) | c(0);
}
inline Px scale(Px a, float k) { return mix(0, a, k); }

struct Font;   // a TrueType font, glyphs cached per size

struct Canvas {
    int w, h; std::vector<Px> px;
    Canvas(int W, int H) : w(W), h(H), px((size_t) W * H, 0) {}

    void clear(Px c) { std::fill(px.begin(), px.end(), c); }
    void blend(int x, int y, Px c, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0) return;
        Px& d = px[(size_t) y * w + x]; d = a >= 1 ? c : mix(d, c, a);
    }
    void rect(float x0, float y0, float x1, float y1, Px c) { rrect(x0, y0, x1, y1, 0, c); }

    // signed distance from a point to a rounded box (negative inside)
    static float sdBox(float px_, float py, float x0, float y0, float x1, float y1, float r) {
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        const float qx = std::fabs(px_ - cx) - ((x1 - x0) / 2 - r), qy = std::fabs(py - cy) - ((y1 - y0) / 2 - r);
        return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - r;
    }
    // filled rounded rectangle, anti-aliased edges. Only the corner rows and the two edge pixels of the other rows
    // need the distance; the rest is a plain fill (most of the screen is boxes, this keeps a frame cheap on the Pi)
    float ty = 0;   // a vertical shift added to everything drawn (the screen draws its pages in their own coordinates)
    void rrect(float x0, float y0, float x1, float y1, float r, Px c, float alpha = 1) {
        y0 += ty; y1 += ty;
        r = std::max(0.f, std::min({ r, (x1 - x0) / 2, (y1 - y0) / 2 }));
        const int ix0 = std::max(0, (int) std::floor(x0)), iy0 = std::max(0, (int) std::floor(y0));
        const int ix1 = std::min(w, (int) std::ceil(x1)), iy1 = std::min(h, (int) std::ceil(y1));
        for (int y = iy0; y < iy1; ++y) {
            const float py = y + .5f;
            if (py - .5f >= y0 + r && py + .5f <= y1 - r) {   // a middle row: full inside except the edge pixels
                const int a = std::max(ix0, (int) std::ceil(x0 + .5f)), b = std::min(ix1, (int) std::floor(x1 - .5f));
                for (int x = ix0; x < a; ++x) blend(x, y, c, std::clamp(.5f - sdBox(x + .5f, py, x0, y0, x1, y1, r), 0.f, 1.f) * alpha);
                if (alpha >= 1) { if (b > a) std::fill(&px[(size_t) y * w + a], &px[(size_t) y * w + b], c); }
                else for (int x = a; x < b; ++x) blend(x, y, c, alpha);
                for (int x = std::max(a, b); x < ix1; ++x) blend(x, y, c, std::clamp(.5f - sdBox(x + .5f, py, x0, y0, x1, y1, r), 0.f, 1.f) * alpha);
                continue;
            }
            for (int x = ix0; x < ix1; ++x)
                blend(x, y, c, std::clamp(.5f - sdBox(x + .5f, py, x0, y0, x1, y1, r), 0.f, 1.f) * alpha);
        }
    }
    // outline of a rounded rectangle (width t, inside the box): only the pixels near the edge are looked at
    void rframe(float x0, float y0, float x1, float y1, float r, float t, Px c, float alpha = 1) {
        y0 += ty; y1 += ty;
        r = std::max(0.f, std::min({ r, (x1 - x0) / 2, (y1 - y0) / 2 }));
        const int ix0 = std::max(0, (int) std::floor(x0)), iy0 = std::max(0, (int) std::floor(y0));
        const int ix1 = std::min(w, (int) std::ceil(x1)), iy1 = std::min(h, (int) std::ceil(y1));
        const float band = std::max(r, t) + 1;
        auto one = [&](int x, int y) {
            const float d = sdBox(x + .5f, y + .5f, x0, y0, x1, y1, r);
            blend(x, y, c, std::clamp(std::min(.5f - d, d + t + .5f), 0.f, 1.f) * alpha);
        };
        for (int y = iy0; y < iy1; ++y) {
            const bool edgeRow = y < y0 + band || y + 1 > y1 - band;
            if (edgeRow) { for (int x = ix0; x < ix1; ++x) one(x, y); continue; }
            for (int x = ix0; x < std::min(ix1, (int) std::ceil(x0 + t + 1)); ++x) one(x, y);
            for (int x = std::max(ix0, (int) std::floor(x1 - t - 1)); x < ix1; ++x) one(x, y);
        }
    }
    void disc(float cx, float cy, float r, Px c, float alpha = 1) {
        cy += ty;
        for (int y = std::max(0, (int) (cy - r - 1)); y < std::min(h, (int) (cy + r + 2)); ++y)
            for (int x = std::max(0, (int) (cx - r - 1)); x < std::min(w, (int) (cx + r + 2)); ++x) {
                const float d = std::hypot(x + .5f - cx, y + .5f - cy) - r;
                blend(x, y, c, std::clamp(.5f - d, 0.f, 1.f) * alpha);
            }
    }
    // an arc of a ring: centre, radius, thickness, from angle a0 to a1 in radians (0 = up, clockwise, both within
    // -pi..pi: the arcs of this UI never cross the bottom), round ends
    void arc(float cx, float cy, float R, float t, float a0, float a1, Px c, float alpha = 1) {
        cy += ty;
        if (a1 < a0) std::swap(a0, a1);
        const float ro = R + t / 2 + 1;
        const float e0x = cx + R * std::sin(a0), e0y = cy - R * std::cos(a0), e1x = cx + R * std::sin(a1), e1y = cy - R * std::cos(a1);
        for (int y = std::max(0, (int) (cy - ro)); y < std::min(h, (int) (cy + ro + 1)); ++y)
            for (int x = std::max(0, (int) (cx - ro)); x < std::min(w, (int) (cx + ro + 1)); ++x) {
                const float dx = x + .5f - cx, dy = y + .5f - cy, rr = dx * dx + dy * dy;
                if (rr > ro * ro || rr < (R - t / 2 - 1) * (R - t / 2 - 1)) continue;   // outside the ring
                const float a = std::atan2(dx, -dy);
                const float d = a >= a0 && a <= a1 ? std::fabs(std::hypot(dx, dy) - R) - t / 2
                    : std::min(std::hypot(x + .5f - e0x, y + .5f - e0y), std::hypot(x + .5f - e1x, y + .5f - e1y)) - t / 2;
                blend(x, y, c, std::clamp(.5f - d, 0.f, 1.f) * alpha);
            }
    }
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, Px c) {   // filled triangle, anti-aliased
        y0 += ty; y1 += ty; y2 += ty;
        const int ix0 = std::max(0, (int) std::min({ x0, x1, x2 })), ix1 = std::min(w, (int) std::max({ x0, x1, x2 }) + 2);
        const int iy0 = std::max(0, (int) std::min({ y0, y1, y2 })), iy1 = std::min(h, (int) std::max({ y0, y1, y2 }) + 2);
        const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0); const float s = area < 0 ? -1.f : 1.f;
        auto edge = [&](float ax, float ay, float bx, float by, float px_, float py) {
            const float l = std::hypot(bx - ax, by - ay); return s * ((bx - ax) * (py - ay) - (by - ay) * (px_ - ax)) / l; };
        for (int y = iy0; y < iy1; ++y)
            for (int x = ix0; x < ix1; ++x) {
                const float px_ = x + .5f, py = y + .5f;
                const float d = std::min({ edge(x0, y0, x1, y1, px_, py), edge(x1, y1, x2, y2, px_, py), edge(x2, y2, x0, y0, px_, py) });
                blend(x, y, c, std::clamp(d + .5f, 0.f, 1.f));
            }
    }

    // text: x is the left edge (align 0), the centre (1) or the right edge (2); y is the baseline
    float text(Font& f, float size, float x, float y, const std::string& s, Px c, int align = 0, float alpha = 1);
    float textWidth(Font& f, float size, const std::string& s);
    // the text cut with "…" so it fits in maxW
    std::string fit(Font& f, float size, const std::string& s, float maxW);
};

} // namespace ui
