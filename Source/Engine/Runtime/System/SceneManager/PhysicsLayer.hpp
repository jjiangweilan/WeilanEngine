#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace PhysicsObjectLayers
{
static constexpr JPH::ObjectLayer Static = 0;
static constexpr JPH::ObjectLayer Dynamic = 1;
static constexpr JPH::ObjectLayer Sprite = 2;
static constexpr JPH::ObjectLayer Sensor = 3;
static constexpr JPH::ObjectLayer NUM_LAYERS = 4;
} // namespace PhysicsObjectLayer
using PhysicsObjectLayer = JPH::ObjectLayer;