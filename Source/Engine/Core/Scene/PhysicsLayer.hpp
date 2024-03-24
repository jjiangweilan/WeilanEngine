#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

enum class PhysicsLayer : JPH::ObjectLayer
{
    NON_MOVING = 0,
    MOVING = 1,
    NUM_LAYERS = 2
}; // namespace PhysicsLayer
