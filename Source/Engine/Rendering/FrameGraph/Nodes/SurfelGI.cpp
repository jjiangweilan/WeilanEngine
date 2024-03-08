#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "Rendering/SurfelGI/GIScene.hpp"

namespace FrameGraph
{
class SurfelGINode : public Node
{
    DECLARE_OBJECT();

public:
    SurfelGINode()
    {
        DefineNode();
    };
    SurfelGINode(FGID id) : Node("Surfel GI", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        bool* debug = GetConfigurablePtr<bool>("Debug");
        Shader* shader = GetConfigurableVal<Shader*>("Surfel Cube Shader");
        SurfelGI::GIScene* giScene = GetConfigurableVal<SurfelGI::GIScene*>("GI Scene");
        Mesh* debugArrow = GetConfigurableVal<Mesh*>("Surfel Debug Arrow");

        if (giScene && shader)
        {
            program = shader->GetDefaultShaderProgram();
            passResource = GetGfxDriver()->CreateShaderResource();
            size_t giSceneSize = giScene->surfels.size() * sizeof(SurfelGI::Surfel);
            Gfx::Buffer::CreateInfo createInfo{
                Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Transfer_Src,
                giSceneSize,
                false,
                "GI Scene"};
            buf = GetGfxDriver()->CreateBuffer(createInfo);
            createInfo.visibleInCPU = true;
            auto staging = GetGfxDriver()->CreateBuffer(createInfo);
            memcpy(staging->GetCPUVisibleAddress(), giScene->surfels.data(), giSceneSize);

            passResource->SetBuffer("GIScene", buf.get());
            GetGfxDriver()->ExecuteImmediately(
                [&](Gfx::CommandBuffer& cmd)
                {
                    Gfx::BufferCopyRegion region[1] = {Gfx::BufferCopyRegion{0, 0, buf->GetSize()}};
                    cmd.CopyBuffer(staging, buf, region);
                }
            );
        }

        auto exec = [=](Gfx::CommandBuffer& cmd, auto& pass, auto& res)
        {
            if (debug && *debug && giScene && debugArrow)
            {
                auto& submesh = debugArrow->GetSubmeshes()[0];
                cmd.BeginRenderPass(pass, {});
                cmd.BindSubmesh(submesh);
                cmd.BindShaderProgram(program, shader->GetDefaultShaderConfig());
                cmd.BindResource(1, passResource.get());
                size_t surfelSize = giScene->surfels.size();
                cmd.DrawIndexed(submesh.GetIndexCount(), surfelSize, 0, 0, 0);
                cmd.EndRenderPass();
            }
        };

        node = graph.AddNode(GetCustomName())
                   .InputRT("opaque color", 0)
                   .InputRT("opaque depth", 1)
                   .AddColor(0, false, Gfx::AttachmentLoadOperation::Load)
                   .AddDepthStencil(1, Gfx::AttachmentLoadOperation::Load)
                   .SetExecFunc(exec)
                   .Finish();

        return {
            Resource(ResourceTag::RenderGraphLink{}, outputPropertyIDs["color"], node, 0),
            Resource(ResourceTag::RenderGraphLink{}, outputPropertyIDs["depth"], node, 1)};
    }

    bool Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [colorNode, colorHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["color"]);
        auto [depthNode, depthHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["depth"]);

        if (graph.Connect(colorNode, colorHandle, node, 0) && graph.Connect(depthNode, depthHandle, node, 1))
        {
            return true;
        }
        else
            return false;
        return true;
    };

    // void Execute(RenderingData& renderingData) override {}

private:
    RenderGraph::RenderNode* node;

    std::unique_ptr<Gfx::Buffer> buf;
    std::unique_ptr<Gfx::ShaderResource> passResource;
    Gfx::ShaderProgram* program;
    void DefineNode()
    {
        AddInputProperty("color", PropertyType::RenderGraphLink);
        AddInputProperty("depth", PropertyType::RenderGraphLink);

        AddOutputProperty("color", PropertyType::RenderGraphLink);
        AddOutputProperty("depth", PropertyType::RenderGraphLink);
        AddConfig<ConfigurableType::ObjectPtr>("GI Scene", nullptr);
        AddConfig<ConfigurableType::Bool>("Debug", false);
        AddConfig<ConfigurableType::ObjectPtr>("Surfel Cube Shader", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("Surfel Debug Arrow", nullptr);
    }
    static char _reg;
}; // namespace FrameGraph

char SurfelGINode::_reg = NodeBlueprintRegisteration::Register<SurfelGINode>("Surfel GI");

DEFINE_OBJECT(SurfelGINode, "1810BA8A-B0B3-47CE-BF52-F1D881E98B01");
} // namespace FrameGraph
