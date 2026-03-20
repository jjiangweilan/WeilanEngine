#if CONFIG
name: GPUDrivenTest
#endif

import WeilanEngine;
import Library.PerScene;

[Global]
ParameterBlock<PerScene> perScene;

struct TestParams
{
    uint objectCount; // number of object IDs to validate
    uint maxTextureIndex; // maximum valid texture index
    uint padding0;
    uint padding1;

    [format("r32ui")]
    RWTexture2D<uint> resultBuffer; // output: pass/fail per object (1 = pass, 0 = fail)
};

[Material]
ParameterBlock<TestParams> params;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint idx = dispatchThreadID.x;
    if (idx >= params.objectCount)
        return;

    uint result = 1; // assume pass

    // Read object ID
    uint objectID = perScene.objectIDs[idx];

    // Load scene object data
    GPUSceneObjectData objData = perScene.LoadData<GPUSceneObjectData>(
        perScene.gpuDrivenConfig.sceneObjectDataBaseOffset, objectID);

    // Validate model matrix (check it's not all zeros)
    float det = objData.model[0][0] * objData.model[1][1] * objData.model[2][2];
    if (det == 0.0f && objData.model[0][0] == 0.0f && objData.model[1][1] == 0.0f && objData.model[2][2] == 0.0f)
    {
        result = 0; // model matrix is likely uninitialized
    }

    // Check mesh offsets are encoded in invTspModel row 3
    uint indexOffset = asuint(objData.invTspModel[0][3]);
    uint positionOffset = asuint(objData.invTspModel[1][3]);
    uint attributeOffset = asuint(objData.invTspModel[2][3]);

    // Offsets should be non-zero for valid meshes
    if (indexOffset == 0 && positionOffset == 0 && attributeOffset == 0)
    {
        result = 0; // mesh offsets not set
    }

    // Validate material index
    uint matIndex = objData.materialIndex;
    if (matIndex >= perScene.gpuDrivenConfig.materialCount)
    {
        result = 0; // material index out of range
    }
    else
    {
        // Load material data and validate texture indices
        GPUMaterialData matData = perScene.LoadData<GPUMaterialData>(
            perScene.gpuDrivenConfig.materialDataBaseOffset, matIndex);

        if (matData.baseColorTexIndex != 0xFFFFFFFF && matData.baseColorTexIndex >= params.maxTextureIndex)
            result = 0;
        if (matData.normalMapTexIndex != 0xFFFFFFFF && matData.normalMapTexIndex >= params.maxTextureIndex)
            result = 0;
        if (matData.metallicRoughnessTexIndex != 0xFFFFFFFF && matData.metallicRoughnessTexIndex >= params.maxTextureIndex)
            result = 0;
        if (matData.emissiveMapTexIndex != 0xFFFFFFFF && matData.emissiveMapTexIndex >= params.maxTextureIndex)
            result = 0;
    }

    // Write result: x = idx, y = 0 (using 1D layout in 2D texture)
    params.resultBuffer[uint2(idx, 0)] = result;
}
