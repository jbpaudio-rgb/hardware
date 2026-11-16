// The Raspberry Pi's display through KMS (raw ioctls, no libdrm) and its touch panel through evdev. Linux only.
#include "Display.h"
#include <string>
#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace ui {

namespace {
template <typename T> uint64_t ptr(T* p) { return (uint64_t) (uintptr_t) p; }

struct Drm : Display {
    int fd = -1, rot = 90; uint32_t conn = 0, crtc = 0; drm_mode_modeinfo mode {};
    struct Buf { uint32_t handle = 0, fb = 0, pitch = 0; uint64_t size = 0; uint8_t* map = nullptr; } buf[2];
    int back = 1; bool pending = false;
    int pw = 0, ph = 0;            // the display's pixels
    int offx = 0, offy = 0;        // landscape monitor: where the picture sits
    bool turned = true;
    // touch
    int tfd = -1; int ax0 = 0, ax1 = 1, ay0 = 0, ay1 = 1; bool mt = false;
    int tx = 0, ty = 0; bool down = false, wasDown = false, movedSince = false;

    ~Drm() override {
        for (auto& b : buf) if (b.map) munmap(b.map, b.size);
        if (tfd >= 0) close(tfd);
        if (fd >= 0) close(fd);
    }
    const char* name() const override { return "KMS"; }

    bool open(int rotate, std::string& why) {
        rot = rotate == 270 ? 270 : 90;
        for (int n = 0; n < 8 && !conn; ++n) {
            const std::string dev = "/dev/dri/card" + std::to_string(n);
            const int f = ::open(dev.c_str(), O_RDWR | O_CLOEXEC); if (f < 0) continue;
            if (pick(f)) { fd = f; break; }
            close(f);
        }
        if (fd < 0) { why = "no connected display in /dev/dri (is dtoverlay=vc4-kms-dsi-ili9881-5inch set?)"; return false; }
        pw = mode.hdisplay; ph = mode.vdisplay;
        turned = pw < ph;   // a portrait panel: turn the picture
        if (!turned) { offx = std::max(0, (pw - 1280) / 2); offy = std::max(0, (ph - 720) / 2); }
        for (auto& b : buf) {
            drm_mode_create_dumb c {}; c.width = (uint32_t) pw; c.height = (uint32_t) ph; c.bpp = 32;
            if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &c) < 0) { why = "cannot create a frame buffer"; return false; }
            b.handle = c.handle; b.pitch = c.pitch; b.size = c.size;
            drm_mode_fb_cmd f {}; f.width = (uint32_t) pw; f.height = (uint32_t) ph; f.pitch = c.pitch; f.bpp = 32; f.depth = 24; f.handle = c.handle;
            if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &f) < 0) { why = "cannot add a frame buffer"; return false; }
            b.fb = f.fb_id;
            drm_mode_map_dumb m {}; m.handle = c.handle;
            if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &m) < 0) { why = "cannot map a frame buffer"; return false; }
            void* p = mmap(nullptr, b.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t) m.offset);
            if (p == MAP_FAILED) { why = "cannot map a frame buffer"; return false; }
            b.map = (uint8_t*) p; std::memset(b.map, 0, b.size);
        }
        drm_mode_crtc s {}; s.crtc_id = crtc; s.fb_id = buf[0].fb; s.set_connectors_ptr = ptr(&conn); s.count_connectors = 1;
        s.mode = mode; s.mode_valid = 1;
        if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &s) < 0) { why = std::string("cannot take the display (") + std::strerror(errno) + "): is a desktop running?"; return false; }
        openTouch();
        return true;
    }

    // the first connected connector with a mode, and a CRTC for it
    bool pick(int f) {
        drm_mode_card_res r {};
        if (ioctl(f, DRM_IOCTL_MODE_GETRESOURCES, &r) < 0 || !r.count_connectors) return false;
        std::vector<uint32_t> crtcs(r.count_crtcs), conns(r.count_connectors), encs(r.count_encoders), fbs(r.count_fbs);
        r.crtc_id_ptr = ptr(crtcs.data()); r.connector_id_ptr = ptr(conns.data()); r.encoder_id_ptr = ptr(encs.data()); r.fb_id_ptr = ptr(fbs.data());
        if (ioctl(f, DRM_IOCTL_MODE_GETRESOURCES, &r) < 0) return false;
        for (uint32_t cid : conns) {
            drm_mode_get_connector c {}; c.connector_id = cid;
            if (ioctl(f, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0) continue;
            if (c.connection != 1 || !c.count_modes) continue;
            std::vector<drm_mode_modeinfo> modes(c.count_modes); std::vector<uint32_t> props(c.count_props), ce(c.count_encoders);
            std::vector<uint64_t> pv(c.count_props);
            c.modes_ptr = ptr(modes.data()); c.props_ptr = ptr(props.data()); c.prop_values_ptr = ptr(pv.data()); c.encoders_ptr = ptr(ce.data());
            if (ioctl(f, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0) continue;
            mode = modes[0];
            for (auto& m : modes) if (m.type & DRM_MODE_TYPE_PREFERRED) { mode = m; break; }
            uint32_t want = 0;
            if (c.encoder_id) { drm_mode_get_encoder e {}; e.encoder_id = c.encoder_id; if (ioctl(f, DRM_IOCTL_MODE_GETENCODER, &e) == 0) want = e.crtc_id; }
            if (!want)
                for (uint32_t eid : ce) {
                    drm_mode_get_encoder e {}; e.encoder_id = eid; if (ioctl(f, DRM_IOCTL_MODE_GETENCODER, &e) < 0) continue;
                    for (size_t k = 0; k < crtcs.size() && !want; ++k) if (e.possible_crtcs & (1u << k)) want = crtcs[k];
                    if (want) break;
                }
            if (!want) continue;
            conn = cid; crtc = want; return true;
        }
        return false;
    }

    void openTouch() {
        DIR* d = opendir("/dev/input"); if (!d) return;
        std::vector<std::string> evs;
        while (auto* e = readdir(d)) if (std::strncmp(e->d_name, "event", 5) == 0) evs.push_back(std::string("/dev/input/") + e->d_name);
        closedir(d);
        for (auto& p : evs) {
            const int f = ::open(p.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC); if (f < 0) continue;
            unsigned long abs[(ABS_MAX + 1 + 8 * sizeof(long) - 1) / (8 * sizeof(long))] {};
            ioctl(f, EVIOCGBIT(EV_ABS, sizeof abs), abs);
            auto has = [&](int b) { return (abs[b / (8 * sizeof(long))] >> (b % (8 * sizeof(long)))) & 1; };
            if (!has(ABS_X) && !has(ABS_MT_POSITION_X)) { close(f); continue; }
            mt = !has(ABS_X);
            input_absinfo ix {}, iy {};
            ioctl(f, EVIOCGABS(mt ? ABS_MT_POSITION_X : ABS_X), &ix); ioctl(f, EVIOCGABS(mt ? ABS_MT_POSITION_Y : ABS_Y), &iy);
            ax0 = ix.minimum; ax1 = std::max(ix.maximum, ix.minimum + 1); ay0 = iy.minimum; ay1 = std::max(iy.maximum, iy.minimum + 1);
            tfd = f; return;
        }
    }

    // a touch on the display (its own pixels) -> the 1280 x 720 picture
    void toPicture(float x, float y, float& u, float& v) const {
        if (!turned) { u = x - (float) offx; v = y - (float) offy; return; }
        if (rot == 90) { u = y; v = 719 - x; }
        else { u = 1279 - y; v = x; }
    }

    void show(const Canvas& c) override {
        if (pending) {   // wait for the last flip (vertical blank)
            pollfd p { fd, POLLIN, 0 };
            if (::poll(&p, 1, 100) > 0) { char ev[1024]; (void) !read(fd, ev, sizeof ev); }
            pending = false;
        }
        Buf& b = buf[back];
        if (!turned) {
            for (int y = 0; y < std::min(720, ph); ++y) {
                uint32_t* row = (uint32_t*) (b.map + (size_t) (y + offy) * b.pitch) + offx;
                std::memcpy(row, &c.px[(size_t) y * c.w], sizeof(uint32_t) * (size_t) std::min(1280, pw));
            }
        } else {
            const int W = c.w, H = c.h;
            for (int y = 0; y < std::min(ph, W); ++y) {
                uint32_t* row = (uint32_t*) (b.map + (size_t) y * b.pitch);
                if (rot == 90) { const Px* src = &c.px[(size_t) y]; for (int x = 0; x < std::min(pw, H); ++x) row[x] = src[(size_t) (H - 1 - x) * W]; }
                else { const Px* src = &c.px[(size_t) (W - 1 - y)]; for (int x = 0; x < std::min(pw, H); ++x) row[x] = src[(size_t) x * W]; }
            }
        }
        drm_mode_crtc_page_flip f {}; f.crtc_id = crtc; f.fb_id = b.fb; f.flags = DRM_MODE_PAGE_FLIP_EVENT;
        if (ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &f) == 0) pending = true;
        else { drm_mode_crtc s {}; s.crtc_id = crtc; s.fb_id = b.fb; s.set_connectors_ptr = ptr(&conn); s.count_connectors = 1; s.mode = mode; s.mode_valid = 1; ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &s); }
        back ^= 1;
    }

    bool poll(const Input& in) override {
        if (tfd < 0) return true;
        input_event e[64];
        for (;;) {
            const ssize_t n = read(tfd, e, sizeof e); if (n <= 0) break;
            for (int i = 0; i < (int) (n / (ssize_t) sizeof(input_event)); ++i) {
                const auto& v = e[i];
                if (v.type == EV_ABS) {
                    if (v.code == ABS_X || v.code == ABS_MT_POSITION_X) { tx = v.value; movedSince = true; }
                    if (v.code == ABS_Y || v.code == ABS_MT_POSITION_Y) { ty = v.value; movedSince = true; }
                    if (mt && v.code == ABS_MT_TRACKING_ID) down = v.value >= 0;
                } else if (v.type == EV_KEY && v.code == BTN_TOUCH) down = v.value != 0;
                else if (v.type == EV_SYN && v.code == SYN_REPORT) {
                    const float x = (float) (tx - ax0) / (float) (ax1 - ax0) * (float) pw, y = (float) (ty - ay0) / (float) (ay1 - ay0) * (float) ph;
                    float u, w; toPicture(x, y, u, w);
                    if (down && !wasDown) in(0, u, w);
                    else if (down && movedSince) in(1, u, w);
                    else if (!down && wasDown) in(2, u, w);
                    wasDown = down; movedSince = false;
                }
            }
        }
        return true;
    }
};
} // namespace

std::unique_ptr<Display> openDrm(int rotate, std::string& why) {
    auto d = std::make_unique<Drm>();
    if (!d->open(rotate, why)) return nullptr;
    return d;
}

} // namespace ui

#else
namespace ui {
std::unique_ptr<Display> openDrm(int, std::string& why) { why = "KMS is Linux only"; return nullptr; }
}
#endif
