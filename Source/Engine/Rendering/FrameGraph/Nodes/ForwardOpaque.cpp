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

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        glm::vec4* clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");
        auto opaqueColorHandle = RenderGraph::StrToHandle("opaque color");
        auto opaqueDepthHnadle = RenderGraph::StrToHandle("opaque depth");
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

        auto execFunc =
            [this,
             clearValuesVal,
             opaqueColorHandle,
             invalidSkybox](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
        {
            MakeFog(cmd);

            Gfx::Image* color = (Gfx::Image*)res.at(opaqueColorHandle)->GetResource();
            uint32_t width = color->GetDescription().width;
            uint32_t height = color->GetDescription().height;
            cmd.SetViewport(
                {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
            );
            Rect2D rect = {{0, 0}, {width, height}};
            clearValues[0] = *clearValuesVal;
            clearValues[0].color = {
                {(*clearValuesVal)[0], (*clearValuesVal)[1], (*clearValuesVal)[2], (*clearValuesVal)[3]}};
            clearValues[1].depthStencil = {1};

            cmd.SetScissor(0, 1, &rect);
            cmd.BeginRenderPass(pass, clearValues);

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

            cmd.EndRenderPass();
        };

        forwardNode =
            graph.AddNode(GetCustomName())
                .InputTexture("shadow", RenderGraph::StrToHandle("shadow map"), Gfx::PipelineStage::Fragment_Shader)
                .InputRT("opaque color", opaqueColorHandle)
                .InputRT("opaque depth", opaqueDepthHnadle)
                .AddColor(opaqueColorHandle)
                .AddDepthStencil(opaqueDepthHnadle)
                .SetExecFunc(execFunc)
                .Finish();

        return {
            Resource(
                ResourceTag::RenderGraphLink{},
                outputPropertyIDs["color"],
                forwardNode,
                RenderGraph::StrToHandle("opaque color")
            ),
            Resource(
                ResourceTag::RenderGraphLink{},
                outputPropertyIDs["depth"],
                forwardNode,
                RenderGraph::StrToHandle("opaque depth")
            )};
    }

    bool Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [colorNode, colorHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["color"]);
        auto [depthNode, depthHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["depth"]);
        auto [shadowNode, shadowHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["shadow map"]);
        drawList = resources.GetResource(ResourceTag::DrawList{}, inputPropertyIDs["draw list"]);

        if (graph.Connect(colorNode, colorHandle, forwardNode, RenderGraph::StrToHandle("opaque color")) &&
            graph.Connect(depthNode, depthHandle, forwardNode, RenderGraph::StrToHandle("opaque depth")) &&
            graph.Connect(shadowNode, shadowHandle, forwardNode, RenderGraph::StrToHandle("shadow map")))
        {
            return true;
        }
        else
            return false;

        return true;
    };

    void Execute(RenderingData& renderingData) override {}

private:
    RenderGraph::RenderNode* forwardNode;
    const DrawList* drawList;
    std::vector<Gfx::ClearValue> clearValues;

    Mesh* cube;
    Shader* skyboxShader;
    ComputeShader* fluidCompute;
    std::unique_ptr<Gfx::Image> fog;
    std::unique_ptr<Gfx::ShaderResource> skyboxResources;

    void DefineNode()
    {
        AddInputProperty("color", PropertyType::RenderGraphLink);
        AddInputProperty("depth", PropertyType::RenderGraphLink);
        AddInputProperty("shadow map", PropertyType::RenderGraphLink);
        AddInputProperty("draw list", PropertyType::DrawList);

        AddOutputProperty("color", PropertyType::RenderGraphLink);
        AddOutputProperty("depth", PropertyType::RenderGraphLink);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("skybox", nullptr);
        clearValues.resize(2);

        fluidCompute = static_cast<ComputeShader*>(
            AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/Fluid/Fog.comp")
        );
        fog = GetGfxDriver()->CreateImage({64, 64, 64, Gfx::ImageFormat::R32_Float}, Gfx::ImageUsage::Storage);
    }

    void MakeFog(Gfx::CommandBuffer& cmd)
    {
        cmd.SetTexture("imgOutput", *fog);
        cmd.BindShaderProgram(fluidCompute->GetDefaultShaderProgram(), fluidCompute->GetDefaultShaderConfig());
        cmd.Dispatch(16, 16, 16);
    }

    static char _reg;
}; // namespace FrameGraph

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");

DEFINE_OBJECT(ForwardOpaqueNode, "E6188926-D83E-4B17-9C7C-060A5862BDCA");
} // namespace FrameGraph
