#pragma once
#include "../NodeBlueprint.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace Engine::FrameGraph
{
class SceneSortNode : public Node
{
    DECLARE_OBJECT();

public:
    SceneSortNode()
    {
        DefineNode();
    }

    SceneSortNode(FGID id) : Node("Scene Sort", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        return {
            Resource(ResourceTag::DrawList{}, propertyIDs["draw list"], &drawList),
        };
    }

    void Execute(GraphResource& graphResource) override
    {
        Scene* scene = graphResource.mainCamera->GetGameObject()->GetGameScene();
        auto objs = scene->GetAllGameObjects();

        for (GameObject* go : objs)
        {
            MeshRenderer* meshRenderer = go->GetComponent<MeshRenderer>();
            if (meshRenderer)
            {
                auto mesh = meshRenderer->GetMesh();
                if (mesh == nullptr)
                    return;

                auto& submeshes = mesh->GetSubmeshes();
                auto& materials = meshRenderer->GetMaterials();

                for (int i = 0; i < submeshes.size() || i < materials.size(); ++i)
                {
                    auto material = i < materials.size() ? materials[i] : nullptr;
                    auto submesh = i < submeshes.size() ? &submeshes[i] : nullptr;
                    auto shader = material ? material->GetShader() : nullptr;

                    if (submesh != nullptr && material != nullptr && shader != nullptr)
                    {
                        // material->SetMatrix("Transform", "model",
                        // meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                        uint32_t indexCount = submesh->GetIndexCount();
                        SceneObjectDrawData drawData;

                        drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                        for (auto& binding : submesh->GetBindings())
                        {
                            drawData.vertexBufferBinding.push_back({submesh->GetVertexBuffer(), binding.byteOffset});
                        }
                        drawData.indexBuffer = submesh->GetIndexBuffer();
                        drawData.indexBufferType = submesh->GetIndexBufferType();

                        drawData.shaderResource = material->GetShaderResource().Get();
                        drawData.shader = shader->GetShaderProgram().Get();
                        drawData.shaderConfig = &material->GetShaderConfig();
                        auto modelMatrix = meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix();
                        drawData.pushConstant = modelMatrix;
                        drawData.indexCount = indexCount;
                        drawList.push_back(drawData);
                    }
                }
            }
        }
    }

private:
    DrawList drawList;
    void DefineNode()
    {
        AddOutputProperty("draw list", PropertyType::DrawList);
    }
    static char _reg;
};

char SceneSortNode::_reg = NodeBlueprintRegisteration::Register<SceneSortNode>("Scene Sort");
DEFINE_OBJECT(SceneSortNode, "24B2D306-5827-479E-A3C8-6B5D0742BCD8");
} // namespace Engine::FrameGraph
