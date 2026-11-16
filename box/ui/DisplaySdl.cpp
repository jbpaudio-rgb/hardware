// A window on a computer (built when CMake finds SDL2). The mouse is the finger; keys stand in for the front panel:
//   1-4 parts   e v b f t m q x i p pages (ENGINE VOICE PAGE2 FILTER ENV MOD SEQ FX MIXER PRESET)   [ ] knob 9   space play/stop
#include "Display.h"
#include <string>
#ifdef OCTAVIO_SDL
#include <SDL.h>

namespace ui {
namespace {
struct Sdl : Display {
    SDL_Window* win = nullptr; SDL_Renderer* ren = nullptr; SDL_Texture* tex = nullptr; float zoom = 1; bool down = false;
    ~Sdl() override { if (tex) SDL_DestroyTexture(tex); if (ren) SDL_DestroyRenderer(ren); if (win) SDL_DestroyWindow(win); SDL_Quit(); }
    const char* name() const override { return "SDL"; }
    bool open(float z, std::string& why) {
        zoom = z > 0 ? z : 1;
        if (SDL_Init(SDL_INIT_VIDEO) != 0) { why = SDL_GetError(); return false; }
        win = SDL_CreateWindow("Octavio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int) (1280 * zoom), (int) (720 * zoom), SDL_WINDOW_ALLOW_HIGHDPI);
        if (!win) { why = SDL_GetError(); return false; }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
        if (!ren) { why = SDL_GetError(); return false; }
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 1280, 720);
        return tex != nullptr;
    }
    void show(const Canvas& c) override {
        SDL_UpdateTexture(tex, nullptr, c.px.data(), c.w * 4);
        SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, nullptr, nullptr); SDL_RenderPresent(ren);
    }
    bool poll(const Input& in) override {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return false;
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { down = true; in(0, e.button.x / zoom, e.button.y / zoom); }
            else if (e.type == SDL_MOUSEMOTION && down) in(1, e.motion.x / zoom, e.motion.y / zoom);
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) { down = false; in(2, e.button.x / zoom, e.button.y / zoom); }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym < 128) in(3, (float) e.key.keysym.sym, 0);
        }
        return true;
    }
};
} // namespace

std::unique_ptr<Display> openSdl(float zoom, std::string& why) {
    auto d = std::make_unique<Sdl>(); if (!d->open(zoom, why)) return nullptr; return d;
}
} // namespace ui

#else
namespace ui {
std::unique_ptr<Display> openSdl(float, std::string& why) { why = "built without SDL2"; return nullptr; }
}
#endif
