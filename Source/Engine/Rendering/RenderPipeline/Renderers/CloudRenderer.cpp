#include "CloudRenderer.hpp"
#include "Core/Component/Cloud.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ShaderLibrary.hpp"
namespace Rendering
{

void CloudRenderer::Init()
{
    cloudNoise.desc.width = 512;
    cloudNoise.desc.height = 512;
    cloudNoise.desc.depth = 64;
    cloudNoise.desc.format = Gfx::GfxFormat::R8G8B8A8_UNorm;
    cloudNoise.tex = GetGfxDriver()->CreateImage(cloudNoise.desc, Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture);

    cube = EngineInternalResources::GetCubeMesh();
}

void CloudRenderer::Render(Scene& scene, Gfx::CommandBuffer& cmd, Gfx::ShaderResource& perScene)
{
    auto& clouds = scene.GetRenderingScene().GetClouds();
    if (clouds.empty())
        return;

    auto& cloud = clouds[0];

    ObjPtr<Material> noiseGenerator = cloud->GetNoiseGenerator();
    ObjPtr<Material> volumetricCloud = cloud->GetVolumetricCloud();

    if (updateCloudShape)
    {
        cmd.BindResource(noiseGenerator->GetSet("prop"), noiseGenerator->GetShaderResource());
        int dispatchX = glm::ceil(cloudNoise.desc.width / 8.0f);
        int dispatchY = glm::ceil(cloudNoise.desc.width / 8.0f);
        int dispatchZ = glm::ceil(cloudNoise.desc.width / 8.0f);
        cmd.BindShaderProgram(noiseGenerator->GetShaderProgram(), noiseGenerator->GetShaderConfig());
        cmd.Dispatch(dispatchX, dispatchY, dispatchZ);
    }

    cmd.BindResource(volumetricCloud->GetSet("scene"), &perScene);

    const Gfx::VertexBufferBinding vertexBindings[] = {cube->GetGfxVertexBufferBindings()[0]};
    auto worldMatrix = cloud->GetGameObject()->GetWorldMatrix();
    cmd.SetPushConstant(volumetricCloud->GetShaderProgram(), &worldMatrix);
    cmd.BindIndexBuffer(cube->GetIndexBuffer(), 0, cube->GetIndexBufferType());
    cmd.BindVertexBuffer(vertexBindings, 0);
    cmd.BindResource(volumetricCloud->GetSet("perMaterial"), volumetricCloud->GetShaderResource());
    cmd.BindShaderProgram(volumetricCloud->GetShaderProgram(), volumetricCloud->GetShaderConfig());
    cmd.DrawIndexed(cube->GetIndexCount(), 1, 0, 0, 0);
}

void CloudRenderer::UpdateCloudShape()
{
    updateCloudShape = true;
}

void CloudRenderer::CreateCloudMaterials(
    std::unique_ptr<Material>& volumetricCloud, std::unique_ptr<Material>& noiseGenerator
)
{
    volumetricCloud = std::make_unique<Material>();
    noiseGenerator = std::make_unique<Material>();
    volumetricCloud->SetShader(ShaderLibrary::GetShader(CloudRenderer::volumetricCloud));
    noiseGenerator->SetShader(ShaderLibrary::GetShader(CloudRenderer::cloudNoiseGenerator));
}
} // namespace Rendering
