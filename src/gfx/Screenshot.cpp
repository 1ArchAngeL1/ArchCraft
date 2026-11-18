#include "gfx/Screenshot.h"

#include <cstdio>
#include <vector>

#include "gfx/gl.h"

bool saveScreenshot(const char* path, int pixelWidth, int pixelHeight) {
    if (pixelWidth <= 0 || pixelHeight <= 0) return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(pixelWidth) * pixelHeight * 4);
    // Default alignment is 4 bytes per row; RGBA is already a multiple of 4,
    // so no padding surprises here.
    glReadPixels(0, 0, pixelWidth, pixelHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::FILE* file = std::fopen(path, "wb");
    if (!file) return false;

    unsigned char header[18] = {};
    header[2] = 2;
    header[12] = static_cast<unsigned char>(pixelWidth & 0xFF);
    header[13] = static_cast<unsigned char>((pixelWidth >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(pixelHeight & 0xFF);
    header[15] = static_cast<unsigned char>((pixelHeight >> 8) & 0xFF);
    header[16] = 32;
    header[17] = 0x08;  // bottom-left origin: matches OpenGL's row order
    std::fwrite(header, 1, sizeof(header), file);

    for (size_t i = 0; i < pixels.size(); i += 4) {
        const unsigned char bgra[4] = {pixels[i + 2], pixels[i + 1], pixels[i], 255};
        std::fwrite(bgra, 1, 4, file);
    }
    std::fclose(file);
    return true;
}
