#pragma once

#include "Core/Component/Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/Material.hpp"
#include <memory>
class RenderingScene;

namespace Editor
{
class CloudInspector;
}

class Cloud : public Component
{
    DECLARE_OBJECT();

public:
    Cloud();
    Cloud(GameObject* owner);
    ~Cloud() override {};

    void SetMesh(Mesh* mesh);
    Mesh* GetMesh();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() const override;

    void Setup();
    void UpdateNoiseTexture();
    Gfx::Image* UpdateDebugImage(int debugImageIndex);
    void OnLoaded() override;
    void TransformChanged() override;
    void Tick() override;
    void IdleTick() override;

    struct
    {
        std::unique_ptr<Gfx::Image> baseShapeNoise;
        std::unique_ptr<Gfx::Image> highFrequencyNoise;
    } cloudNoise;

    std::unique_ptr<Material> volumetricCloud = std::make_unique<Material>();
private:
    std::unique_ptr<Gfx::Image> debugImage;
    std::unique_ptr<Material> debugImageMaterial;
    std::unique_ptr<Material> noiseGenerator = std::make_unique<Material>();
    std::unique_ptr<Material> highFrequencyNoiseGenerator = std::make_unique<Material>();

    Gfx::RenderPass debugRenderPass = Gfx::RenderPass::SingleColor("a debug pass");
    inline static const char* cloudNoiseGeneratorShader =
        "Source/Engine/Modules/VolumetricCloud/Shaders/CloudNoiseGenerator";

    const float cloudSideResolution = 128;
    bool isSetup = false;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();

    void OnEnable() override;
    void OnDisable() override;

    friend Editor::CloudInspector;
};
