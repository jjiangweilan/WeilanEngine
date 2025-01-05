#pragma once
#include "Libs/Math.hpp"

struct Offset2D
{
    int32_t x;
    int32_t y;
};

struct Offset3D
{
    int32_t x;
    int32_t y;
    int32_t z;
};

struct Extent2D
{
    uint32_t width;
    uint32_t height;
};

struct Extent3D
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct Rect2D
{
    Offset2D offset;
    Extent2D extent;
};

struct AABB
{
    struct PosConstruct {};
    glm::float3 min = {};
    glm::float3 max = {};
    glm::float3 GetCenter() { return (min + max) / 2.0f; }

    AABB() : min(0), max(0) {}
    AABB(const glm::float3& min, const glm::float3& max) : min(min), max(max)
    { }
    AABB(const glm::float3& center, const glm::float3& size, PosConstruct)
    {
        glm::float3 halfSize = size / 2.0f;
        min = center - halfSize;
        max = center + halfSize;
    }

    void Transform(const glm::float3x3& rs, const glm::float3& t)
    {

        glm::float3 nmin, nmax;
        nmin = t;
        nmax = t;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
            {
                float a = rs[i][j] * min[j];
                float b = rs[i][j] * max[j];
                nmin[i] += a < b ? a : b;
                nmax[i] += a < b ? b : a;
            }

        min = nmin;
        max = nmax;
    }
};
