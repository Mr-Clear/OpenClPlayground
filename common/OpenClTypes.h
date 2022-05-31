#pragma once

#include <cmath>

struct int2
{
    int x;
    int y;
};

struct float2
{
    float x;
    float y;

    float2(float x, float y) : x(x), y(y) {}

    float2 operator-(const float2 &o) const
    {
        return {x - o.x, y - o.y};
    }

    [[nodiscard]] float length() const
    {
        return sqrtf(x * x + y * y);
    }
};
