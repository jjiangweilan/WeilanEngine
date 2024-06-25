#include "../NodeBlueprint.hpp"
#include "Rendering/Material.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Model.hpp"
#include "Rendering/Graphics.hpp"
#include "Rendering/Shader.hpp"
#include <random>


namespace Rendering::FrameGraph
{
class SkyboxPassNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(SkyboxPassNode)
    {
        SetCustomName("Skybox");
        shader = (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/ProcedualSkybox.shad");
        skyboxMat = (Material*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Materials/ProcedualSkybox.mat");

        AddConfig<ConfigurableType::Bool>("enable", true);

        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        output.color = AddOutputProperty("color", PropertyType::Attachment);

        Gfx::RG::SubpassAttachment c[] = {{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store,
        }};
        Gfx::RG::SubpassAttachment d = {
            1,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store,
        };
        skyboxPass = Gfx::RG::RenderPass(1, 2);
        skyboxPass.SetSubpass(0, c, d);
        skyboxPass.SetName("skybox pass");

        paramsBuffer = GetGfxDriver()->CreateBuffer(
            Gfx::Buffer::CreateInfo{Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform, sizeof(SkyboxPass)}
        );

        Model* model = static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Cube.glb"));
        cube = model->GetMeshes()[0].get();
    }

    void Compile() override
    {
        config.enable = GetConfigurablePtr<bool>("enable");
        Gfx::ClearValue v{0, 0, 0, 0};
        clears = {v};
    }

    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override
    {
        auto& cmd = *renderingData.cmd;
        if (*config.enable)
        {
            auto inputAttachment = input.color->GetValue<AttachmentProperty>();
            auto inputDepth = input.depth->GetValue<AttachmentProperty>();

            Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
            skyboxPass.SetAttachment(0, inputAttachment.id);
            skyboxPass.SetAttachment(1, inputDepth.id);
            cmd.BeginRenderPass(skyboxPass, clears);
            const Submesh& submesh = cube->GetSubmeshes()[0];
            auto vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
            for (auto& binding : submesh.GetBindings())
            {
                vertexBufferBinding.push_back({submesh.GetVertexBuffer(), binding.byteOffset});
            }
            cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
            cmd.BindVertexBuffer(vertexBufferBinding, 0);
            cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
            if (skyboxMat)
            {
                skyboxMat->UploadDataToGPU();
                cmd.BindResource(2, skyboxMat->GetShaderResource());
            }
            glm::vec4 position(renderingData.mainCamera->GetGameObject()->GetPosition(), 1.0);
            cmd.SetPushConstant(shader->GetDefaultShaderProgram(), &position);
            cmd.DrawIndexed(submesh.GetIndexCount(), 1, 0, 0, 0);

            cmd.EndRenderPass();
        }

        output.color->SetValue(input.color->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    Material* skyboxMat;

    std::unique_ptr<Gfx::Buffer> paramsBuffer;

    std::vector<Gfx::ClearValue> clears;
    Gfx::RG::RenderPass skyboxPass;
    Texture* noiseTex;
    Mesh* cube;

    struct SkyboxPass
    {
        bool operator==(const SkyboxPass& other) const = default;
        glm::vec4 sourceTexelSize;
    } params;

    struct
    {
        bool* enable;
    } config;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
    } input;

    struct
    {
        PropertyHandle color;
    } output;

}; // namespace Rendering::FrameGraph
DEFINE_FRAME_GRAPH_NODE(SkyboxPassNode, "F6D6CECC-F140-45B8-B8F7-501BB1F9B87E");

} // namespace Rendering::FrameGraph
