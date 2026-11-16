// A picture of the screen ("shot" command).
#include "Display.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"
#include <vector>

namespace ui {
bool writePng(const Canvas& c, const std::string& file) {
    std::vector<unsigned char> rgb((size_t) c.w * c.h * 3);
    for (size_t i = 0; i < c.px.size(); ++i) { rgb[i * 3] = (unsigned char) (c.px[i] >> 16); rgb[i * 3 + 1] = (unsigned char) (c.px[i] >> 8); rgb[i * 3 + 2] = (unsigned char) c.px[i]; }
    return stbi_write_png(file.c_str(), c.w, c.h, 3, rgb.data(), c.w * 3) != 0;
}
}
