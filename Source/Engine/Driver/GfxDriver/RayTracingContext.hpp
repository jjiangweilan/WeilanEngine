#pragma once
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Library/Math.hpp"

#include <span>

namespace Gfx
{
using RayTracingMeshHandle = int;
using RayTracingSceneHandle = int;
using RayTracingInstanceHandle = int;

struct BlasGeometry
{
    uint64_t vertexBufferShaderDeviceAddress;
    Gfx::GfxFormat vertexFormat;
    uint32_t vertexStride;
    uint32_t maxVertex;
    uint64_t indexBufferShaderDeviceAddress;
    Gfx::IndexBufferType indexBufferType;
    uint32_t triangleCount;
};

class RayTracingContext
{
public:
    virtual RayTracingSceneHandle CreateScene(uint32_t maxInstanceCount) = 0;
    virtual RayTracingMeshHandle CreateBLAS(std::span<BlasGeometry> geometries) = 0;
    virtual RayTracingInstanceHandle CreateInstance(RayTracingMeshHandle mesh, glm::float4x3 initialTransform, uint32_t customIndex) = 0;
    virtual void BuildScene(const RayTracingSceneHandle& sceneHandle, std::span<RayTracingInstanceHandle> instances) = 0;

    virtual ~RayTracingContext() = default;
};
} // namespace Gfx
