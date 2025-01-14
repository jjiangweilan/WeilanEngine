#pragma once

#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Structs.hpp"
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
    void CreateCloudMaterials();
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

    void Setup();
    void UpdateNoiseTexture();
    Gfx::Image* UpdateDebugImage();
    void OnLoaded() override;
    void TransformChanged() override;

private:
    std::unique_ptr<Material> volumetricCloud;
    std::unique_ptr<Material> noiseGenerator;

    std::unique_ptr<Gfx::Image> debugImage;
    std::unique_ptr<Material> debugImageMaterial;
    Gfx::RG::RenderPass debugRenderPass = Gfx::RG::RenderPass::SingleColor("a debug pass");

    const float cloudSideResolution = 512;
    const float cloudHeightResolution = 512;
    bool isSetup = false;

    inline static const char* cloudNoiseGeneratorShader = "Cloud/CloudNoiseGenerator";
    inline static const char* volumetricCloudShader = "Cloud/VolumetricCloud";

    struct
    {
        std::unique_ptr<Gfx::Image> tex;
        Gfx::ImageDescription desc;
    } cloudNoise;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();
    void UpdateCloudGPUProperties();

    void OnEnable() override;
    void OnDisable() override;

    friend Editor::CloudInspector;
};
