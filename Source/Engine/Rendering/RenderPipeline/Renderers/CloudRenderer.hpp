#pragma once
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Material.hpp"

class Scene;
namespace Rendering
{
class CloudRenderer
{
public:
    void Init();

    void Render(Scene& scene, Gfx::CommandBuffer& cmd, Gfx::ShaderResource& perScene);
    void UpdateCloudShape();

    static void CreateCloudMaterials(std::unique_ptr<Material>& volumetricCloud, std::unique_ptr<Material>& noiseGenerator);

private:
    bool updateCloudShape = true;
    inline static const char* cloudNoiseGenerator = "Cloud/CloudNoiseGenerator";
    inline static const char* volumetricCloud = "Cloud/VolumetricCloud";

    struct
    {
        std::unique_ptr<Gfx::Image> tex;
        Gfx::ImageDescription desc;
    } cloudNoise;

    Submesh* cube;
};
} // namespace Rendering
