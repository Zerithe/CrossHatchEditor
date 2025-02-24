// Light.h
#pragma once

enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightProperties {
    LightType type = LightType::Point; // Default is point light.
    float direction[3] = { 0.0f, -1.0f, 0.0f }; // Only used for directional/spot.
    float intensity = 1.0f;
    float range = 10.0f;      // Used for point/spot lights.
    float coneAngle = 0.0f;   // Used only for spot lights (in radians).
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

