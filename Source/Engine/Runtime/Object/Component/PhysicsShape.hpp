#pragma once
#include "Runtime/Object/Component/Component.hpp"
#include "Core/Object.hpp"

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

class PhysicsShape : public Component
{
    DECLARE_OBJECT();

public:
private:

    JPH::Ref<JPH::Shape> shapeRef;
};
