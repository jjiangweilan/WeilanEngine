#pragma once

#include "Engine/Library/Math.hpp"


class EngineDebugVars
{
public:
    static bool& SceneBVH();
    static bool& ShadowFrustum();
};

class [[LuaClass]] Debug
{
public:
    [[LuaFn]] static void DrawLine(const float3& from, const float3& to, const float4& color);
    [[LuaFn]] static void DrawBox(const float3& center, const float3& halfExtents, const float4& color);
};
