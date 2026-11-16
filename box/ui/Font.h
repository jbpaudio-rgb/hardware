// A TrueType font for the canvas (DejaVu Sans Condensed, in third_party/fonts).
#pragma once
#include <memory>
#include <cstddef>
#include <string>

namespace ui {

struct Font {
    struct Impl; std::unique_ptr<Impl> d;
    Font(); ~Font();
    bool load(const std::string& path);
    bool loadMemory(const unsigned char* data, size_t size);   // the browser build has no files
    bool ok() const;
};

} // namespace ui
