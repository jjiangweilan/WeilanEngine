#pragma once
#include "Core/Component/Component.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Libs/Math.hpp"

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

private:
    int level;

    std::unique_ptr<Gfx::Buffer> sh;


    void BakeToSh();
    float SHBasis(int l, int m, float3 dir);
};
