#include "Inspector.hpp"
#include "Engine/Runtime/Module/VolumetricCloud/Cloud.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

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
        if (ImGui::Button("Reset"))
        {
            target->ResetToDefaultValues();
        }
        ImGui::Checkbox("Always Update", &alwayUpdate);
        ImGui::SeparatorText("Parameter");
        Draw(target->volumetricCloud.get(), target->volumetricCloud->GetShaderProgram());

        ImGui::SeparatorText("Noise Generator");
        bool update = Draw(target->noiseGenerator.get(), target->noiseGenerator->GetShaderProgram());

        ImGui::SeparatorText("High Frequency Noise Generator");
        update |=
            Draw(target->highFrequencyNoiseGenerator.get(), target->highFrequencyNoiseGenerator->GetShaderProgram());

        update |= alwayUpdate;
        if (update)
        {
            target->UpdateNoiseTexture();
        }

        ImGui::Checkbox("Debug", &debugOn);

        if (debugOn)
        {
            ImGui::DragInt("Debug Image Index", &debugImageIndex, 1, 0, 1);

            auto debugImage = target->UpdateDebugImage(debugImageIndex);
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
        ImGui::PushID(target);
        bool changed = false;
        if (shader)
        {
            auto& pipelineInfo = shader->GetShaderInfo();
            auto set = pipelineInfo.GetDescriptorSet(Gfx::DescriptorSetSemantics::Material);
            if (set)
            {
                const auto binding = set->GetBinding(0);
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

                            if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Float)
                            {
                                if (ImGui::DragFloat(member.name.c_str(), &val))
                                {
                                    target->SetFloat("", member.name, val);
                                    changed = true;
                                }
                            }
                            else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Int)
                            {
                                int ival = val;
                                if (ImGui::DragInt(member.name.c_str(), &ival))
                                {
                                    target->SetFloat("", member.name, (float)ival);
                                    changed = true;
                                }
                            }
                            else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::UInt)
                            {
                                int ival = val;
                                if (ImGui::DragInt(member.name.c_str(), &ival, 1, 0, std::numeric_limits<int>::max()))
                                {
                                    target->SetFloat("", member.name, (float)ival);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        ImGui::PopID();

        return changed;
    }

private:
    using ViewSlice = int;
    std::unordered_map<ViewSlice, std::unique_ptr<Gfx::ImageView>> imageViews;
    static const char _register;
    bool alwayUpdate = false;
    int debugImageIndex = 0;
    float sizeScale = 1.0;
    float debugLayer = 0;
    float debugAxis = 0;
    float debugChannel = 0;
    bool debugOn = false;
};

const char CloudInspector::_register = InspectorRegistry::Register<CloudInspector, Cloud>();

} // namespace Editor
