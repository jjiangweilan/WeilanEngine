#pragma once
#include "../NodeBlueprint.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace Rendering::FrameGraph
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

    void Compile() override
    {
        output.drawList->SetValue(drawList.get());
    }

    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override
    {
        drawList->clear();

        Scene* scene = renderingData.mainCamera->GetGameObject()->GetScene();

        drawList->Add(scene->GetRenderingScene().GetMeshRenderers());
        drawList->Sort(renderingData.mainCamera->GetGameObject()->GetPosition());

        output.drawList->SetValue(drawList.get());
    }

    bool FrustumCull(MeshRenderer& r)
    {
        return false;
    }

private:
    std::unique_ptr<DrawList> drawList;
    DrawList* append;

    struct
    {
        PropertyHandle drawList;
    } output;
    void DefineNode()
    {
        output.drawList = AddOutputProperty("draw list", PropertyType::DrawListPointer);
        output.drawList->SetValue(drawList.get());
        drawList = std::make_unique<DrawList>();
    }
    static char _reg;
};

char SceneSortNode::_reg = NodeBlueprintRegisteration::Register<SceneSortNode>("Scene Sort");
DEFINE_OBJECT(SceneSortNode, "24B2D306-5827-479E-A3C8-6B5D0742BCD8");
} // namespace Rendering::FrameGraph
