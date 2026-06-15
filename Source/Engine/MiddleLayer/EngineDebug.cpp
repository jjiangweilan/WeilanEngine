#include "EngineDebug.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"

#define ENGINE_DEBUG_VAR(name)                                                                                         \
    bool& EngineDebugVars::name()                                                                                      \
    {                                                                                                                  \
        static bool val = false;                                                                                       \
        return val;                                                                                                    \
    }

ENGINE_DEBUG_VAR(SceneBVH);
ENGINE_DEBUG_VAR(ShadowFrustum);

void Debug::DrawLine(const float3& from, const float3& to, const float4& color)
{
    Graphics::DrawLine(from, to, color);
}

void Debug::DrawBox(const float3& center, const float3& halfExtents, const float4& color)
{
    const float3 min = center - halfExtents;
    const float3 max = center + halfExtents;

    const float3 corners[] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z},
    };

    Graphics::DrawLines(
        {
            {corners[0], corners[1], color},
            {corners[1], corners[2], color},
            {corners[2], corners[3], color},
            {corners[3], corners[0], color},
            {corners[4], corners[5], color},
            {corners[5], corners[6], color},
            {corners[6], corners[7], color},
            {corners[7], corners[4], color},
            {corners[0], corners[4], color},
            {corners[1], corners[5], color},
            {corners[2], corners[6], color},
            {corners[3], corners[7], color},
        }
    );
}
