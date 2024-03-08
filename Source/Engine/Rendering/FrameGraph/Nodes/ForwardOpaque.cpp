#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include <spdlog/spdlog.h>

namespace FrameGraph
{
class ForwardOpaqueNode : public Node
{
    DECLARE_OBJECT();

public:
    ForwardOpaqueNode()
    {
        DefineNode();
    };
    ForwardOpaqueNode(FGID id) : Node("Forward Opaque", id)
    {
        DefineNode();
    }

    void Compile() override
    {
        clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");
        volumetricFogShader = static_cast<Shader*>(
            AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/VolumetricFog.shad")
        );

        auto skybox = GetConfigurableVal<Texture*>("skybox");

        auto cubeModel = static_cast<Model*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/Cube.glb"));
        cube = cubeModel ? cubeModel->GetMeshes()[0].get() : nullptr;
        skyboxShader =
            static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/Skybox.shad"));

        bool validCube = true;
        if (!cube || cube->GetSubmeshes().empty() || cube->GetSubmeshes()[0].GetBindings().empty())
            validCube = false;

        bool invalidSkybox = cube == nullptr || skyboxShader == nullptr || !validCube || skybox == nullptr ||
                             !skybox->GetDescription().img.isCubemap;
        if (invalidSkybox)
        {
            SPDLOG_WARN("failed to load skybox data, skybox won't render");
        }
        else
        {
            skyboxResources = GetGfxDriver()->CreateShaderResource();
            skyboxResources->SetImage("envMap", skybox->GetGfxImage());
        }

        drawList = input.drawList->GetValue<DrawList*>();
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        MakeCloudNoise(cmd);

        auto desc = input.color->GetValue<AttachmentProperty>().desc;

        uint32_t width = desc.GetWidth();
        uint32_t height = desc.GetHeight();
        cmd.SetViewport({.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1});
        Rect2D rect = {{0, 0}, {width, height}};
        clearValues[0] = *clearValuesVal;
        clearValues[0].color = {
            {(*clearValuesVal)[0], (*clearValuesVal)[1], (*clearValuesVal)[2], (*clearValuesVal)[3]}};
        clearValues[1].depthStencil = {1};

        cmd.SetScissor(0, 1, &rect);
        mainPass.SetAttachment(0, input.color->GetValue<AttachmentProperty>().id);
        mainPass.SetAttachment(1, input.depth->GetValue<AttachmentProperty>().id);
        cmd.BeginRenderPass(mainPass, clearValues);

        if (!invalidSkybox)
        {
            cmd.BindShaderProgram(skyboxShader->GetDefaultShaderProgram(), skyboxShader->GetDefaultShaderConfig());
            auto& cubeSubmesh = cube->GetSubmeshes()[0];
            Gfx::VertexBufferBinding bindins[] = {
                {cubeSubmesh.GetVertexBuffer(), cubeSubmesh.GetBindings()[0].byteOffset}};
            cmd.BindVertexBuffer(bindins, 0);
            cmd.BindIndexBuffer(cubeSubmesh.GetIndexBuffer(), 0, cubeSubmesh.GetIndexBufferType());
            cmd.BindResource(2, skyboxResources.get());
            cmd.DrawIndexed(cubeSubmesh.GetIndexCount(), 1, 0, 0, 0);
        }

        // draw scene objects
        for (auto& draw : *drawList)
        {
            cmd.BindShaderProgram(draw.shader, *draw.shaderConfig);
            cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
            cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
            cmd.BindResource(2, draw.shaderResource);
            cmd.SetPushConstant(draw.shader, (void*)&draw.pushConstant);
            cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
        }

        cmd.EndRenderPass();

        output.color->SetValue(input.color->GetValue<AttachmentProperty>());
        output.depth->SetValue(input.depth->GetValue<AttachmentProperty>());
    }

private:
    Gfx::RG::RenderPass fogGenerationPass = Gfx::RG::RenderPass::SingleColor();
    Shader* volumetricFogShader;
    Gfx::RG::RenderPass mainPass = Gfx::RG::RenderPass::Default();

    RenderGraph::RenderNode* forwardNode;
    const DrawList* drawList;
    std::vector<Gfx::ClearValue> clearValues;

    glm::vec4* clearValuesVal;
    Mesh* cube;
    Shader* skyboxShader;
    ComputeShader* fluidCompute;
    std::unique_ptr<Gfx::Image> cloudRT;
    std::unique_ptr<Gfx::Image> cloud2RT;
    std::unique_ptr<Gfx::ShaderResource> skyboxResources;
    bool invalidSkybox = true;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
        PropertyHandle shadowMap;
        PropertyHandle drawList;
    } input;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
    } output;

    uint32_t cloudTexSize = 128;
    uint32_t cloudTex2Size = 64;
    void DefineNode()
    {
        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.shadowMap = AddInputProperty("shadow map", PropertyType::Attachment);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer);

        output.color = AddOutputProperty("color", PropertyType::Attachment);
        output.depth = AddOutputProperty("depth", PropertyType::Attachment);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("skybox", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("cloud noise material", nullptr);
        clearValues.resize(2);

        fluidCompute = static_cast<ComputeShader*>(
            AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/Fluid/Fog.comp")
        );
        cloudRT = GetGfxDriver()->CreateImage(
            {cloudTexSize, cloudTexSize, cloudTexSize, Gfx::ImageFormat::R8G8B8A8_UNorm},
            Gfx::ImageUsage::Storage
        );

        cloud2RT = GetGfxDriver()->CreateImage(
            {cloudTex2Size, cloudTex2Size, cloudTex2Size, Gfx::ImageFormat::R8G8B8A8_UNorm},
            Gfx::ImageUsage::Storage
        );
    }

    void MakeCloudNoise(Gfx::CommandBuffer& cmd)
    {
        Material* cloudNoiseMaterial = GetConfigurableVal<Material*>("cloud noise material");

        if (cloudNoiseMaterial)
        {
            cloudNoiseMaterial->UploadDataToGPU();
            cmd.SetTexture("imgOutput", *cloudRT);
            cmd.SetTexture("imgOutput2", *cloud2RT);
            cmd.BindShaderProgram(fluidCompute->GetDefaultShaderProgram(), fluidCompute->GetDefaultShaderConfig());
            cmd.BindResource(2, cloudNoiseMaterial->GetShaderResource());
            cmd.Dispatch(cloudTexSize / 4, cloudTexSize / 4, cloudTexSize / 4);
            cmd.SetTexture("cloudDensity", *cloudRT);
        }
    }

    static char _reg;
}; // namespace FrameGraph

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");

DEFINE_OBJECT(ForwardOpaqueNode, "E6188926-D83E-4B17-9C7C-060A5862BDCA");
} // namespace FrameGraph
