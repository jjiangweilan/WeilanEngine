#include "Editor/EditorState.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Editor/EditorGUI.hpp"
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Driver/GfxDriver/ShaderProgram.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Editor/Inspectors/InspectorRegistry.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "MaterialAttributeParser.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <map>
#include <limits>
#include <algorithm>
#include <cctype>
namespace Editor
{
class MaterialInspector : public Inspector<Material>
{
public:
    void OnEnable(Object& obj) override { Inspector<Material>::OnEnable(obj); }

    void DrawInspector(GameEditor& editor) override
    {
        std::string name = target->GetName();
        if (EditorGUI::InputText("Name", name))
            target->SetName(name);

        auto shader = target->GetShaderProgram();
        const char* shaderName = shader ? shader->GetName().c_str() : "";
        if (const char* picked = EditorGUI::ShaderPicker(shaderName, shaderSearch))
        {
            target->SetShader(picked);
        }

        if (shaderName[0] != '\0')
        {
            const ShaderFeatures& features = ShaderLibrary::QueryShaderFeatures(shaderName);
            for (const auto& feature : features.toggleFeatures)
            {
                bool enabled = target->IsFeatureEnabled(feature.name);
                if (ImGui::Checkbox(feature.name.c_str(), &enabled))
                {
                    if (enabled)
                        target->EnableFeature(feature.name);
                    else
                        target->DisableFeature(feature.name);
                }
            }
        }

        {
            auto cfgPtr = target->GetShaderConfig();
            int cullMode = (int)cfgPtr->cullMode;
            static const char* CullModeNames[] = {"None", "Front", "Back", "Both"};
            if (ImGui::Combo("Cull Mode", &cullMode, CullModeNames, (int)Gfx::CullMode::MAX_COUNT - 1))
            {
                Gfx::PipelineConfig::PipelineConfig_t newCfg = *target->GetShaderConfig();
                newCfg.cullMode = (Gfx::CullMode)cullMode;
                target->SetShaderConfig(newCfg);
            }
        }

        DrawMaterialProperties(shader, editor);

        if (ImGui::TreeNode("Auto Inspector"))
        {
            EditorGUI::AutoObjectInspector(target, true);
            ImGui::TreePop();
        }
    }

private:
    std::string shaderSearch;

    struct PropertyItem
    {
        std::string name;
        MaterialAttributeInfo info;
        const Gfx::ShaderPipelineInfo::BufferMember* member = nullptr;
        const Gfx::ShaderPipelineInfo::Binding* binding = nullptr;
    };

    void DrawMaterialProperties(Gfx::ShaderProgram* shader, GameEditor& editor)
    {
        if (shader)
        {
            const ShaderFeatures& features = ShaderLibrary::QueryShaderFeatures(shader->GetName().c_str());
            auto& pipelineInfo = shader->GetShaderInfo();
            auto set = pipelineInfo.GetDescriptorSet(Gfx::DescriptorSetSemantics::Material);

            std::map<std::string, std::vector<PropertyItem>> groups;
            std::vector<PropertyItem> ungrouped;

            if (!pipelineInfo.uiPropertySchema.empty())
            {
                for (const auto& schemaMember : pipelineInfo.uiPropertySchema)
                {
                    PropertyItem item;
                    item.name = schemaMember.name;
                    item.info = ParseAttributes(schemaMember.attributes);
                    item.member = &schemaMember; // Use schema member as the source of reflection data

                    // Still try to find if it corresponds to a real texture binding if it's not a buffer member
                    if (set)
                    {
                        for (const auto& binding : set->bindings)
                        {
                            if (binding.descriptorType == Gfx::DescriptorType::CombinedImageSampler ||
                                binding.descriptorType == Gfx::DescriptorType::SampledImage)
                            {
                                if (binding.name == schemaMember.name)
                                {
                                    item.binding = &binding;
                                    item.member = nullptr; // It's a texture
                                    break;
                                }
                            }
                        }
                    }

                    if (item.info.group)
                        groups[item.info.group->name].push_back(item);
                    else
                        ungrouped.push_back(item);
                }
            }
            else if (set)
            {
                for (const auto& binding : set->bindings)
                {
                    if (binding.descriptorType == Gfx::DescriptorType::UniformBuffer)
                    {
                        for (const auto& member : binding.bufferMembers)
                        {
                            PropertyItem item;
                            item.name = member.name;
                            item.info = ParseAttributes(member.attributes);
                            item.member = &member;
                            if (item.info.group)
                                groups[item.info.group->name].push_back(item);
                            else
                                ungrouped.push_back(item);
                        }
                    }
                    else if (binding.descriptorType == Gfx::DescriptorType::CombinedImageSampler ||
                             binding.descriptorType == Gfx::DescriptorType::SampledImage)
                    {
                        PropertyItem item;
                        item.name = binding.name;
                        item.binding = &binding;
                        if (item.info.group)
                            groups[item.info.group->name].push_back(item);
                        else
                            ungrouped.push_back(item);
                    }
                }
            }

            for (const auto& item : ungrouped)
            {
                DrawProperty(item, features, editor);
            }

            for (auto& [groupName, items] : groups)
            {
                if (ImGui::CollapsingHeader(groupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& item : items)
                    {
                        DrawProperty(item, features, editor);
                    }
                }
            }
        }
    }

    void DrawProperty(const PropertyItem& item, const ShaderFeatures& features, GameEditor& editor)
    {
        if (item.info.isTexture || (item.binding && !item.member))
        {
            DrawTextureProperty(item, features, editor);
        }
        else if (item.member)
        {
            DrawBufferMember(item);
        }

        if (item.info.tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", item.info.tooltip->text.c_str());
        }
    }

    void DrawBufferMember(const PropertyItem& item)
    {
        const auto& member = *item.member;
        const auto& info = item.info;

        if (member.IsVector())
        {
            glm::vec4 val = target->GetVector("", member.name);
            bool changed = false;

            if (info.isColor)
            {
                if (member.rowCount == 3)
                    changed = ImGui::ColorEdit3(member.name.c_str(), &val[0]);
                else if (member.rowCount == 4)
                    changed = ImGui::ColorEdit4(member.name.c_str(), &val[0]);
            }
            else
            {
                if (member.rowCount == 2)
                    changed = ImGui::DragFloat2(member.name.c_str(), &val[0]);
                else if (member.rowCount == 3)
                    changed = ImGui::DragFloat3(member.name.c_str(), &val[0]);
                else if (member.rowCount == 4)
                    changed = ImGui::DragFloat4(member.name.c_str(), &val[0]);
            }

            if (changed)
                target->SetVector("", member.name, val);
        }
        else if (member.IsElement())
        {
            float val = target->GetFloat("", member.name);
            bool changed = false;

            if (info.range)
            {
                changed = ImGui::SliderFloat(member.name.c_str(), &val, info.range->min, info.range->max);
            }
            else
            {
                if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Float)
                    changed = ImGui::DragFloat(member.name.c_str(), &val);
                else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Int)
                {
                    int ival = (int)val;
                    if (ImGui::DragInt(member.name.c_str(), &ival))
                    {
                        val = (float)ival;
                        changed = true;
                    }
                }
                else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::UInt)
                {
                    int ival = (int)val;
                    if (ImGui::DragInt(member.name.c_str(), &ival, 1, 0, std::numeric_limits<int>::max()))
                    {
                        val = (float)ival;
                        changed = true;
                    }
                }
            }

            if (changed)
                target->SetFloat("", member.name, val);
        }
    }

    void DrawTextureProperty(const PropertyItem& item, const ShaderFeatures& features, GameEditor& editor)
    {
        auto texture = target->GetTexture(item.name);
        auto newTexture = EditorGUI::TextureField(item.name, texture);
        if (newTexture != texture)
        {
            SetTexture(item.name, newTexture, features);
            texture = newTexture;
        }

        if (texture)
        {
            if (ImGui::TreeNode(("Inspect " + texture->GetName()).c_str()))
            {
                auto inspector = InspectorRegistry::GetInspector(*texture);
                if (inspector->GetTarget() != texture)
                {
                    inspector->OnEnable(*texture);
                }
                inspector->DrawInspector(editor);
                ImGui::TreePop();
            }
        }
    }

private:
    static const char _register;

    void SetTexture(const std::string& param, Texture* tex, const ShaderFeatures& features)
    {
        target->SetTexture(param, tex);

        auto shader = target->GetShaderProgram();
        if (!shader)
            return;

        std::string featureName = "";

        static const std::map<std::string, std::string> explicitMap = {
            {"baseColorTex", "_BaseColorMap"},
            {"normalMap", "_NormalMap"},
            {"emissiveMap", "_EmissiveMap"},
            {"metallicRoughnessMap", "_MetallicRoughnessMap"}};

        auto it = explicitMap.find(param);
        if (it != explicitMap.end())
        {
            featureName = it->second;
        }
        else
        {
            std::string conventionName = "_" + param;
            if (features.featureToBitMask.count(conventionName))
            {
                featureName = conventionName;
            }
            else
            {
                std::string lowerConvention = conventionName;
                std::transform(lowerConvention.begin(), lowerConvention.end(), lowerConvention.begin(), ::tolower);

                for (const auto& f : features.featureToBitMask)
                {
                    std::string lowerF = f.first;
                    std::transform(lowerF.begin(), lowerF.end(), lowerF.begin(), ::tolower);
                    if (lowerF == lowerConvention)
                    {
                        featureName = f.first;
                        break;
                    }
                }
            }
        }

        if (!featureName.empty() && features.featureToBitMask.count(featureName))
        {
            if (tex != nullptr)
                target->EnableFeature(featureName);
            else
                target->DisableFeature(featureName);
        }
    }
};

const char MaterialInspector::_register = InspectorRegistry::Register<MaterialInspector, Material>();

} // namespace Editor
