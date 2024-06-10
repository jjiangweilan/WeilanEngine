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
        for (auto r : scene->GetRenderingScene().GetMeshRenderers())
        {
            if (r && r->IsEnabled())
            {
                drawList->Add(*r);
            }
        }

        Sort(*drawList, renderingData);

        output.drawList->SetValue(drawList.get());
    }

    bool FrustumCull(MeshRenderer& r)
    {
        return false;
    }

    void Sort(DrawList& drawList, RenderingData& renderingData)
    {
        auto pos = renderingData.mainCamera->GetGameObject()->GetPosition();
        std::sort(
            drawList.begin(),
            drawList.end(),
            [&pos](const SceneObjectDrawData& left, const SceneObjectDrawData& right) {
                return glm::distance2(pos, glm::vec3(left.pushConstant[3])) <
                       glm::distance2(pos, glm::vec3(right.pushConstant[3]));
            }
        );

        // partition transparent object to the end
        auto transparentIter = std::stable_partition(
            drawList.begin(),
            drawList.end(),
            [](const SceneObjectDrawData& val)
            { return val.shaderConfig->color.blends.empty() ? true : !val.shaderConfig->color.blends[0].blendEnable; }
        );
        drawList.transparentIndex = std::distance(drawList.begin(), transparentIter);

        // partition opaque and alpha tested objects
        auto alphaTestIter = std::stable_partition(
            drawList.begin(),
            transparentIter,
            [](const SceneObjectDrawData& val)
            {
                static std::string alphaTest = "_AlphaTest";

                auto& features = val.material->GetCachedShaderProgramFeatureUsed();
                return std::find(features.begin(), features.end(), alphaTest) == features.end();
            }
        );
        drawList.alphaTestIndex = std::distance(drawList.begin(), alphaTestIter);
        drawList.opaqueIndex = 0;
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
