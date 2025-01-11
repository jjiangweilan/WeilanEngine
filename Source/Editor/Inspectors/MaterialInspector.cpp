#include "../EditorState.hpp"
#include "EditorGUI.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Inspector.hpp"
#include "Rendering/EnumStringMapping.hpp"
#include "Rendering/Material.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Editor
{
class MaterialInspector : public Inspector<Material>
{
public:
    void OnEnable(Object& obj) override { Inspector<Material>::OnEnable(obj); }

    void DrawInspector(GameEditor& editor) override
    {
        std::string name = target->GetName();
        if (GUI::InputText("Name", name))
            target->SetName(name);

        auto shader = target->GetShaderProgram();
        Draw(shader);

        ImGui::Separator();

        GUI::AutoObjectInspector(target);
    }

    void Draw(Gfx::ShaderProgram* shader)
    {
        if (shader)
        {
            auto& pipelineInfo = shader->GetShaderInfo();
            auto set = pipelineInfo.GetDescriptorSet(Material::PerMaterial);
            if (set)
            {
                const auto binding = set->GetBinding(Material::PerMaterial);
                if (binding)
                {
                    for (auto member : binding->bufferMembers)
                    {
                        if (member.IsVector())
                        {
                            glm::float4 val = target->GetVector("", member.name);
                            if (ImGui::DragFloat4(member.name.c_str(), &val[0]))
                            {
                                target->SetVector("", member.name, val);
                            }
                        }
                        else if (member.IsElement())
                        {
                            float val = target->GetFloat("", member.name);
                            if (ImGui::DragFloat(member.name.c_str(), &val))
                            {
                                target->SetFloat("", member.name, val);
                            }
                        }
                    }
                }
            }
        }
    }

private:
    static const char _register;
    char featureToEnable[256];

    glm::vec2 ResizeKeepRatio(float width, float height, float contentWidth, float contentHeight)
    {
        float imageWidth = width;
        float imageHeight = height;

        // shrink width
        if (imageWidth > contentWidth)
        {
            float ratio = contentWidth / (float)imageWidth;
            imageWidth = contentWidth;
            imageHeight *= ratio;
        }

        if (imageHeight > contentHeight)
        {
            float ratio = contentHeight / (float)imageHeight;
            imageHeight = contentHeight;
            imageWidth *= ratio;
        }

        return {imageWidth, imageHeight};
    }
};

const char MaterialInspector::_register = InspectorRegistry::Register<MaterialInspector, Material>();

} // namespace Editor
