#ifndef PRIMITIVE_OBJECTS_H
#define PRIMITIVE_OBJECTS_H

#include <vector>
#include <cmath>
#include <cstdint>


struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
};

static PosColorVertex planeVertices[] =
{
    { -10.0f, -5.0f, -10.0f, 0xff888888 },  // Bottom-left
    {  10.0f, -5.0f, -10.0f, 0xff888888 },  // Bottom-right
    { -10.0f, -5.0f,  10.0f, 0xff888888 },  // Top-left
    {  10.0f, -5.0f,  10.0f, 0xff888888 },  // Top-right
};

static const uint16_t planeIndices[] =
{
    0, 1, 2,  // First triangle
    1, 3, 2   // Second triangle
};

static PosColorVertex cubeVertices[] =
{
    {-1.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {-1.0f, -1.0f,  1.0f, 0xff00ff00 },
    { 1.0f, -1.0f,  1.0f, 0xff00ffff },
    {-1.0f,  1.0f, -1.0f, 0xffff0000 },
    { 1.0f,  1.0f, -1.0f, 0xffff00ff },
    {-1.0f, -1.0f, -1.0f, 0xffffff00 },
    { 1.0f, -1.0f, -1.0f, 0xffffffff },
};


static const uint16_t cubeIndices[] =
{
    0, 1, 2,
    1, 3, 2,
    4, 6, 5,
    5, 6, 7,
    0, 2, 4,
    4, 2, 6,
    1, 5, 3,
    5, 7, 3,
    0, 4, 1,
    4, 5, 1,
    2, 3, 6,
    6, 3, 7,
};

static const uint16_t s_cubeTriStrip[] =
{
    0, 1, 2,
    3,
    7,
    1,
    5,
    0,
    4,
    2,
    6,
    7,
    4,
    5,
};

// Capsule top and bottom hemisphere vertices with body vertices
static const PosColorVertex capsuleVertices[] = {
    {  0.0f,  1.5f,  0.0f, 0xffff0000 }, // Top-center
    { -0.5f,  1.0f,  0.0f, 0xff00ff00 }, // Upper-left
    {  0.5f,  1.0f,  0.0f, 0xff0000ff }, // Upper-right
    {  0.0f, -1.0f,  0.0f, 0xffffff00 }, // Lower-left
    { -0.5f, -1.0f,  0.0f, 0xff00ffff }, // Bottom-left
    {  0.5f, -1.0f,  0.0f, 0xff888888 }  // Bottom-right
};

static const uint16_t capsuleIndices[] = {
    // Top hemisphere
    0, 1, 2,

    // Cylinder body
    1, 4, 2,
    2, 4, 5,

    // Bottom hemisphere
    3, 4, 5
};

static const PosColorVertex cylinderVertices[] = {
    // Top circle
    {  0.0f,  1.0f,  0.0f, 0xffff0000 }, // Top-center
    {  1.0f,  1.0f,  0.0f, 0xff00ff00 }, // Top-right
    { -1.0f,  1.0f,  0.0f, 0xff0000ff }, // Top-left

    // Bottom circle
    {  0.0f, -1.0f,  0.0f, 0xffffff00 }, // Bottom-center
    {  1.0f, -1.0f,  0.0f, 0xff00ffff }, // Bottom-right
    { -1.0f, -1.0f,  0.0f, 0xff888888 }  // Bottom-left
};

static const uint16_t cylinderIndices[] = {
    // Top face
    0, 1, 2,

    // Bottom face
    3, 5, 4,

    // Side faces
    1, 4, 2,
    2, 4, 5
};

// Function to generate a UV Sphere
void generateSphere(float radius, int stacks, int sectors,
    std::vector<PosColorVertex>& vertices,
    std::vector<uint16_t>& indices) {
    const float PI = 3.14159265359f;

    // Vertices
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = PI / 2 - i * (PI / stacks); // From +PI/2 to -PI/2
        float xy = radius * cosf(stackAngle);         // Radius of the circle
        float z = radius * sinf(stackAngle);          // Z-coordinate

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * (2 * PI / sectors); // From 0 to 2*PI

            // Vertex position
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // Assign a color based on position (example)
            uint32_t color = 0xff0000ff; // Blue
            if (z > 0) color = 0xff00ff00; // Green for top hemisphere

            // Add the vertex
            vertices.push_back({ x, y, z, color });
        }
    }

    // Indices
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            int first = i * (sectors + 1) + j;
            int second = first + sectors + 1;

            // First triangle of the quad
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            // Second triangle of the quad
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

#endif