// Octavio Synth enclosure, generated with the Manifold CSG library. Writes three STL files:
//   octavio-capot.stl   top shell: front panel + walls (print it panel side down)
//   octavio-fond.stl    bottom plate with the Raspberry Pi bosses
//   octavio-cadre.stl   frame that clamps the touch display against the panel
//   octavio-inscriptions.stl  white labels inlaid in the panel (second filament, same origin as the capot)
//   octavio-capuchon-*.stl    raised Choc v1 keycap, three leg thicknesses to try
//   octavio-capuchons-*-plaque.stl + octavio-capuchons-texte.stl  the 14 caps with their white labels
//   octavio-bouton-test-*.stl, octavio-boutons-*-plaque.stl + octavio-boutons-traits.stl  the 10 knobs (3 bore sizes)
//
// Coordinates follow the front-panel board ("panel" mm): x to the right, y towards the player,
// so the front-panel parts are placed with the same numbers as in fpb/design.py.
// 3D: X = x, Y = -y, Z up (Z = 0 under the bottom plate).
#include <manifold/manifold.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace manifold;

// ---------------------------------------------------------------- parameters (mm)
constexpr double X0 = -4, X1 = 304, Y0 = -4, Y1 = 159;   // outside of the case, panel coordinates
constexpr double WALL = 2.5, CORNER_R = 6;
constexpr double FLOOR = 3;                              // bottom plate
constexpr double HIN = 39;                               // floor top -> panel underside
constexpr double PANEL = 3;                              // panel thickness (encoder nuts need <= 3)
constexpr double ZP = FLOOR + HIN;                       // panel underside
constexpr double ZTOP = ZP + PANEL;
constexpr double BOARD_GAP = 7;                          // panel underside -> front board top
// Touch Display 2, 5" (Raspberry Pi product brief, "Physical specification"), used in landscape: its long side along x.
// The glass sits in a pocket of the panel, flush with the top, on a ledge under its black border, and is pulled down onto
// that ledge by four M2.5 screws into the display's own Pi mounting posts (49 x 58 mm, the Raspberry Pi pattern).
constexpr double SCX = 83.75, SCY = 61.75;               // glass centre (panel mm)
constexpr double GL = 143.4, GW = 91.46, GR = 5;          // glass outline, corner radius
constexpr double LENS = 0.7;                             // cover glass thickness ("0.7 lens"): the pocket depth
constexpr double GCLR = 0.2;                             // pocket clearance around the glass, each side
constexpr double RH_L = 122.74, RH_W = 72.96;            // rear metal housing (centred across, 12.8 mm from the FPC end)
constexpr double RH_END = 12.8;                          // glass edge -> housing, FPC end
constexpr double SCREEN_DEPTH = 14.92;                   // glass top -> end of the Pi posts (the deepest part)
constexpr double PIPOST_FROM_END = 33.91;                // FPC-end glass edge -> first row of Pi posts
constexpr double OPEN_HX = RH_L / 2 + std::fabs(GL - RH_L - 2 * RH_END) / 2 + 1;   // opening under the glass, half sizes
constexpr double OPEN_HY = RH_W / 2 + 1;
constexpr double USB_X = 180, USB_EARS = 18;               // rear panel USB-C (SparkFun CAB-15455): centre x, M3 spacing
constexpr double SCREEN_GAP = 0.6;                       // posts -> bridge: the glass bottoms on its ledge first
constexpr double PI_BOSS = 4;                            // floor bosses under the Pi
constexpr double PI_STANDOFF = 11;                       // Pi -> HAT: standard 8.5 mm socket, passive heatsink <= 7 mm
constexpr double SEG = 48;                               // circle segments
constexpr double CHOC_TOP = 8.8;                         // board top -> top of a Choc keycap (switch 5 + cap; check on the real caps)
constexpr double WELL = 3.0 - (CHOC_TOP - 7.0) + 0.2;    // depth of the wells around the buttons: caps ~0.2 mm above the well floor
constexpr double CAP_RAISE = 1.5;                        // printed raised caps: top this far above the panel surface
constexpr double CAP_GAP = 0.25;                         // gap between a keycap and the panel, each side
constexpr double INLAY = 0.6;                            // depth of the white labels (3 layers of 0.2 mm)

// the Raspberry Pi 5 + HAT, turned 90 degrees: HAT x -> case y, HAT y -> case -x
constexpr double PI_XREF = 290;                          // case x of HAT y = 0
constexpr double PI_YREF = 19.8;                         // case y of HAT x = 0 (HAT edge at -1.2)
static double piX(double hy) { return PI_XREF - hy; }
static double piY(double hx) { return PI_YREF + hx; }

// ---------------------------------------------------------------- helpers
static Manifold box(double x0, double y0, double x1, double y1, double z0, double z1) {
    return Manifold::Cube({x1 - x0, y1 - y0, z1 - z0}).Translate({x0, -y1, z0});
}
static Manifold cyl(double x, double y, double d, double z0, double z1) {
    return Manifold::Cylinder(z1 - z0, d / 2, d / 2, (int)SEG).Translate({x, -y, z0});
}
static Manifold rbox(double x0, double y0, double x1, double y1, double r, double z0, double z1) {
    if (r <= 0) return box(x0, y0, x1, y1, z0, z1);
    Manifold m = box(x0 + r, y0, x1 - r, y1, z0, z1) + box(x0, y0 + r, x1, y1 - r, z0, z1);
    for (double cx : {x0 + r, x1 - r})
        for (double cy : {y0 + r, y1 - r}) m += cyl(cx, cy, 2 * r, z0, z1);
    return m;
}
// hole through the rear wall (along y), centre (x, z)
static Manifold rearHole(double x, double z, double d) {
    return Manifold::Cylinder(20, d / 2, d / 2, (int)SEG).Rotate(-90, 0, 0).Translate({x, -6, z});
}
static Manifold rearSlot(double x, double z, double w, double h) {
    const double r = h / 2;
    Manifold s = Manifold::Cube({w - 2 * r, 20, h}).Translate({x - w / 2 + r, -6, z - h / 2});
    for (double cx : {x - w / 2 + r, x + w / 2 - r})
        s += Manifold::Cylinder(20, r, r, (int)SEG).Rotate(-90, 0, 0).Translate({cx, -6, z});
    return s;
}

// label polygons written by inscriptions.py: sections start with "# NAME", then "n" + n points per contour
static std::vector<std::pair<std::string, Polygons>> readLabels(const char* path) {
    std::vector<std::pair<std::string, Polygons>> out;
    std::ifstream in(path); std::string tok;
    while (in >> tok) {
        if (tok == "#") { std::string name; in >> name; out.push_back({name, {}}); continue; }
        if (out.empty()) out.push_back({"", {}});
        SimplePolygon p(std::stoi(tok)); for (auto& v : p) in >> v.x >> v.y;
        out.back().second.push_back(p);
    }
    return out;
}
// the keycaps in the order of capuchons.txt: 10 page buttons, then the 4 part buttons
static void capPos(int i, double& x, double& y) { if (i < 10) { x = 21.5 + 19 * i; y = 132; } else { x = 180 + 30 * (i - 10); y = 100; } }

static void writeStl(const Manifold& m, const std::string& path) {
    MeshGL g = m.GetMeshGL();
    FILE* f = std::fopen(path.c_str(), "wb");
    char hdr[80] = "Octavio Synth enclosure";
    std::fwrite(hdr, 1, 80, f);
    const uint32_t n = (uint32_t)(g.triVerts.size() / 3);
    std::fwrite(&n, 4, 1, f);
    auto P = [&](uint32_t v, int k) { return g.vertProperties[v * g.numProp + k]; };
    for (uint32_t t = 0; t < n; ++t) {
        const uint32_t a = g.triVerts[3 * t], b = g.triVerts[3 * t + 1], c = g.triVerts[3 * t + 2];
        float u[3], w[3], nn[3];
        for (int k = 0; k < 3; ++k) { u[k] = P(b, k) - P(a, k); w[k] = P(c, k) - P(a, k); }
        nn[0] = u[1] * w[2] - u[2] * w[1]; nn[1] = u[2] * w[0] - u[0] * w[2]; nn[2] = u[0] * w[1] - u[1] * w[0];
        const float l = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]) + 1e-12f;
        for (float& x : nn) x /= l;
        std::fwrite(nn, 4, 3, f);
        for (uint32_t v : {a, b, c}) for (int k = 0; k < 3; ++k) { float x = P(v, k); std::fwrite(&x, 4, 1, f); }
        const uint16_t attr = 0;
        std::fwrite(&attr, 2, 1, f);
    }
    std::fclose(f);
    std::printf("%s: %u triangles, volume %.0f cm3, %s\n", path.c_str(), n, m.Volume() / 1000.0,
                m.Status() == Manifold::Error::NoError ? "ok" : "ERROR");
}


// ---------------------------------------------------------------- assembly view (simplified parts)
struct Part { std::string name; float r, g, b, a; Manifold m; };
static void writeParts(const std::vector<Part>& parts, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    const uint32_t np = (uint32_t)parts.size(); std::fwrite(&np, 4, 1, f);
    for (auto& p : parts) {
        MeshGL g = p.m.GetMeshGL();
        const uint32_t nl = (uint32_t)p.name.size(); std::fwrite(&nl, 4, 1, f); std::fwrite(p.name.data(), 1, nl, f);
        const float c[4] = {p.r, p.g, p.b, p.a}; std::fwrite(c, 4, 4, f);
        const uint32_t nt = (uint32_t)(g.triVerts.size() / 3); std::fwrite(&nt, 4, 1, f);
        for (uint32_t i = 0; i < g.triVerts.size(); ++i)
            for (int k = 0; k < 3; ++k) { float x = g.vertProperties[g.triVerts[i] * g.numProp + k]; std::fwrite(&x, 4, 1, f); }
    }
    std::fclose(f);
}

int main() {
    // ============================================================ top shell
    Manifold shell = rbox(X0, Y0, X1, Y1, CORNER_R, FLOOR, ZTOP)
                   - rbox(X0 + WALL, Y0 + WALL, X1 - WALL, Y1 - WALL, CORNER_R - WALL, FLOOR - 1, ZP);

    // corner posts down to the bottom plate (M3 heat-set inserts from below)
    const double CP[4][2] = {{1, 1}, {299, 1}, {1, 154}, {299, 154}};
    for (auto& p : CP) shell += cyl(p[0], p[1], 7, FLOOR, ZP + 0.1);

    // front-board posts (M3 inserts), holes H1..H6 of the board
    const double HB[6][2] = {{8, 113}, {8, 147}, {163, 8}, {292, 8}, {292, 147}, {163, 104}};
    for (auto& p : HB) shell += cyl(p[0], p[1], 6.5, ZP - BOARD_GAP, ZP + 0.1);

    // touch display: the bridge under it screws onto these posts
    const double FZ1 = ZTOP - SCREEN_DEPTH - SCREEN_GAP, FZ0 = FZ1 - 4;               // bridge (4 mm, printed flat)
    const double SP[3][2] = {{40, 11}, {127, 11}, {7, 61}};
    for (auto& p : SP) shell += cyl(p[0], p[1], 6, FZ1, ZP + 0.1);
    shell += cyl(157.25, 61, 3.0, FZ1, ZP + 0.1);   // narrow gap between screen and board

    // --- holes in the panel
    const double z0 = ZP - 1, z1 = ZTOP + 1;
    // screen: a pocket for the glass (flush), and under its border a ledge around an opening for the rear housing. The
    // opening leaves 1 mm round the housing whichever way the display is turned (FPC end left or right).
    Manifold cut = rbox(SCX - GL / 2 - GCLR, SCY - GW / 2 - GCLR, SCX + GL / 2 + GCLR, SCY + GW / 2 + GCLR, GR + GCLR, ZTOP - LENS, z1)
                 + rbox(SCX - OPEN_HX, SCY - OPEN_HY, SCX + OPEN_HX, SCY + OPEN_HY, 2, z0, ZTOP);
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 4; ++c) {
            const double cx = 180 + 30 * c, cy = 30 + 38 * r;
            cut += cyl(cx, cy, 7.3, z0, z1);                                          // PEC11R, M7 bushing
            for (int k = 0; k < 12; ++k) {                                            // ring LEDs (r = 11)
                const double a = (15 + 30 * k) * M_PI / 180;
                cut += cyl(cx + 11 * std::cos(a), cy + 11 * std::sin(a), 2.0, z0, z1);
            }
        }
    cut += cyl(232, 132, 7.3, z0, z1);                                                // main encoder
    cut += cyl(272, 132, 7.3, z0, z1);                                                // volume pot
    // one opening per keycap, CAP_GAP all around (caps 17.5 x 16.5), and a small hole above each for its LED
    auto capHole = [&](double cx, double cy) {
        return rbox(cx - 8.75 - CAP_GAP, cy - 8.25 - CAP_GAP, cx + 8.75 + CAP_GAP, cy + 8.25 + CAP_GAP, 1.5 + CAP_GAP, z0, z1); };
    for (int k = 0; k < 10; ++k) {
        cut += capHole(21.5 + 19 * k, 132);                                           // page buttons
        cut += cyl(21.5 + 19 * k, 121.5, 2.0, z0, z1);                                // their LEDs
    }
    for (int c = 0; c < 4; ++c) {
        cut += capHole(180 + 30 * c, 100);                                            // part buttons
        cut += cyl(180 + 30 * c, 89.5, 2.0, z0, z1);                                  // part LEDs
    }

    // --- wells: the panel is thinner around the Choc buttons so the caps come up flush
    // one L-shaped deck: the page-button row, joined to the part buttons, knob 9 and the volume pot
    // the same margin all round each group of keys: around the 10 page buttons, as much as above them (5.75 mm, where
    // their LEDs are); around the 4 part buttons, as much as right of button 4 (5.25 mm), so left of button 1 too
    shell -= rbox(7.0, 118.0, 284.0, 146.0, 4, ZTOP - WELL, ZTOP + 1)
           + rbox(166.0, 86.5, 284.0, 146.0, 4, ZTOP - WELL, ZTOP + 1)
           + box(158.0, 110.0, 166.25, 118.5, ZTOP - WELL, ZTOP + 1)
           - cyl(158.0, 110.0, 16, ZTOP - WELL - 1, ZTOP + 2);   // rounded inside corner (r = 8)

    // --- white labels under knob 9 and the volume pot (polygons from inscriptions.py)
    Manifold labels;
    {
        labels = Manifold::Extrude(readLabels("inscriptions.txt")[0].second, INLAY).Translate({0, 0, ZTOP - INLAY});
        // the rear wall: each connector's name above it (polygons in x / z, mirrored to read from behind), 0.6 mm into the wall
        labels += Manifold::Extrude(readLabels("arriere.txt")[0].second, INLAY).Rotate(90, 0, 0).Translate({0, -Y0, 0});
        shell -= labels;
    }

    // --- inserts in the posts
    for (auto& p : CP) cut += cyl(p[0], p[1], 4.0, FLOOR - 1, FLOOR + 6);
    for (auto& p : HB) cut += cyl(p[0], p[1], 4.0, ZP - BOARD_GAP - 1, ZP - BOARD_GAP + 5);
    for (auto& p : SP) cut += cyl(p[0], p[1], 3.5, FZ1 - 1, FZ1 + 5);
    cut += cyl(157.25, 61, 1.6, FZ1 - 1, FZ1 + 7);                                     // M2 self-tapping

    // --- rear panel (y = -4 .. -1.5), from the HAT and the Pi
    const double zHat = FLOOR + PI_BOSS + 1.6 + PI_STANDOFF + 1.6;                    // HAT top surface
    const double jackY[4] = {5.0, 18.0, 31.0, 48.3};                                  // MIDI IN, MIDI OUT, LINE IN, PHONES
    for (double jy : jackY) cut += rearHole(piX(jy - 0.9), zHat + 2.6, 6.6);
    cut += rearHole(205, FLOOR + 18, 9.6);                                            // LINE OUT L (1/4" panel jack)
    cut += rearHole(222, FLOOR + 18, 9.6);                                            // LINE OUT R
    // USB-C (power + data): SparkFun CAB-15455 panel-mount extension, screwed on from behind: a slot for the plug,
    // two M3 holes 18 mm apart (the cable's own spacing)
    cut += rearSlot(USB_X, FLOOR + 12, 12, 6.5);
    for (double ux : { USB_X - USB_EARS / 2, USB_X + USB_EARS / 2 }) cut += rearHole(ux, FLOOR + 12, 3.3);
    // (no microSD slot: the card stays in the Pi; files go over the USB-C cable, and the card is reached by opening the
    // bottom)
    // (no vents: the box runs cool enough without them)

    shell -= cut;

    // ============================================================ bottom plate
    Manifold plate = rbox(X0, Y0, X1, Y1, CORNER_R, 0, FLOOR);
    const double PH[4][2] = {{3.5, 3.5}, {61.5, 3.5}, {3.5, 52.5}, {61.5, 52.5}};      // Pi mounting holes (HAT coords)
    for (auto& h : PH) plate += cyl(piX(h[1]), piY(h[0]), 6.5, FLOOR - 0.1, FLOOR + PI_BOSS);
    Manifold pcut;
    for (auto& p : CP) {
        pcut += cyl(p[0], p[1], 3.4, -1, FLOOR + 1);                                 // M3 screws from below
        pcut += cyl(p[0], p[1], 6.5, -1, 1.6);                                       // head counterbore
    }
    for (auto& h : PH) pcut += cyl(piX(h[1]), piY(h[0]), 3.5, FLOOR, FLOOR + PI_BOSS + 1);   // M2.5 inserts
    for (double fx : {20.0, 280.0})                                                   // recesses for rubber feet
        for (double fy : {12.0, 143.0}) pcut += cyl(fx, fy, 12.5, -1, 1);
    plate -= pcut;

    // ============================================================ screen bridge (4 mm, printed flat)
    // a ring on the 4 posts and two bars under the display's Pi posts; four M2.5 x 8 screws through the bars into those
    // posts pull the glass down onto its ledge. The slots take the posts whichever way the display is turned.
    const double PPX0 = GL / 2 - PIPOST_FROM_END - 58, PPX1 = GL / 2 - PIPOST_FROM_END;   // post offsets from the centre
    Manifold frame = rbox(9, 13, 159.0, 107.5, 2, FZ0, FZ1) - rbox(18, 22, 149.5, 101.5, 2, FZ0 - 1, FZ1 + 1);
    for (double by : {SCY - 24.5, SCY + 24.5}) frame += box(9, by - 4.5, 159, by + 4.5, FZ0, FZ1);
    for (auto& p : SP) frame += cyl(p[0], p[1], 8, FZ0, FZ1);
    frame += box(36, 11, 44, 16, FZ0, FZ1) + box(123, 11, 131, 16, FZ0, FZ1) + box(7, 57, 12, 65, FZ0, FZ1);   // ears
    frame += cyl(157.25, 61, 5, FZ0, FZ1);
    for (auto& p : SP) frame -= cyl(p[0], p[1], 2.9, FZ0 - 1, FZ1 + 1);
    frame -= cyl(157.25, 61, 2.2, FZ0 - 1, FZ1 + 1);
    for (double by : {SCY - 24.5, SCY + 24.5})
        for (double sgn : {-1.0, 1.0}) {
            const double a = SCX + sgn * PPX0, b = SCX + sgn * PPX1;                       // the post in either orientation
            frame -= rbox(std::min(a, b) - 1.45, by - 1.45, std::max(a, b) + 1.45, by + 1.45, 1.45, FZ0 - 1, FZ1 + 1);
        }
    // the display (FPC end towards +x here), for the checks and the assembly view
    auto display = [&](double zTop) {
        const double hx1 = SCX + GL / 2 - RH_END, hx0 = hx1 - RH_L;
        Manifold d = rbox(SCX - GL / 2, SCY - GW / 2, SCX + GL / 2, SCY + GW / 2, GR, zTop - LENS, zTop)
                   + box(hx0, SCY - RH_W / 2, hx1, SCY + RH_W / 2, zTop - 8.5, zTop - LENS);
        for (double px : {SCX + PPX0, SCX + PPX1})
            for (double py : {SCY - 24.5, SCY + 24.5}) d += cyl(px, py, 5, zTop - SCREEN_DEPTH, zTop - 8.5);
        d += box(SCX - 16.3, SCY - 16.3, SCX + 16.3, SCY + 16.3, zTop - 12, zTop - 8.5);   // FPC connector and the like
        return d;
    };

    // ============================================================ clearance checks against the parts inside
    {
        SimplePolygon b;   // front-panel board outline (fpb/design.py), reversed because Y = -y
        const double O[6][2] = {{159, 4}, {296, 4}, {296, 151}, {4, 151}, {4, 108}, {159, 108}};
        for (int i = 5; i >= 0; --i) b.push_back({O[i][0], -O[i][1]});
        const double zb = ZP - BOARD_GAP;
        Manifold board = Manifold::Extrude({b}, 1.6 + 2.5).Translate({0, 0, zb - 1.6 - 2.5});      // board + solder tails
        Manifold above = Manifold::Extrude({b}, BOARD_GAP - 0.01).Translate({0, 0, zb});             // parts between board and panel
        for (auto& p : HB) above -= cyl(p[0], p[1], 7, zb - 1, ZP + 1);
        Manifold pi = box(piX(56.5) - 1, piY(-21), piX(0) + 1, piY(85) + 3, FLOOR + PI_BOSS, zHat + 7);
        Manifold screen = display(ZTOP);
        Manifold frameAt = frame;
        auto v = [](const Manifold& m) { return m.Volume(); };
        std::printf("interferences (mm3): capot/carte %.2f  capot/composants %.2f  capot/Pi %.2f  capot/ecran %.2f  "
                    "carte/Pi %.2f  cadre/Pi %.2f  cadre/carte %.2f  cadre/ecran %.2f\n",
                    v(shell ^ board), v(shell ^ above), v(shell ^ pi), v(shell ^ screen), v(board ^ pi), v(frameAt ^ pi), v(frameAt ^ board),
                    v(frameAt ^ screen));
        std::printf("ecran: vitre au ras du dessus, rebord de %.1f mm (bouts) / %.1f mm (cotes) sous la vitre, %.1f mm d'epaisseur\n",
                    GL / 2 - OPEN_HX - GCLR,
                    GW / 2 - OPEN_HY - GCLR, PANEL - LENS);
        std::printf("jeu entre le dessous de la carte de facade et le haut du HAT: %.1f mm\n", (zb - 1.6 - 2.5) - (zHat + 7));
        std::printf("jeu entre la carte de facade et les jacks 1/4 (corps de 13 mm): %.1f mm\n", (zb - 1.6 - 2.5) - (FLOOR + 18 + 6.5));
    }


    // ============================================================ assembly: where the boards sit in the case
    {
        std::vector<Part> P;
        const double zb = ZP - BOARD_GAP;
        SimplePolygon b;
        const double O[6][2] = {{159, 4}, {296, 4}, {296, 151}, {4, 151}, {4, 108}, {159, 108}};
        for (int i = 5; i >= 0; --i) b.push_back({O[i][0], -O[i][1]});
        Manifold fb = Manifold::Extrude({b}, 1.6).Translate({0, 0, zb - 1.6});
        for (auto& p : HB) fb -= cyl(p[0], p[1], 3.2, zb - 3, zb + 1);
        P.push_back({"Carte de facade", 0.10f, 0.45f, 0.25f, 1, fb});
        Manifold encs, knobs, caps, leds;
        auto enc = [&](double cx, double cy, double kd) {
            encs += box(cx - 6.2, cy - 6.7, cx + 6.2, cy + 6.7, zb, zb + 6.5) + cyl(cx, cy, 7, zb + 6.5, ZTOP + 2) + cyl(cx, cy, 6, ZTOP + 2, ZTOP + 14);
            knobs += cyl(cx, cy, kd, ZTOP + 1, ZTOP + 16);
        };
        for (int r = 0; r < 2; ++r) for (int c = 0; c < 4; ++c) {
            const double cx = 180 + 30 * c, cy = 30 + 38 * r; enc(cx, cy, 18);
            for (int k = 0; k < 12; ++k) { const double a = (15 + 30 * k) * M_PI / 180; leds += box(cx + 11 * std::cos(a) - 1, cy + 11 * std::sin(a) - 1, cx + 11 * std::cos(a) + 1, cy + 11 * std::sin(a) + 1, zb, zb + 0.8); }
        }
        enc(232, 132, 22);
        encs += box(272 - 5, 132 - 5, 272 + 5, 132 + 6, zb, zb + 6.5) + cyl(272, 132, 7, zb + 6.5, ZTOP + 2) + cyl(272, 132, 6, ZTOP + 2, ZTOP + 14);
        knobs += cyl(272, 132, 14, ZTOP + 1, ZTOP + 14);
        auto choc = [&](double cx, double cy) { encs += box(cx - 7, cy - 7, cx + 7, cy + 7, zb, zb + 5); caps += rbox(cx - 8.75, cy - 8.25, cx + 8.75, cy + 8.25, 1.5, zb + CHOC_TOP - 1.3, ZTOP + CAP_RAISE); };
        for (int k = 0; k < 10; ++k) choc(21.5 + 19 * k, 132);
        for (int c = 0; c < 4; ++c) choc(180 + 30 * c, 100);
        P.push_back({"Encodeurs et boutons", 0.25f, 0.25f, 0.28f, 1, encs});
        const auto capText = readLabels("capuchons.txt");
        Manifold capLab;
        for (size_t i = 0; i < capText.size(); ++i) {
            double x, y; capPos((int)i, x, y);
            capLab += Manifold::Extrude(capText[i].second, INLAY).Translate({x, -y, ZTOP + CAP_RAISE - INLAY + 0.02});
            caps -= Manifold::Extrude(capText[i].second, INLAY).Translate({x, -y, ZTOP + CAP_RAISE - INLAY});
        }
        {   // the caps must move 3 mm freely: check the shell against them at rest and pressed
            Manifold pressed = caps.Translate({0, 0, -3.0});
            std::printf("capot/capuchons: au repos %.2f mm3, enfonces %.2f mm3\n", (shell ^ caps).Volume(), (shell ^ pressed).Volume());
        }
        P.push_back({"Capuchons et boutons", 0.16f, 0.16f, 0.17f, 1, knobs + caps});
        P.push_back({"Inscriptions des capuchons", 1.0f, 1.0f, 1.0f, 1, capLab});
        P.push_back({"LED", 1.0f, 0.55f, 0.1f, 1, leds});
        Manifold scr = display(ZTOP - 0.02);
        P.push_back({"Ecran 5 po", 0.08f, 0.08f, 0.10f, 1, scr});
        P.push_back({"Cadre de l'ecran", 0.95f, 0.75f, 0.2f, 1, frame});
        // Raspberry Pi 5 (HAT coordinates -> case), passive heatsink, USB/Ethernet
        auto hbox = [&](double hx0, double hy0, double hx1, double hy1, double z0, double z1) {
            return box(piX(hy1), piY(hx0), piX(hy0), piY(hx1), z0, z1); };
        const double zPi = FLOOR + PI_BOSS;
        Manifold pi = hbox(0, 0, 85, 56, zPi, zPi + 1.6);
        for (auto& h : PH) pi -= cyl(piX(h[1]), piY(h[0]), 2.7, zPi - 1, zPi + 3);
        P.push_back({"Raspberry Pi 5", 0.05f, 0.55f, 0.20f, 1, pi});
        Manifold pip = hbox(20, 18, 40, 38, zPi + 1.6, zPi + 1.6 + 7)                         // heatsink
                     + hbox(70, 2, 88, 16, zPi + 1.6, zPi + 1.6 + 16) + hbox(70, 20, 88, 34, zPi + 1.6, zPi + 1.6 + 16)   // USB
                     + hbox(66, 38, 88, 54, zPi + 1.6, zPi + 1.6 + 13.5)                    // Ethernet
                     + hbox(7, 0, 58, 5, zPi + 1.6, zPi + 1.6 + 8.5 + 2.5);                 // GPIO header + socket
        P.push_back({"Pi : dissipateur, USB, Ethernet", 0.65f, 0.67f, 0.70f, 1, pip});
        for (auto& h : PH) P.back().m += cyl(piX(h[1]), piY(h[0]), 5, zPi + 1.6, zHat - 1.6);
        // HAT and its jacks
        Manifold hat = hbox(-21, 0, 65, 56.5, zHat - 1.6, zHat);
        P.push_back({"Carte principale (HAT)", 0.10f, 0.30f, 0.60f, 1, hat});
        Manifold jacks;
        for (double jy : jackY) jacks += hbox(-25.2, jy - 3.9, -21, jy + 2.1, zHat, zHat + 5.2) + hbox(-21, jy - 4.2, -2, jy + 4.0, zHat, zHat + 5.2);
        jacks += hbox(-16, 37.3, -8, 42.3, zHat, zHat + 6) + hbox(10, 9, 17, 17, zHat, zHat + 2);
        P.push_back({"Jacks et connecteurs", 0.20f, 0.20f, 0.22f, 1, jacks});
        Manifold big = Manifold::Cylinder(30, 6.5, 6.5, 32).Rotate(-90, 0, 0).Translate({205, -31.5, FLOOR + 18})
                     + Manifold::Cylinder(30, 6.5, 6.5, 32).Rotate(-90, 0, 0).Translate({222, -31.5, FLOOR + 18});
        P.push_back({"Jacks 1/4 (panneau)", 0.55f, 0.55f, 0.58f, 1, big});
        P.push_back({"Fond", 0.35f, 0.38f, 0.42f, 1, plate});
        P.push_back({"Inscriptions (blanc)", 1.0f, 1.0f, 1.0f, 1, labels.Translate({0, 0, 0.02})});
        P.push_back({"Capot", 0.12f, 0.12f, 0.13f, 0.35f, shell});
        writeParts(P, "assemblage.bin");
        std::printf("assemblage.bin: %zu pieces\n", P.size());
    }

    writeStl(shell, "octavio-capot.stl");
    writeStl(plate, "octavio-fond.stl");
    writeStl(frame.Translate({0, 0, -FZ0}), "octavio-cadre.stl");
    writeStl(labels, "octavio-inscriptions.stl");
    std::printf("ouvertures des capuchons: jeu de %.2f mm, cloison de %.2f mm entre deux boutons de page, %.2f mm vers les LED\n",
                CAP_GAP, 19 - 17.5 - 2 * CAP_GAP, (132 - 8.25 - CAP_GAP) - (121.5 + 1.0));
    std::printf("inscriptions: %.0f mm3, %.1f mm de profondeur\n", labels.Volume(), INLAY);

    // raised Choc v1 keycap, printed top down. The underside matches a standard MBK cap (top plate underside
    // 1.3 mm below a standard cap top, short skirt, two legs 5.7 mm apart), only the top is taller.
    {
        const double P = CHOC_TOP - 1.3;                         // plate underside above the board
        const double T = (ZTOP + CAP_RAISE) - (ZP - BOARD_GAP);  // cap top above the board
        auto rb = [](double w, double h, double r, double z0, double z1) { return rbox(-w / 2, -h / 2, w / 2, h / 2, r, z0, z1); };
        Manifold body = (rb(17.5, 16.5, 1.5, P, T - 0.6) + rb(16.3, 15.3, 0.9, T - 0.01, T)).Hull();
        body += rb(17.5, 16.5, 1.5, P - 1.2, P) - rb(15.5, 14.5, 0.5, P - 2, P);   // skirt
        for (double leg : {1.25, 1.30, 1.35}) {
            Manifold c = body;
            for (double lx : {-2.85, 2.85}) c += box(lx - leg / 2, -1.5, lx + leg / 2, 1.5, P - 3.0, P + 0.01);
            Manifold up = c;
            c = c.Rotate(180, 0, 0).Translate({0, 0, T});                // top on the bed
            char name[64]; std::snprintf(name, sizeof name, "octavio-capuchon-%.2f.stl", leg);
            writeStl(c, name);
            // the 14 caps on one plate (5 x 3, 22 mm pitch), labels cut 0.6 mm into the top
            const auto capText = readLabels("capuchons.txt");
            Manifold plate, text;
            for (size_t i = 0; i < capText.size(); ++i) {
                const double px = 22.0 * (i % 5), py = -22.0 * (i / 5);
                Manifold t = Manifold::Extrude(capText[i].second, INLAY).Translate({0, 0, T - INLAY});
                plate += (up - t).Rotate(180, 0, 0).Translate({px, py, T});
                if (leg == 1.30) text += t.Rotate(180, 0, 0).Translate({px, py, T});
            }
            std::snprintf(name, sizeof name, "octavio-capuchons-%.2f-plaque.stl", leg);
            writeStl(plate, name);
            if (leg == 1.30) writeStl(text, "octavio-capuchons-texte.stl");
        }
        std::printf("capuchons: %.1f mm de haut, %.1f mm au-dessus de la facade, %.1f mm au-dessus du creux\n",
                    T - (P - 3.0), CAP_RAISE, CAP_RAISE + WELL);
    }
    {   // --- the knobs, printed: 8 x 18 mm (the encoders with LED rings), 22 mm (knob 9), 14 mm (volume); knurled, a
        // chamfered top (a white line on the volume's only), a D bore for the 6 mm flatted shafts (4.5 mm across the flat). Printed top
        // on the plate (smooth top, bore upwards, no supports). The bore's fit depends on the printer: a test knob
        // per bore size (6.00 / 6.10 / 6.20), then the plate with the one that holds without forcing.
        auto knob = [](double d, double H, double bore, Manifold* line) {
            const double r = d / 2, ch = 1.0;
            Manifold k = Manifold::Cylinder(H - ch, r, r, 96) + Manifold::Cylinder(ch, r, r - ch, 96).Translate({0, 0, H - ch});
            const int n = d > 20 ? 30 : d > 16 ? 24 : 18;   // knurls
            for (int i = 0; i < n; ++i) { const double a = 2 * M_PI * i / n;
                k -= Manifold::Cylinder(H - ch - 1.5, 0.7, 0.7, 16).Translate({(r + 0.35) * std::cos(a), (r + 0.35) * std::sin(a), 1.5}); }
            const double depth = H - 2.0;   // the shaft's bore, from the bottom; the top 2 mm stay solid
            Manifold b = Manifold::Cylinder(depth, bore / 2, bore / 2, 48) - Manifold::Cube({bore, bore, depth}).Translate({-bore / 2, 4.5 - bore / 2 + (bore - 6.0) / 2, 0});
            k -= b;
            k -= Manifold::Cylinder(0.6, bore / 2 + 0.6, bore / 2, 48);   // a chamfer at the bore's mouth
            // the white line on top, on the flat's side: only on the volume knob (the encoders turn endlessly, a mark
            // on them would point anywhere)
            if (line) { Manifold l = Manifold::Cube({1.2, r - ch - r * 0.3, 0.6}).Translate({-0.6, r * 0.3, H - 0.6}); k -= l; *line = l; }
            return k;
        };
        auto flip = [](const Manifold& m, double H, double x, double y) { return m.Rotate(180, 0, 0).Translate({x, y, H}); };
        for (double bore : {6.00, 6.10, 6.20}) {
            char name[64]; std::snprintf(name, sizeof name, "octavio-bouton-test-%.2f.stl", bore);
            writeStl(flip(knob(14, 14, bore, nullptr), 14, 0, 0), name);
            Manifold plate, lines;
            struct K { double d, H; } ks[10] = {{18, 16}, {18, 16}, {18, 16}, {18, 16}, {18, 16}, {18, 16}, {18, 16}, {18, 16}, {22, 16}, {14, 14}};
            for (int i = 0; i < 10; ++i) {
                const double x = (i % 5) * 26.0, y = (i / 5) * 26.0; Manifold l;
                const bool vol = i == 9;   // the volume pot's knob: the only one with a line
                plate += flip(knob(ks[i].d, ks[i].H, bore, vol ? &l : nullptr), ks[i].H, x, y);
                if (vol) lines += flip(l, ks[i].H, x, y);
            }
            std::snprintf(name, sizeof name, "octavio-boutons-%.2f-plaque.stl", bore);
            writeStl(plate, name);
            if (bore == 6.10) writeStl(lines, "octavio-boutons-traits.stl");
        }
        std::printf("boutons: 8 x 18 mm, 22 mm, 14 mm ; alesage en D de 6 mm (essais 6.00 / 6.10 / 6.20)\n");
    }
    std::printf("creux autour des boutons: %.1f mm (facade %.1f mm a cet endroit), capuchons Choc standard %.1f mm au-dessus du fond du creux\n",
                WELL, PANEL - WELL, (ZP - BOARD_GAP + CHOC_TOP) - (ZTOP - WELL));
    std::printf("hauteur totale %.1f mm, HAT a z %.1f, prises arriere a z %.1f\n", ZTOP, zHat, zHat + 2.6);
    return 0;
}
