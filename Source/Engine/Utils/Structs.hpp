#pragma once
#include "glm/glm.hpp"
#include <cinttypes>

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
    AABB():min(0), max(0){}
    AABB(const glm::vec3& center, const glm::vec3& size)
    {
        glm::vec3 halfSize = size / 2.0f;
        min = center - halfSize;
        max = center + halfSize;
    }
    glm::vec3 min;
    glm::vec3 max;

    void Transform(const glm::mat3& rs, const glm::vec3& t)
    {

        glm::vec3 nmin, nmax;
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
