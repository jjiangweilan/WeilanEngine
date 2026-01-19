#pragma once

#include "Engine/Library/Math.hpp"

class RenderInstance
{
public:
    virtual const AABB& GetAABB() const = 0;
};
