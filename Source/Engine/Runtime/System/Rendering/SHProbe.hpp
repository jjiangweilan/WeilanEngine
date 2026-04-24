#pragma once
#include "Engine/Runtime/Object/Component/Component.hpp"
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Library/Math.hpp"
#include <array>

struct SHProbeUpdateSettings
{
    bool skyboxOnly = true;
};
class SHProbe : public Object
{
    DECLARE_OBJECT();

public:
    struct UpdateProbeInfo
    {};

    SHProbe();
    SHProbe(GameObject* gameObject);

    void Init(int level);
    void UpdateProbe(Scene& scene, const SHProbeUpdateSettings& settings = {});

    // ** Object **/
    void Serialize(Serializer* s) const override {};
    void Deserialize(Serializer* s) override {};

    void DebugDrawProbe(const float3& position);

    const std::vector<float4>& GetSHCoefficients() const { return shData; }
    bool HasSH() const { return !shData.empty(); }

private:
    int level;

    std::unique_ptr<Gfx::Buffer> sh = nullptr;
    std::unique_ptr<Material> debugMaterial = nullptr;
    std::vector<float4> shData = {};

    void BakeToSh();
    std::array<float3, 9> BakeToSHCPU(Gfx::ImageDescription& cubeMapDesc, std::unique_ptr<Gfx::Buffer>& readbackBuffer);
    float SHBasis(int l, int m, float3 dir);
};
