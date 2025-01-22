#ifndef PRIMITIVE_OBJECTS_H
#define PRIMITIVE_OBJECTS_H

#include <vector>
#include <cmath>
#include <cstdint>

struct PosColorVertex {
    float x, y, z;        // Position
    float nx, ny, nz;     // Normal
    uint32_t abgr;        // Color
};

static PosColorVertex planeVertices[] = {
    { -10.0f, -5.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0xff888888 },
    {  10.0f, -5.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0xff888888 },
    { -10.0f, -5.0f,  10.0f, 0.0f, 1.0f, 0.0f, 0xff888888 },
    {  10.0f, -5.0f,  10.0f, 0.0f, 1.0f, 0.0f, 0xff888888 }
};

static const uint16_t planeIndices[] = {
    0, 1, 2,
    1, 3, 2
};

static PosColorVertex cubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, -0.577f,  0.577f,  0.577f, 0xff000000},
    { 1.0f,  1.0f,  1.0f,  0.577f,  0.577f,  0.577f, 0xff0000ff},
    {-1.0f, -1.0f,  1.0f, -0.577f, -0.577f,  0.577f, 0xff00ff00},
    { 1.0f, -1.0f,  1.0f,  0.577f, -0.577f,  0.577f, 0xff00ffff},
    {-1.0f,  1.0f, -1.0f, -0.577f,  0.577f, -0.577f, 0xffff0000},
    { 1.0f,  1.0f, -1.0f,  0.577f,  0.577f, -0.577f, 0xffff00ff},
    {-1.0f, -1.0f, -1.0f, -0.577f, -0.577f, -0.577f, 0xffffff00},
    { 1.0f, -1.0f, -1.0f,  0.577f, -0.577f, -0.577f, 0xffffffff}
};

static const uint16_t cubeIndices[] = {
    0, 1, 2, 1, 3, 2,
    4, 6, 5, 5, 6, 7,
    0, 2, 4, 4, 2, 6,
    1, 5, 3, 5, 7, 3,
    0, 4, 1, 4, 5, 1,
    2, 3, 6, 6, 3, 7
};

static PosColorVertex capsuleVertices[] = {
    {  0.0f,  1.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0xffff0000},
    { -0.5f,  1.0f,  0.0f, -0.707f, 0.707f,  0.0f, 0xff00ff00},
    {  0.5f,  1.0f,  0.0f,  0.707f, 0.707f,  0.0f, 0xff0000ff},
    {  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 0xffffff00},
    { -0.5f, -1.0f,  0.0f, -0.707f, -0.707f,  0.0f, 0xff00ffff},
    {  0.5f, -1.0f,  0.0f,  0.707f, -0.707f,  0.0f, 0xff888888}
};

static const uint16_t capsuleIndices[] = {
    0, 1, 2,
    1, 4, 2,
    2, 4, 5,
    3, 4, 5
};

static PosColorVertex cylinderVertices[] = {
    {  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, 0xffff0000},
    {  1.0f,  1.0f,  0.0f,  0.707f, 0.707f,  0.0f, 0xff00ff00},
    { -1.0f,  1.0f,  0.0f, -0.707f, 0.707f,  0.0f, 0xff0000ff},
    {  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 0xffffff00},
    {  1.0f, -1.0f,  0.0f,  0.707f, -0.707f,  0.0f, 0xff00ffff},
    { -1.0f, -1.0f,  0.0f, -0.707f, -0.707f,  0.0f, 0xff888888}
};

static const uint16_t cylinderIndices[] = {
    0, 1, 2,
    3, 5, 4,
    1, 4, 2,
    2, 4, 5
};

void generateSphere(float radius, int stacks, int sectors,
    std::vector<PosColorVertex>& vertices,
    std::vector<uint16_t>& indices) {
    const float PI = 3.14159265359f;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = PI / 2 - i * (PI / stacks);
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * (2 * PI / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // Calculate normal
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;

            uint32_t color = 0xff0000ff;
            if (z > 0) color = 0xff00ff00;

            vertices.push_back({ x, y, z, nx, ny, nz, color });
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            int first = i * (sectors + 1) + j;
            int second = first + sectors + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

#endif