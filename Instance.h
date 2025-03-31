// Instance.h
#pragma once

#include <vector>
#include <string>
#include <bgfx/bgfx.h>
#include "Light.h"
#include "SceneIO.h"

const float TAU = 6.28318530718f;
static int instanceCounter = 0;

struct TextureOption {
    std::string name;
    bgfx::TextureHandle handle;
};

struct MaterialParams
{
    float tiling[2];     // (tilingU, tilingV)
    float offset[2];     // (offsetU, offsetV)
    float albedo[4];     // (r, g, b, a)
};

struct LightAnimation {
    bool enabled = false;
    float amplitude[3] = { 6.0f, 0.0f, 0.0f };
    float frequency[3] = { 1.0f, 1.0f, 1.0f };
    float phase[3] = { 0.0f, 0.0f, 0.0f };
};

static std::vector<TextureOption> availableNoiseTextures;
static std::vector<TextureOption> availableTextures;

struct Instance {
    int id;
    std::string name;
    std::string type;
    float position[3];
    float rotation[3]; // Euler angles in radians (for X, Y, Z)
    float scale[3];    // Non-uniform scale for each axis
    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;
    bool selected = false;

    // Add an override object color (RGBA)
    float objectColor[4];
    // NEW: optional diffuse texture for the object.
    bgfx::TextureHandle diffuseTexture = BGFX_INVALID_HANDLE;

    // NEW: noise texture
    bgfx::TextureHandle noiseTexture = BGFX_INVALID_HANDLE;

    MaterialParams material;

    // --- New for lights ---
    bool isLight = false;
    LightProperties lightProps; // Valid if isLight == true.

    // NEW: Store the base (original) position for animation.
    float basePosition[3];

    // NEW: Animation parameters for lights.
    LightAnimation lightAnim;

    // NEW: For light objects only – determines if the debug visual (the sphere)
    // is drawn. (Default true.)
    bool showDebugVisual = true;

    std::vector<Instance*> children; // Hierarchy: child instances
    Instance* parent = nullptr;      // pointer to parent

    float inkColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // Declare variables to hold our crosshatch parameters:
    float epsilonValue = 0.02f;             // Outer Line Smoothness or Epsilon
    float strokeMultiplier = 1.0f;          // Outer Hatch Density or Stroke Multiplier
    float lineAngle1 = TAU / 8.0f;          // Outer Hatch Angle or Line Angle 1
    float lineAngle2 = TAU / 16.0f;         // Line Angle 2

    // These variables will hold the extra parameter values.
    float patternScale = 3.0f;              // Outer Hatch Scale or Pattern Scale
    float lineThickness = 0.3f;             // Outer Hatch Weight or Line Thickness

    // You can leave the remaining components as 0 (or later repurpose them)
    float transparencyValue = 1.0f;         // Hatch Opacity or Transparency
    int crosshatchMode = 2;                 // 0 = hatch ver 1.0, 1 = hatch ver 1.1, 2 = hatch ver 1.2, 3 = basic shader

    // These variables will hold the values for u_paramsLayer
    float layerPatternScale = 1.0f;         // Inner Hatch Scale or Layer Pattern Scale
    float layerStrokeMult = 0.250f;         // Inner Hatch Density or Layer Stroke Multiplier
    float layerAngle = 2.983f;              // Inner Hatch Angle or Layer Angle
    float layerLineThickness = 10.0f;       // Inner Hatch Weight or Layer Line Thickness

    //variables for rotating spot light
    float centerX = 0.0f;
    float centerZ = 0.0f;
    float radius = 5.0f;
    float rotationSpeed = 0.5f;
    float instanceAngle = 0.0f;

    Instance(int instanceId, const std::string& instanceName, const std::string& instanceType, float x, float y, float z, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh)
        : id(instanceId), name(instanceName), type(instanceType), vertexBuffer(vbh), indexBuffer(ibh)
    {
        position[0] = x;
        position[1] = y;
        position[2] = z;
        // Initialize with no rotation and uniform scale of 1
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        scale[0] = scale[1] = scale[2] = 1.0f;
        // Initialize the object color to white (no override)
        objectColor[0] = 1.0f; objectColor[1] = 1.0f; objectColor[2] = 1.0f; objectColor[3] = 1.0f;
        // For light objects, default to drawing the debug visual.
        showDebugVisual = true;

        material.tiling[0] = 1.0f;
        material.tiling[1] = 1.0f;
        material.offset[0] = 0.0f;
        material.offset[1] = 0.0f;
        material.albedo[0] = 1.0f; // r
        material.albedo[1] = 1.0f; // g
        material.albedo[2] = 1.0f; // b
        material.albedo[3] = 1.0f; // a

        noiseTexture = availableNoiseTextures[0].handle;
        // Store base position for animation.
        basePosition[0] = x; basePosition[1] = y; basePosition[2] = z;
    }
    void addChild(Instance* child) {
        children.push_back(child);
        child->parent = this;
    }
};

static Instance* selectedInstance = nullptr;
