#pragma once

#include <bx/math.h>

struct Camera
{
    bx::Vec3 position;
    bx::Vec3 front;
    bx::Vec3 up;
    bx::Vec3 right;
    float yaw;
    float pitch;

    Camera()
        : position{ 0.0f, 0.0f, 3.0f }
        , front{ 0.0f, 0.0f, -1.0f }
        , up{ 0.0f, 1.0f, 0.0f }
        , right{ 1.0f, 0.0f, 0.0f }
        , yaw{ -90.0f }   // Default facing negative Z
        , pitch{ 0.0f }
    {
    }
};