#pragma once
#include <Jolt/Jolt.h>

class PhysicsScene
{
public:
    PhysicsScene();

    PhysicsScene(const PhysicsScene& other) = delete;
    PhysicsScene(PhysicsScene&& other) = delete;
    ~PhysicsScene() {}

private:
};
