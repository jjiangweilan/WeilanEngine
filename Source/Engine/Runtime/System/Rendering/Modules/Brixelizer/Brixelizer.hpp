#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"

class Brixelizer
{
public:
    void Execute(Gfx::CommandBuffer& cmd);

private:
    struct Instance
    {
        float4x4 transform;
        AABB aabb;
        uint32_t* triangleIndices;
        uint32_t triangleIndexCount;
        Submesh* mesh;
    };

    struct DispatchData
    {
        uint32_t triangleBufferIndex;
    };

    struct ExecuteContext
    {
        std::vector<uint32_t> cascadeOffsets;
        std::vector<Instance*> instancesInCascade;

        std::vector<Gfx::Buffer*> vertexBuffers;
        std::vector<DispatchData> dispatchData;
    };

    struct CascadeGridSize
    {
        float3 minWS;
        float3 maxWS;
    };

    const int CascadeCount = 1;
    std::unique_ptr<Gfx::Image> brixelAtlas;
    std::unique_ptr<Gfx::Image> cascadeVolumes;
    std::vector<Instance> instances;

    ObjPtr<Shader> voxelizer;

    bool InstanceInCascade(const Instance& instance, int cascadeIndex) { return true; }
};
