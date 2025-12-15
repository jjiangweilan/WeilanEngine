#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/type_aligned.hpp>

namespace GPUParameter
{
using float2 = glm::aligned_vec2;
using float3 = glm::aligned_vec3;
using float4 = glm::aligned_vec4;
using int2 = glm::aligned_ivec2;
using int3 = glm::aligned_ivec3;
using int4 = glm::aligned_ivec4;
using uint2 = glm::aligned_uvec2;
using uint3 = glm::aligned_uvec3;
using uint4 = glm::aligned_uvec4;
using float4x4 = glm::aligned_fmat4x4;
using float3x3 = glm::aligned_fmat3x3;
using float2x2 = glm::aligned_fmat2x2;

#include "Engine/Shaders/DeferredPBRShadingInput.hlsl"
#include "Engine/Shaders/Library/PerScene.hlsl"
} // namespace GPUParameter
