#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

enum class PhysicsLayer : JPH::ObjectLayer
{
    Scene = 0,
    Moving,
    Interactable,
    NUM_LAYERS
}; // namespace PhysicsLayer
