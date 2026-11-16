// Where the screen's pixels go, and where touches come from.
//   openDrm   the Raspberry Pi's display (Touch Display 2, DSI) straight through KMS, no desktop needed, and its touch
//             panel through evdev. The panel is 720 x 1280 portrait: the 1280 x 720 picture is turned by 90 or 270
//             degrees (--rotate). A landscape monitor (HDMI) shows it unturned, centred.
//   openSdl   a window on a computer (Mac, Linux desktop), if the program was built with SDL2. The mouse is the finger.
#pragma once
#include "Canvas.h"
#include <functional>
#include <string>
#include <memory>

namespace ui {

struct Display {
    // what: 0 = finger down, 1 = moved, 2 = up (x, y in the 1280 x 720 picture), 3 = key (x = the character)
    using Input = std::function<void(int what, float x, float y)>;
    virtual ~Display() = default;
    virtual void show(const Canvas& c) = 0;     // waits for the display if it is still showing the last frame
    virtual bool poll(const Input& in) = 0;     // false when the window is closed
    virtual const char* name() const = 0;
};

std::unique_ptr<Display> openDrm(int rotate, std::string& why);   // nullptr (and why) if no display can be driven
std::unique_ptr<Display> openSdl(float zoom, std::string& why);

bool writePng(const Canvas& c, const std::string& file);

} // namespace ui
