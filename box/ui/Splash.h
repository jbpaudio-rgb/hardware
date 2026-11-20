// The start-up and closing pictures. Opening: SINECRAFT large, in the blocky pixel lettering of the case's top (box/enclosure/inscriptions.py),
// and the OCTAVIO logo small in the bottom right corner (box/enclosure/logo-octavio.svg). Shown by `octavio-box
// --splash` while Linux starts (octavio-splash.service), then by octavio-box itself until its first frame.
// Closing: SINECRAFT small on top, THIS IS THE END large, fading to black when the synth is switched off.
#pragma once
#include "Canvas.h"
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ui {

// the pixel lettering: 5 x 7 (I: 3 wide, the space: 3 empty columns), one column between letters
inline const char* const* pixelGlyph(char ch) {
    static const char* S[] = { ".####", "#....", "#....", ".###.", "....#", "....#", "####." };
    static const char* I[] = { "###", ".#.", ".#.", ".#.", ".#.", ".#.", "###" };
    static const char* N[] = { "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#" };
    static const char* E[] = { "#####", "#....", "#....", "####.", "#....", "#....", "#####" };
    static const char* C[] = { ".####", "#....", "#....", "#....", "#....", "#....", ".####" };
    static const char* R[] = { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" };
    static const char* A[] = { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" };
    static const char* F[] = { "#####", "#....", "#....", "####.", "#....", "#....", "#...." };
    static const char* T[] = { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." };
    static const char* H[] = { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" };
    static const char* D[] = { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." };
    static const char* SP[] = { "...", "...", "...", "...", "...", "...", "..." };
    switch (ch) { case 'S': return S; case 'I': return I; case 'N': return N; case 'E': return E; case 'C': return C; case 'R': return R;
                  case 'A': return A; case 'F': return F; case 'T': return T; case 'H': return H; case 'D': return D; default: return SP; }
}
inline int pixelWidth(const char* text) { int w = 0; for (const char* p = text; *p; ++p) w += (int) std::strlen(pixelGlyph(*p)[0]) + (p[1] ? 1 : 0); return w; }
// the text centred on cx, its top at y, solid letters of px pixels a dot
inline void pixelText(Canvas& c, const char* text, float cx, float y, int px, Px col) {
    float x = std::round(cx - pixelWidth(text) * px / 2.f); y = std::round(y);   // whole pixels: no seams between the dots
    for (const char* p = text; *p; ++p) {
        const char* const* g = pixelGlyph(*p); const int gw = (int) std::strlen(g[0]);
        for (int r = 0; r < 7; ++r) for (int k = 0; k < gw; ++k)
            if (g[r][k] == '#') c.rect(x + k * px, y + r * px, x + (k + 1) * px, y + (r + 1) * px, col);
        x += (gw + 1) * px;
    }
}

// the OCTAVIO logo, small, in the bottom right corner: its outline (straight segments only) filled even-odd, 4 x 4
// samples a pixel
inline void drawLogo(Canvas& c) {
    static const char* LOGO = "M213.38 12.00 L212.62 12.62 L212.62 47.25 L212.88 47.62 L213.62 47.88 L224.12 47.88 L224.88 47.38 L224.88 12.50 L224.25 12.00ZM164.62 12.12 L164.38 12.75 L165.25 15.00 L166.00 16.25 L170.12 26.50 L171.75 29.75 L173.25 34.00 L174.00 35.25 L179.00 47.38 L179.88 47.88 L192.00 47.88 L192.62 47.62 L195.75 40.50 L196.88 37.25 L197.62 36.00 L199.12 31.75 L201.75 26.00 L204.00 20.00 L205.62 16.62 L206.12 14.88 L207.12 13.00 L207.12 12.38 L206.50 12.00 L195.50 12.00 L194.88 12.50 L187.12 34.50 L186.88 35.88 L186.00 36.62 L185.25 36.00 L177.25 12.50 L176.50 12.00ZM138.88 12.25 L137.12 15.88 L132.75 27.12 L131.38 29.88 L124.25 47.38 L125.12 47.88 L135.75 47.88 L136.75 47.38 L138.12 42.75 L138.88 42.00 L152.00 41.88 L152.88 42.50 L154.12 46.88 L155.00 47.75 L166.88 47.75 L167.25 47.12 L153.12 12.62 L152.38 12.00 L139.38 12.00ZM145.50 21.50 L146.50 22.75 L149.12 30.38 L149.88 31.75 L149.88 32.62 L149.38 33.00 L142.50 33.00 L141.75 32.38 L144.12 24.75 L144.75 23.50 L145.00 22.00ZM241.38 13.00 L237.75 15.25 L235.25 17.75 L233.12 21.25 L232.00 24.88 L231.50 29.50 L232.00 35.00 L233.12 38.62 L235.25 42.12 L238.00 44.88 L240.88 46.75 L245.88 48.38 L247.00 48.25 L247.75 48.88 L257.25 48.88 L257.88 48.25 L258.62 48.38 L263.38 46.88 L266.75 44.75 L269.00 42.50 L270.75 40.00 L271.62 38.12 L272.25 35.62 L272.88 35.00 L272.88 24.62 L272.50 24.38 L271.00 20.38 L269.62 18.12 L266.75 15.12 L263.75 13.25 L260.50 12.00 L257.12 11.50 L255.75 11.00 L248.62 11.00 L247.12 11.62 L245.62 11.62ZM250.25 18.88 L253.75 18.88 L256.75 20.00 L258.88 22.00 L260.00 24.12 L260.62 26.62 L260.62 32.25 L259.88 36.12 L258.75 38.25 L257.12 39.75 L254.00 41.00 L250.25 41.00 L247.38 39.62 L245.25 37.38 L244.00 33.88 L243.75 30.75 L244.12 25.38 L244.88 23.25 L246.00 21.50 L247.38 20.25ZM57.88 12.25 L54.12 14.12 L50.25 17.62 L48.12 21.25 L47.25 23.88 L46.62 29.25 L46.75 32.50 L47.75 37.25 L49.25 40.50 L51.00 42.88 L54.12 45.75 L58.12 47.75 L59.62 47.88 L60.62 48.38 L62.12 48.38 L62.50 48.88 L72.12 48.88 L72.50 48.38 L75.88 47.75 L78.12 46.75 L80.62 45.00 L82.75 42.75 L84.75 39.25 L85.25 37.75 L85.12 37.12 L84.50 36.62 L83.38 36.50 L82.50 36.00 L81.62 36.00 L79.12 35.12 L75.12 34.25 L74.62 34.62 L73.75 37.00 L71.38 39.88 L68.50 41.00 L65.12 41.00 L63.75 40.62 L62.12 39.75 L60.75 38.38 L59.38 35.38 L58.88 32.00 L59.12 26.00 L59.88 23.38 L60.88 21.75 L63.00 19.88 L65.25 18.88 L68.62 18.88 L70.50 19.50 L73.00 21.38 L74.25 23.62 L75.50 23.75 L78.50 22.88 L79.88 22.88 L84.62 21.75 L84.88 20.88 L83.75 18.75 L81.88 16.25 L79.75 14.25 L76.00 12.25 L74.38 12.00 L73.25 11.50 L71.75 11.50 L70.50 11.00 L63.38 11.00 L62.38 11.50 L60.62 11.50 L59.50 12.00ZM10.38 13.00 L6.75 15.25 L4.25 17.75 L2.12 21.25 L1.12 24.38 L0.50 29.25 L1.00 34.75 L2.12 38.62 L4.25 42.12 L7.00 44.88 L9.88 46.75 L14.88 48.38 L16.00 48.25 L16.75 48.88 L26.25 48.88 L26.88 48.25 L27.62 48.38 L32.38 46.88 L35.75 44.75 L38.00 42.50 L39.75 40.00 L40.75 37.88 L41.88 32.62 L41.88 26.75 L40.75 22.00 L38.62 18.12 L35.75 15.12 L32.75 13.25 L29.50 12.00 L26.12 11.50 L24.75 11.00 L17.62 11.00 L16.12 11.62 L14.62 11.62ZM19.25 18.88 L22.75 18.88 L25.75 20.00 L27.88 22.00 L29.00 24.12 L29.62 26.62 L29.62 32.25 L28.88 36.12 L27.75 38.25 L26.12 39.75 L23.00 41.00 L19.25 41.00 L16.38 39.62 L14.25 37.38 L13.00 33.88 L12.75 30.75 L13.12 25.38 L13.88 23.25 L15.00 21.50 L16.38 20.25ZM81.75 0.00 L81.75 10.25 L82.12 10.75 L98.12 10.88 L98.88 11.38 L99.00 47.50 L99.75 47.88 L115.25 47.75 L115.75 47.12 L115.62 11.62 L116.50 10.88 L132.50 10.75 L132.75 10.38 L132.75 0.00Z";
    const float vw = 274, vh = 49, lw = 190, s = lw / vw, lx = c.w - lw - 40, ly = c.h - vh * s - 36;
    std::vector<std::vector<float>> polys; std::vector<float> cur;
    for (const char* p = LOGO; *p;) {
        if (*p == 'M') { if (!cur.empty()) polys.push_back(cur); cur.clear(); ++p; }
        else if (*p == 'L' || *p == ' ') ++p;
        else if (*p == 'Z') { if (!cur.empty()) polys.push_back(cur); cur.clear(); ++p; }
        else { char* e; const float x = std::strtof(p, &e); p = e; const float y = std::strtof(p, &e); p = e; cur.push_back(lx + x * s); cur.push_back(ly + y * s); }
    }
    if (!cur.empty()) polys.push_back(cur);
    const int X0 = (int) lx, X1 = (int) (lx + lw) + 1, Y0 = (int) ly, Y1 = (int) (ly + vh * s) + 1;
    for (int y = Y0; y < Y1; ++y) for (int x = X0; x < X1; ++x) {
        int hits = 0;
        for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
            const float qx = x + (sx + .5f) / 4, qy = y + (sy + .5f) / 4; bool in = false;
            for (auto& P : polys) for (size_t i = 0, j = P.size() - 2; i < P.size(); j = i, i += 2) {
                const float xi = P[i], yi = P[i + 1], xj = P[j], yj = P[j + 1];
                if ((yi > qy) != (yj > qy) && qx < (xj - xi) * (qy - yi) / (yj - yi) + xi) in = !in;
            }
            hits += in;
        }
        if (hits) c.blend(x, y, rgb(150, 150, 146), hits / 16.f);
    }
}

constexpr Px SPLASH_WHITE = rgb(240, 240, 236);

// opening: SINECRAFT large in the middle
inline void drawSplash(Canvas& c) {
    c.clear(rgb(0, 0, 0));
    const int px = std::min((c.w - 160) / pixelWidth("SINECRAFT"), 22);
    pixelText(c, "SINECRAFT", c.w / 2.f, (c.h - 7 * px) / 2.f - px, px, SPLASH_WHITE);
    drawLogo(c);
}

// closing: SINECRAFT small at the top, THIS IS THE END large in the middle
inline void drawGoodbye(Canvas& c) {
    c.clear(rgb(0, 0, 0));
    pixelText(c, "SINECRAFT", c.w / 2.f, 70, 7, SPLASH_WHITE);
    const int px = std::min((c.w - 160) / pixelWidth("THIS IS THE END"), 22);
    pixelText(c, "THIS IS THE END", c.w / 2.f, (c.h - 7 * px) / 2.f, px, SPLASH_WHITE);
    drawLogo(c);
}

} // namespace ui
