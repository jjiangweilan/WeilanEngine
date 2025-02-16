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
        ImGui::SeparatorText("Parameter");
        Draw(target->volumetricCloud.get(), target->volumetricCloud->GetShaderProgram());

        ImGui::SeparatorText("Noise Generator");
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
            if (ImGui::DragFloat("Debug Channel", &debugChannel))
            {
                target->debugImageMaterial->SetFloat("channel", debugChannel);
            }
            ImGui::DragFloat("Image Size Scale", &sizeScale);
            ImGui::Image(&debugImage->GetDefaultImageView(), (glm::vec2{256, 256} * sizeScale));
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
    float sizeScale = 1.0;
    float debugLayer = 0;
    float debugAxis = 0;
    float debugChannel = 0;
    bool debugOn = false;
};

const char CloudInspector::_register = InspectorRegistry::Register<CloudInspector, Cloud>();

} // namespace Editor
