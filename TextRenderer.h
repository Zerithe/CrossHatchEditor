#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <string>
#include <bgfx/bgfx.h>

// Creates a BGFX texture containing rendered text.
// The implementation uses stb_truetype to bake a font atlas, then blits glyphs
// for the given text string into an offscreen buffer that is converted to an RGBA texture.
// Make sure you have a TTF file at the specified path (adjust as needed).
bgfx::TextureHandle createTextTexture(const std::string& text);

#endif // TEXT_RENDERER_H
