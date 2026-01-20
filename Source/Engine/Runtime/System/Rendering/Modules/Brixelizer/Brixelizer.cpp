#include "Brixelizer.hpp"

void Brixelizer::Execute(Gfx::CommandBuffer& cmd)
{
    ExecuteContext executeContext{};

    std::vector<uint32_t>& cascadeOffsets = executeContext.cascadeOffsets;
    std::vector<Instance*>& instancesInCascade = executeContext.instancesInCascade;
    std::vector<DispatchData>& dispatchData = executeContext.dispatchData;

    // collect instances into cascade to update
    cascadeOffsets.push_back(0); // cascade 0's offset is always 0
    for (int cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        for (auto& instance : instances)
        {
            if (InstanceInCascade(instance, cascadeIndex))
            {
                instancesInCascade.push_back(&instance);
            }

            break;
        }

        cascadeOffsets.push_back(instancesInCascade.size());
    }

    // build dispatchData
    for (int cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        uint32_t offset = cascadeOffsets[cascadeIndex];
        uint32_t endOffset = cascadeOffsets[cascadeIndex + 1];
        for (uint32_t instanceIndex = offset; instanceIndex < endOffset; ++instanceIndex)
        {
            Instance* instance = instancesInCascade[instanceIndex];
            for (uint32_t triIndex = 0; triIndex < instance->triangleIndexCount; ++triIndex)
            {
                DispatchData data{};
                data.triangleBufferIndex = instanceIndex;
                dispatchData.push_back(data);
            }
        }
    }

    // dispatch coarse culling

    auto shaderProgram = voxelizer->GetShaderProgram();
    cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());

    uint32_t triangleCount = 0;
    cmd.Dispatch((triangleCount + 63) / 64, 1, 1);
}
