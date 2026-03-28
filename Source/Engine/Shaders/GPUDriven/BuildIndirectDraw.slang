import Library.PerScene;

struct DrawIndexedIndirectCommand {
    uint32_t    indexCount;
    uint32_t    instanceCount;
    uint32_t    firstIndex;
    int32_t     vertexOffset;
    uint32_t    firstInstance;
}

ParameterBlock<PerScene> perScene;

struct GpuObjectDrawInfo
{
    uint pObject;
    uint drawCount; // draw count after shader filtered in CPU
};

struct PassData
{
    StructuredBuffer<GpuObjectDrawInfo> objects;
    RWStructuredBuffer<DrawIndexedIndirectCommand> outDrawCommands;
    RWStructuredBuffer<uint> batchGpuObjectOffsets;
    RWStructuredBuffer<uint> writeIndex;
}

ParameterBlock<PassData> passData;

[vk_push_constant]
uint shaderID;

[shader("compute")]
[numthreads(64, 1, 1)]
void Main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    GpuObjectDrawInfo drawInfo = passData.objects[dispatchThreadID.x];
    GpuObject gpuObject = perScene.LoadData<GpuObject>(drawInfo.pObject);

    uint writeOffset = 0;
    

    InterlockedAdd(passData.writeIndex[0], drawInfo.drawCount, writeOffset);

    for(uint i = 0, localWriteIndex = 0; i < gpuObject.renderDataCount; i++)
    {
        GpuRenderData renderData = perScene.LoadData<GpuRenderData>(gpuObject.pRenderDataOffset, i);

        if (renderData.shaderID == shaderID)
        {
            GpuGeometry geometry = perScene.LoadData<GpuGeometry>(renderData.geometryOffset);

            passData.outDrawCommands[writeOffset + localWriteIndex] = DrawIndexedIndirectCommand(
                    geometry.indexCount, 1, geometry.indexOffset / 4, 0, renderData.geometryOffset);
            passData.batchGpuObjectOffsets[writeOffset + localWriteIndex] = renderData.geometryOffset;

            localWriteIndex += 1;
        }
    }

}
