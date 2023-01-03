#pragma once
#include <cinttypes>
#include "glm/glm.hpp"
namespace Engine
{
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
        glm::vec3 min;
        glm::vec3 max;
    };
}
