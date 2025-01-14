#pragma once

#include <vector>
#include <string>
#include <bgfx/bgfx.h>
#include <bx/math.h>

class ObjLoader {
public:
    struct Vertex {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    static bool loadObj(const std::string& filepath,
        std::vector<Vertex>& vertices,
        std::vector<uint16_t>& indices);

    static bgfx::VertexBufferHandle createVertexBuffer(const std::vector<Vertex>& vertices);
    static bgfx::IndexBufferHandle createIndexBuffer(const std::vector<uint16_t>& indices);

private:
    static void computeNormals(std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
};