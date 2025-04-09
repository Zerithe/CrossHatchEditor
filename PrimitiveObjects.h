#ifndef PRIMITIVE_OBJECTS_H
#define PRIMITIVE_OBJECTS_H

#include <vector>
#include <cmath>
#include <cstdint>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct PosColorVertex {
    float x, y, z;        // Position
    float nx, ny, nz;     // Normal
    uint32_t abgr;        // Color
    float u, v;  // texture coordinates
};

// Double–sided plane: 8 vertices (first 4 for front, next 4 for back)
static PosColorVertex planeVertices[] = {
    // Front face (normal pointing up, as before)
    { -10.0f, 0.0f, -10.0f, 0.0f,  1.0f,  0.0f, 0xff888888, 0.0f, 0.0f },
    {  10.0f, 0.0f, -10.0f, 0.0f,  1.0f,  0.0f, 0xff888888, 1.0f, 0.0f },
    { -10.0f, 0.0f,  10.0f, 0.0f,  1.0f,  0.0f, 0xff888888, 0.0f, 1.0f },
    {  10.0f, 0.0f,  10.0f, 0.0f,  1.0f,  0.0f, 0xff888888, 1.0f, 1.0f },
    // Back face (duplicate vertices with inverted normals)
    { -10.0f, 0.0f, -10.0f, 0.0f, -1.0f,  0.0f, 0xff888888, 0.0f, 0.0f },
    {  10.0f, 0.0f, -10.0f, 0.0f, -1.0f,  0.0f, 0xff888888, 1.0f, 0.0f },
    { -10.0f, 0.0f,  10.0f, 0.0f, -1.0f,  0.0f, 0xff888888, 0.0f, 1.0f },
    {  10.0f, 0.0f,  10.0f, 0.0f, -1.0f,  0.0f, 0xff888888, 1.0f, 1.0f }
};

static const uint16_t planeIndices[] = {
    // Front face uses vertices 0-3:
    0, 1, 2,
    1, 3, 2,
    // Back face uses vertices 4-7 (note reversed winding order for proper back–face orientation):
    4, 6, 5,
    5, 6, 7
};

// Define 24 vertices for the cube (4 vertices per face for 6 faces)
static PosColorVertex cubeVertices[] = {
    // Front face (z = +1)
    // Position              Normal         Color         UV
    { -1.0f,  1.0f,  1.0f,    0, 0, 1,      0xffffffff,  0.0f, 0.0f },
    {  1.0f,  1.0f,  1.0f,    0, 0, 1,      0xffffffff,  1.0f, 0.0f },
    {  1.0f, -1.0f,  1.0f,    0, 0, 1,      0xffffffff,  1.0f, 1.0f },
    { -1.0f, -1.0f,  1.0f,    0, 0, 1,      0xffffffff,  0.0f, 1.0f },

    // Back face (z = -1)
    {  1.0f,  1.0f, -1.0f,    0, 0,-1,      0xffffffff,  0.0f, 0.0f },
    { -1.0f,  1.0f, -1.0f,    0, 0,-1,      0xffffffff,  1.0f, 0.0f },
    { -1.0f, -1.0f, -1.0f,    0, 0,-1,      0xffffffff,  1.0f, 1.0f },
    {  1.0f, -1.0f, -1.0f,    0, 0,-1,      0xffffffff,  0.0f, 1.0f },

    // Top face (y = +1)
    { -1.0f,  1.0f, -1.0f,    0, 1, 0,      0xffffffff,  0.0f, 0.0f },
    {  1.0f,  1.0f, -1.0f,    0, 1, 0,      0xffffffff,  1.0f, 0.0f },
    {  1.0f,  1.0f,  1.0f,    0, 1, 0,      0xffffffff,  1.0f, 1.0f },
    { -1.0f,  1.0f,  1.0f,    0, 1, 0,      0xffffffff,  0.0f, 1.0f },

    // Bottom face (y = -1)
    { -1.0f, -1.0f,  1.0f,    0,-1, 0,      0xffffffff,  0.0f, 0.0f },
    {  1.0f, -1.0f,  1.0f,    0,-1, 0,      0xffffffff,  1.0f, 0.0f },
    {  1.0f, -1.0f, -1.0f,    0,-1, 0,      0xffffffff,  1.0f, 1.0f },
    { -1.0f, -1.0f, -1.0f,    0,-1, 0,      0xffffffff,  0.0f, 1.0f },

    // Left face (x = -1)
    { -1.0f,  1.0f, -1.0f,   -1, 0, 0,      0xffffffff,  0.0f, 0.0f },
    { -1.0f,  1.0f,  1.0f,   -1, 0, 0,      0xffffffff,  1.0f, 0.0f },
    { -1.0f, -1.0f,  1.0f,   -1, 0, 0,      0xffffffff,  1.0f, 1.0f },
    { -1.0f, -1.0f, -1.0f,   -1, 0, 0,      0xffffffff,  0.0f, 1.0f },

    // Right face (x = +1)
    {  1.0f,  1.0f,  1.0f,    1, 0, 0,      0xffffffff,  0.0f, 0.0f },
    {  1.0f,  1.0f, -1.0f,    1, 0, 0,      0xffffffff,  1.0f, 0.0f },
    {  1.0f, -1.0f, -1.0f,    1, 0, 0,      0xffffffff,  1.0f, 1.0f },
    {  1.0f, -1.0f,  1.0f,    1, 0, 0,      0xffffffff,  0.0f, 1.0f }
};


static const uint16_t cubeIndices[] = {
    // Front face
    0, 1, 2,  0, 2, 3,
    // Back face
    4, 5, 6,  4, 6, 7,
    // Top face
    8, 9,10,  8,10,11,
    // Bottom face
    12,13,14, 12,14,15,
    // Left face
    16,17,18, 16,18,19,
    // Right face
    20,21,22, 20,22,23
};

static PosColorVertex arrowVertices[] = {
    // Shaft (rectangular prism)
    {-0.5f, -0.05f,  0.05f, 0xff0000ff}, // Left bottom front (0)
    {-0.5f, -0.05f, -0.05f, 0xff0000ff}, // Left bottom back (1)
    {-0.5f,  0.05f, -0.05f, 0xff0000ff}, // Left top back (2)
    {-0.5f,  0.05f,  0.05f, 0xff0000ff}, // Left top front (3)

    { 0.3f, -0.05f,  0.05f, 0xff0000ff}, // Right bottom front (4)
    { 0.3f, -0.05f, -0.05f, 0xff0000ff}, // Right bottom back (5)
    { 0.3f,  0.05f, -0.05f, 0xff0000ff}, // Right top back (6)
    { 0.3f,  0.05f,  0.05f, 0xff0000ff}, // Right top front (7)

    // Arrowhead (pyramid)
    { 0.3f, -0.1f,  0.1f, 0xff0000ff},   // Base bottom front (8)
    { 0.3f, -0.1f, -0.1f, 0xff0000ff},   // Base bottom back (9)
    { 0.3f,  0.1f, -0.1f, 0xff0000ff},   // Base top back (10)
    { 0.3f,  0.1f,  0.1f, 0xff0000ff},   // Base top front (11)
    { 0.5f,  0.0f,  0.0f, 0xff0000ff}    // Tip (12)
};

// Define the indices for triangles
static const uint16_t arrowIndices[] = {
    // Shaft quads (each quad is made of 2 triangles)
    0, 1, 2, 2, 3, 0,  // Left face
    4, 5, 6, 6, 7, 4,  // Right face
    0, 1, 5, 5, 4, 0,  // Bottom face
    3, 2, 6, 6, 7, 3,  // Top face
    1, 2, 6, 6, 5, 1,  // Back face
    0, 3, 7, 7, 4, 0,  // Front face

    // Arrowhead triangles (pyramid faces)
    8, 9, 12,          // Bottom face
    9, 10, 12,         // Back face
    10, 11, 12,        // Top face
    11, 8, 12,         // Front face
    8, 11, 10, 10, 9, 8 // Base face (made of 2 triangles)
};

void generateCapsule(float radius, float halfHeight, int stacks, int sectors,
    std::vector<PosColorVertex>& vertices,
    std::vector<uint16_t>& indices)
{
    const float PI = 3.14159265359f;

    // Top hemisphere
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = (PI / 2) - (i * PI / stacks);
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle) + halfHeight;

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * (2 * PI / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // Calculate normal
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;

            uint32_t color = 0xffff0000; // Red for top hemisphere
            vertices.push_back({ x, y, z, nx, ny, nz, color });
        }
    }

    // Cylinder
    for (int i = 0; i <= 1; ++i) {
        float z = (i == 0) ? halfHeight : -halfHeight;

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * (2 * PI / sectors);
            float x = radius * cosf(sectorAngle);
            float y = radius * sinf(sectorAngle);

            // Normal same as position for cylinder
            float nx = x / radius;
            float ny = y / radius;
            float nz = 0.0f;

            uint32_t color = (i == 0) ? 0xff00ff00 : 0xff0000ff; // Green for top, Blue for bottom
            vertices.push_back({ x, y, z, nx, ny, nz, color });
        }
    }

    // Bottom hemisphere
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = (-PI / 2) + (i * PI / stacks);
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle) - halfHeight;

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * (2 * PI / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // Calculate normal
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;

            uint32_t color = 0xffffff00; // Yellow for bottom hemisphere
            vertices.push_back({ x, y, z, nx, ny, nz, color });
        }
    }

    // Generate indices
    int k1, k2;

    // Top hemisphere indices
    for (int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) { // Skip first stack (pole vertex)
                indices.push_back(k1);
                indices.push_back(k1 + 1);
                indices.push_back(k2); // Reverse: k2 comes last
            }

            if (i != (stacks - 1)) { // Skip last stack (pole vertex)
                indices.push_back(k1 + 1);
                indices.push_back(k2 + 1);
                indices.push_back(k2); // Reverse: k2 comes last
            }
        }
    }

    // Cylinder indices
    int offset = (stacks + 1) * (sectors + 1);
    for (int i = 0; i < 1; ++i) {
        k1 = offset + i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            indices.push_back(k1);
            indices.push_back(k1 + 1);
            indices.push_back(k2); // Reverse: k2 comes last

            indices.push_back(k1 + 1);
            indices.push_back(k2 + 1);
            indices.push_back(k2); // Reverse: k2 comes last
        }
    }

    // Bottom hemisphere indices
    offset += 2 * (sectors + 1);
    for (int i = 0; i < stacks; ++i) {
        k1 = offset + i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k1 + 1);
                indices.push_back(k2); // Reverse: k2 comes last
            }

            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2 + 1);
                indices.push_back(k2); // Reverse: k2 comes last
            }
        }
    }
}
//static PosColorVertex cylinderVertices[] = {
//    {  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, 0xffff0000},
//    {  1.0f,  1.0f,  0.0f,  0.707f, 0.707f,  0.0f, 0xff00ff00},
//    { -1.0f,  1.0f,  0.0f, -0.707f, 0.707f,  0.0f, 0xff0000ff},
//    {  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 0xffffff00},
//    {  1.0f, -1.0f,  0.0f,  0.707f, -0.707f,  0.0f, 0xff00ffff},
//    { -1.0f, -1.0f,  0.0f, -0.707f, -0.707f,  0.0f, 0xff888888}
//};
//
//static const uint16_t cylinderIndices[] = {
//    0, 1, 2,
//    3, 5, 4,
//    1, 4, 2,
//    2, 4, 5
//};

void generateCylinder(float radius, float height, size_t resolution,
    std::vector<PosColorVertex>& vertices,
    std::vector<uint16_t>& indices)
{
    float halfHeight = height * 0.5f;
    float angleStep = 2.0f * M_PI / resolution;

    std::vector<uint16_t> bottomVertices;
    std::vector<uint16_t> topVertices;

    // Generate bottom and top circle vertices
    for (size_t i = 0; i < resolution; i++) {
        float angle = i * angleStep;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        float nx = cos(angle);
        float nz = sin(angle);

        // Bottom vertex
        uint16_t vBottom = vertices.size();
        vertices.push_back({ x, -halfHeight, z, nx, 0, nz, 0xffffffff, (float)i / resolution, 0.0f });
        bottomVertices.push_back(vBottom);

        // Top vertex
        uint16_t vTop = vertices.size();
        vertices.push_back({ x, halfHeight, z, nx, 0, nz, 0xffffffff, (float)i / resolution, 1.0f });
        topVertices.push_back(vTop);
    }

    // Generate side faces
    for (size_t i = 0; i < resolution; i++) {
        size_t next = (i + 1) % resolution;
        indices.push_back(bottomVertices[i]);
        indices.push_back(bottomVertices[next]);
        indices.push_back(topVertices[i]);

        indices.push_back(topVertices[i]);
        indices.push_back(bottomVertices[next]);
        indices.push_back(topVertices[next]);
    }

    // Center vertices for caps
    uint16_t topCenter = vertices.size();
    vertices.push_back({ 0, halfHeight, 0, 0, 1, 0, 0xffffffff, 0.5f, 0.5f });
    uint16_t bottomCenter = vertices.size();
    vertices.push_back({ 0, -halfHeight, 0, 0, -1, 0, 0xffffffff, 0.5f, 0.5f });

    // Top cap
    for (size_t i = 0; i < resolution; i++) {
        size_t next = (i + 1) % resolution;
        indices.push_back(topCenter);
        indices.push_back(topVertices[i]);
        indices.push_back(topVertices[next]);
    }

    // Reverse bottom vertices order for consistent face orientation
    std::reverse(bottomVertices.begin(), bottomVertices.end());

    // Bottom cap
    for (size_t i = 0; i < resolution; i++) {
        size_t next = (i + 1) % resolution;
        indices.push_back(bottomCenter);
        indices.push_back(bottomVertices[next]);
        indices.push_back(bottomVertices[i]);
    }
}


void generateSphere(float radius, int stacks, int sectors,
    std::vector<PosColorVertex>& vertices,
    std::vector<uint16_t>& indices)
{
    const float PI = 3.14159265359f;

    for (int i = 0; i <= stacks; ++i)
    {
        float stackAngle = PI / 2 - i * (PI / stacks);
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        // v goes from 0 at the top to 1 at the bottom
        // (adjust this if your shader expects the opposite)
        for (int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * (2 * PI / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;
            float u = (float)j / sectors;   // 0 to 1
            float v = (float)i / stacks;    // 0 to 1
            // Set vertex color to white so it does not tint the texture.
            uint32_t color = 0xffffffff;
            vertices.push_back({ x, y, z, nx, ny, nz, color, u, v });
        }
    }

    // Generate indices for each quad (two triangles per quad)
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < sectors; ++j)
        {
            int first = i * (sectors + 1) + j;
            int second = first + sectors + 1;

            // Reversed triangle order:
            indices.push_back(first);
            indices.push_back(first + 1);
            indices.push_back(second);

            indices.push_back(first + 1);
            indices.push_back(second + 1);
            indices.push_back(second);
        }
    }

}

static PosColorVertex cornellBoxVertices[] = {
    // Floor (white)
    {-1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},
    { 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},
    {-1.0f, -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},
    { 1.0f, -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},

    // Ceiling (white)
    {-1.0f,  1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},
    { 1.0f,  1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},
    {-1.0f,  1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},
    { 1.0f,  1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},

    // Back Wall (white)
    {-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},
    { 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},
    {-1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},
    { 1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},

    // Left Wall (red)
    {-1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},
    {-1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},
    {-1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},
    {-1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},

    // Right Wall (green)
    { 1.0f, -1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00},
    { 1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00},
    { 1.0f,  1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00},
    { 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00}
};

static const uint16_t cornellBoxIndices[] = {
    0, 1, 2, 1, 3, 2,  // Floor (front)
    2, 3, 0, 3, 1, 0,  // Floor (back)

    4, 6, 5, 5, 6, 7,  // Ceiling (front)
    6, 4, 7, 7, 4, 5,  // Ceiling (back)

    8, 9, 10, 9, 11, 10,  // Back wall (front)
    10, 11, 8, 11, 9, 8,  // Back wall (back)

    12, 13, 14, 13, 15, 14,  // Left wall (front)
    14, 15, 12, 15, 13, 12,  // Left wall (back)

    16, 18, 17, 17, 18, 19,  // Right wall (front)
    18, 16, 19, 19, 16, 17  // Right wall (back)
};

// Cube for inside the Cornell Box (white)
static PosColorVertex innerCubeVertices[] = {
    {-0.3f, -1.0f, -0.3f, -0.577f, -0.577f, -0.577f, 0xffffffff},
    { 0.3f, -1.0f, -0.3f,  0.577f, -0.577f, -0.577f, 0xffffffff},
    {-0.3f, -0.4f, -0.3f, -0.577f,  0.577f, -0.577f, 0xffffffff},
    { 0.3f, -0.4f, -0.3f,  0.577f,  0.577f, -0.577f, 0xffffffff},
    {-0.3f, -1.0f,  0.3f, -0.577f, -0.577f,  0.577f, 0xffffffff},
    { 0.3f, -1.0f,  0.3f,  0.577f, -0.577f,  0.577f, 0xffffffff},
    {-0.3f, -0.4f,  0.3f, -0.577f,  0.577f,  0.577f, 0xffffffff},
    { 0.3f, -0.4f,  0.3f,  0.577f,  0.577f,  0.577f, 0xffffffff}
};

static const uint16_t innerCubeIndices[] = {
    0, 1, 2, 1, 3, 2, // Front
    4, 6, 5, 5, 6, 7, // Back
    0, 2, 4, 4, 2, 6, // Left
    1, 5, 3, 5, 7, 3, // Right
    0, 4, 1, 4, 5, 1, // Bottom
    2, 3, 6, 6, 3, 7  // Top
};

static PosColorVertex cornellBoxFloorVertices[] = {
    // Floor (white)
    {-1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},
    { 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},
    {-1.0f, -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff},
    { 1.0f, -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff}
};
static const uint16_t cornellBoxFloorIndices[] = {
    0, 1, 2, 1, 3, 2  // Floor (front)
};
static PosColorVertex cornellBoxCeilingVertices[] = {
    // Ceiling (white)
    {-1.0f,  1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},
    { 1.0f,  1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},
    {-1.0f,  1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff},
    { 1.0f,  1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0xffffffff}
};
static const uint16_t cornellBoxCeilingIndices[] = {
    0, 2, 1, 1, 2, 3  // Ceiling (front)
};
static PosColorVertex cornellBoxBackVertices[] = {
    // Back Wall (white)
    {-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},
    { 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},
    {-1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff},
    { 1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}
};
static const uint16_t cornellBoxBackIndices[] = {
    2, 3, 0, 3, 1, 0  // Back wall (back)
};
static PosColorVertex cornellBoxRightVertices[] = {
    // Right Wall (red)
    {-1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},
    {-1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},
    {-1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000},
    {-1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0xffff0000}
};
static const uint16_t cornellBoxRightIndices[] = {
    2, 3, 0, 3, 1, 0  // Right wall (back)
};
static PosColorVertex cornellBoxLeftVertices[] = {
    // Left Wall (green)
    { 1.0f, -1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00},
    { 1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00},
    { 1.0f,  1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00},
    { 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0xff00ff00}
};
static const uint16_t cornellBoxLeftIndices[] = {
    2, 0, 3, 3, 0, 1  // Left wall (back)
};

// Generates a cone mesh with the given base radius, height, and number of sectors.
// The cone is oriented with its apex at (0, height, 0) and its base centered on the plane y = 0.
inline void generateCone(float radius, float height, int sectors,
    std::vector<PosColorVertex>& vertices,
    std::vector<uint16_t>& indices)
{
    const float PI = 3.14159265359f;

    // Apex vertex at (0, height, 0)
    PosColorVertex apex = { 0.0f, height, 0.0f,  0.0f, 1.0f, 0.0f, 0xffffffff, 0.5f, 1.0f };
    vertices.push_back(apex);

    // Base center vertex at (0, 0, 0)
    PosColorVertex baseCenter = { 0.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f, 0xffffffff, 0.5f, 0.5f };
    vertices.push_back(baseCenter);

    // Generate base perimeter vertices.
    // They will be stored starting at index 2.
    for (int i = 0; i <= sectors; ++i)
    {
        float angle = 2 * PI * i / sectors;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        PosColorVertex v;
        v.x = x;
        v.y = 0.0f;
        v.z = z;

        // Compute the side normal.
        // For a cone, a common approximation is to set the normal to (x, radius, z) normalized.
        float nx = x;
        float ny = radius;
        float nz = z;
        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        v.nx = nx / len;
        v.ny = ny / len;
        v.nz = nz / len;

        v.abgr = 0xffffffff;
        // Simple UV mapping based on angle.
        v.u = (cosf(angle) + 1.0f) * 0.5f;
        v.v = (sinf(angle) + 1.0f) * 0.5f;
        vertices.push_back(v);
    }

    // Create indices for the side surface.
    // The apex is vertex 0; base perimeter vertices start at index 2.
    for (int i = 0; i < sectors; ++i)
    {
        indices.push_back(0);
        indices.push_back(2 + i);
        indices.push_back(2 + i + 1);
    }

    // Create indices for the base (using a triangle fan centered at vertex 1).
    for (int i = 0; i < sectors; ++i)
    {
        indices.push_back(1);
        indices.push_back(2 + i + 1);
        indices.push_back(2 + i);
    }
}

#endif