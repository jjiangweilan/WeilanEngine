#pragma once

struct DebugOptions
{
    bool drawPhysicsColliders = false;
    bool drawPhysicsQueries = false;
    bool drawGameObjectDebugDraw = false;
};

DebugOptions& GetDebugOptions();
