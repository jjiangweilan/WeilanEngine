#pragma once

struct DebugOptions
{
    bool drawPhysicsColliders = false;
    bool drawGameObjectDebugDraw = false;
};

DebugOptions& GetDebugOptions();
