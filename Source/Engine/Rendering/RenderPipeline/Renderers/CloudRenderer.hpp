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


    Submesh* cube;
};
} // namespace Rendering
