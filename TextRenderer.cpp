#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "TextRenderer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <windows.h> 
#include <filesystem> 
#include <string>

std::string getExecutableDir()
{
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path().string();
#else
    return std::filesystem::current_path().string();
#endif
}

// Helper: load entire file into memory.
static unsigned char* loadFile(const char* fileName, int* size)
{
    FILE* fp = fopen(fileName, "rb");
    if (!fp) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char* buffer = (unsigned char*)malloc(*size);
    if (!buffer) {
        fclose(fp);
        return nullptr;
    }
    fread(buffer, 1, *size, fp);
    fclose(fp);
    return buffer;
}

bgfx::TextureHandle createTextTexture(const std::string& text)
{
    // Fixed resolution for our generated text texture.
    const int texWidth = 1024; //512 original | texWidth = 1024;  // Wider canvas for text
    const int texHeight = 256; //512 original | texHeight = 256;  // Shorter height
    const int fontSize = 64; //32 original | 64
    // Our temporary buffers: one for baking the atlas and one for compositing our text.
    std::vector<unsigned char> atlas(texWidth * texHeight, 0);
    std::vector<unsigned char> textBitmap(texWidth * texHeight, 0);

    // Load the TTF font.
    int fontBufferSize = 0;
    //const char* fontFile = "fonts\\SF_Arch_Rival.ttf"; // Adjust the font path as needed.
    std::string fontPath = getExecutableDir() + "/fonts/SF_Arch_Rival.ttf";
    unsigned char* fontBuffer = loadFile(fontPath.c_str(), &fontBufferSize);
    if (!fontBuffer) {
        std::cerr << "Could not load font file." << std::endl;
        return BGFX_INVALID_HANDLE;
    }

    // Bake the font atlas for ASCII 32..127.
    stbtt_bakedchar cdata[96]; // For characters 32..127.
    if (stbtt_BakeFontBitmap(fontBuffer, 0, fontSize, atlas.data(), texWidth, texHeight, 32, 96, cdata) <= 0) {
        std::cerr << "Failed to bake font bitmap." << std::endl;
        free(fontBuffer);
        return BGFX_INVALID_HANDLE;
    }

    // Starting drawing coordinates (with a little margin).
    float x = 0.0f;
    float y = (float)fontSize;

    // For each character in the input text, compute its quad and blit it from the atlas into textBitmap.
    for (char ch : text)
    {
        if (ch < 32 || ch >= 128) continue; // skip non-printable chars

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, texWidth, texHeight, ch - 32, &x, &y, &q, 1);

        // q.x0, q.y0, q.x1, q.y1 give the destination positions (in our textBitmap) for this glyph.
        // q.s0, q.t0, q.s1, q.t1 give the corresponding texture coordinates inside the atlas.
        int destX0 = (int)q.x0;
        int destY0 = (int)q.y0;
        int destX1 = (int)q.x1;
        int destY1 = (int)q.y1;

        // Compute source rect in the atlas.
        int srcX0 = (int)(q.s0 * texWidth);
        int srcY0 = (int)(q.t0 * texHeight);
        int srcX1 = (int)(q.s1 * texWidth);
        int srcY1 = (int)(q.t1 * texHeight);

        int glyphW = srcX1 - srcX0;
        int glyphH = srcY1 - srcY0;

        // Blit glyph from atlas to textBitmap.
        for (int j = 0; j < glyphH; j++) {
            for (int i = 0; i < glyphW; i++) {
                int destX = destX0 + i;
                int destY = destY0 + j;
                if (destX < 0 || destX >= texWidth || destY < 0 || destY >= texHeight)
                    continue;
                int srcX = srcX0 + i;
                int srcY = srcY0 + j;
                if (srcX < 0 || srcX >= texWidth || srcY < 0 || srcY >= texHeight)
                    continue;
                unsigned char value = atlas[srcY * texWidth + srcX];
                // For simplicity, we overwrite the destination pixel.
                textBitmap[destY * texWidth + destX] = value;
            }
        }
    }

    // Convert the 8-bit grayscale textBitmap into an RGBA buffer.
    std::vector<uint32_t> rgbaBuffer(texWidth * texHeight, 0);
    for (int i = 0; i < texWidth * texHeight; i++) {
        // We want white text modulated by the glyph’s alpha.
        // In BGRA8, the layout is: B, G, R, A (little-endian packing).
        unsigned char alpha = textBitmap[i];
        uint32_t pixel = (alpha << 24) | (255 << 16) | (255 << 8) | 255;
        rgbaBuffer[i] = pixel;
    }

    // Create a BGFX memory block from the RGBA buffer.
    const bgfx::Memory* mem = bgfx::copy(rgbaBuffer.data(), texWidth * texHeight * sizeof(uint32_t));

    // Create a BGFX texture (using BGRA8 format).
    bgfx::TextureHandle texHandle = bgfx::createTexture2D(
        static_cast<uint16_t>(texWidth),
        static_cast<uint16_t>(texHeight),
        false, // No mipmaps for simplicity.
        1,     // One layer.
        bgfx::TextureFormat::BGRA8,
        0,     // No special flags.
        mem
    );

    free(fontBuffer);
    return texHandle;
}
