#pragma once
#include "Core/Component/Component.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Libs/Math.hpp"
#include <array>

struct SHProbeUpdateSettings
{
    bool skyboxOnly = true;
};
class SHProbe : public Component
{
    DECLARE_OBJECT();

public:
    struct UpdateProbeInfo
    {};

    SHProbe();
    SHProbe(GameObject* gameObject);

    void Init(int level);
    void UpdateProbe(const SHProbeUpdateSettings& settings = {});

    // ** Componet **//
    const std::string& GetName() override;

    std::unique_ptr<Gfx::Image> cubeMap;

    // ** Object **/
    void Serialize(Serializer* s) const override {};
    void Deserialize(Serializer* s) override {};

    void DrawDebugProbe();

private:
    int level;

    std::unique_ptr<Gfx::Buffer> sh = nullptr;
    std::vector<float4> shData = {};
    std::unique_ptr<Material> debugMaterial = nullptr;

    void BakeToSh();
    std::array<float3, 9> BakeToSHCPU(Gfx::ImageDescription& cubeMapDesc, std::unique_ptr<Gfx::Buffer>& readbackBuffer);
    float SHBasis(int l, int m, float3 dir);
};
