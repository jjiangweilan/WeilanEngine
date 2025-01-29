#include "Inspector.hpp"
#include "Modules/VolumetricCloud/Cloud.hpp"
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
        if (Draw(target->noiseGenerator.get(), target->noiseGenerator->GetShaderProgram()) || alwayUpdate)
        {
            target->UpdateNoiseTexture();
        }

        ImGui::Checkbox("Debug", &debugOn);

        if (debugOn)
        {
            auto debugImage = target->UpdateDebugImage();
            if (ImGui::DragFloat("Debug Layer", &debugLayer))
            {
                target->debugImageMaterial->SetFloat("layer", debugLayer);
            }
            if (ImGui::DragFloat("Debug Axis", &debugAxis))
            {
                target->debugImageMaterial->SetFloat("axis", debugAxis);
            }
            ImGui::Image(&debugImage->GetDefaultImageView(), {256, 256});

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

                            if (member.rowCount == 4)
                            {
                                if (ImGui::DragFloat4(member.name.c_str(), &val[0]))
                                {
                                    target->SetVector("", member.name, val);
                                    changed = true;
                                }
                            }
                            if (member.rowCount == 3)
                            {
                                if (ImGui::DragFloat3(member.name.c_str(), &val[0]))
                                {
                                    target->SetVector("", member.name, val);
                                    changed = true;
                                }
                            }
                            else if (member.rowCount == 2)
                            {
                                if (ImGui::DragFloat2(member.name.c_str(), &val[0]))
                                {
                                    target->SetVector("", member.name, val);
                                    changed = true;
                                }
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
    using ViewSlice = int;
    ViewSlice viewSlice;
    std::unordered_map<ViewSlice, std::unique_ptr<Gfx::ImageView>> imageViews;
    static const char _register;
    bool alwayUpdate = false;
    float debugLayer = 0;
    float debugAxis = 0;
    bool debugOn = false;
};

const char CloudInspector::_register = InspectorRegistry::Register<CloudInspector, Cloud>();

} // namespace Editor
