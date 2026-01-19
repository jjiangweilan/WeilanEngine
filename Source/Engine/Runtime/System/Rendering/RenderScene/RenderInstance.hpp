#pragma once

#include "Engine/Library/Math.hpp"

class RenderInstance
{
public:
    virtual const AABB& GetAABB() const = 0;
    virtual void SetTransform(const float4x4& transform) = 0;
};
