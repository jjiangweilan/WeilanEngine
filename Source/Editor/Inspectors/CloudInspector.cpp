#include "Core/Component/Cloud.hpp"
#include "Inspector.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
class CloudInspector : public Inspector<Cloud>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Cloud>::DrawInspector(editor);

        ImGui::Separator();
        ImGui::Text("Noise Generator");
        auto ptr = target->noiseGenerator.get();
        ImGui::Checkbox("Always Update", &alwayUpdate);
        if(Draw(target->noiseGenerator.get(), target->noiseGenerator->GetShaderProgram()) || alwayUpdate)
        {
            target->UpdateNoiseTexture();
        }
    }

    // taken from MaterialInspector
    bool Draw(Material* target, Gfx::ShaderProgram* shader)
    {
        bool changed = false;
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
                                changed = true;
                            }
                        }
                        else if (member.IsElement())
                        {
                            float val = target->GetFloat("", member.name);
                            if (ImGui::DragFloat(member.name.c_str(), &val))
                            {
                                target->SetFloat("", member.name, val);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        return changed;
    }


private:
    static const char _register;
    bool alwayUpdate = false;
};

const char CloudInspector::_register = InspectorRegistry::Register<CloudInspector, Cloud>();

} // namespace Editor
