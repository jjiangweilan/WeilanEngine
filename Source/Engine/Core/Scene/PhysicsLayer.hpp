#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

enum class PhysicsLayer : JPH::ObjectLayer
{
    Scene = 0,
    Moving = 1,
    NUM_LAYERS = 2
}; // namespace PhysicsLayer
