#include "Editor/EditorState.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Editor/EditorGUI.hpp"
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Driver/GfxDriver/ShaderProgram.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "MaterialAttributeParser.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <map>
#include <limits>
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
        if (const char* picked = EditorGUI::ShaderPicker(shaderName))
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

        DrawMaterialProperties(shader);

        if (ImGui::TreeNode("Auto Inspector"))
        {
            EditorGUI::AutoObjectInspector(target, true);
            ImGui::TreePop();
        }
    }

private:
    struct PropertyItem
    {
        std::string name;
        MaterialAttributeInfo info;
        const Gfx::ShaderPipelineInfo::BufferMember* member = nullptr;
        const Gfx::ShaderPipelineInfo::Binding* binding = nullptr;
    };

    void DrawMaterialProperties(Gfx::ShaderProgram* shader)
    {
        if (shader)
        {
            auto& pipelineInfo = shader->GetShaderInfo();
            auto set = pipelineInfo.GetDescriptorSet(Gfx::DescriptorSetSemantics::Material);
            if (set)
            {
                std::map<std::string, std::vector<PropertyItem>> groups;
                std::vector<PropertyItem> ungrouped;

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
                        // For now textures don't have attributes on Binding
                        item.binding = &binding;
                        if (item.info.group)
                            groups[item.info.group->name].push_back(item);
                        else
                            ungrouped.push_back(item);
                    }
                }

                for (const auto& item : ungrouped)
                {
                    DrawProperty(item);
                }

                for (auto& [groupName, items] : groups)
                {
                    if (ImGui::CollapsingHeader(groupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        for (const auto& item : items)
                        {
                            DrawProperty(item);
                        }
                    }
                }
            }
        }
    }

    void DrawProperty(const PropertyItem& item)
    {
        if (item.member)
        {
            DrawBufferMember(item);
        }
        else if (item.binding)
        {
            DrawTextureProperty(item);
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

    void DrawTextureProperty(const PropertyItem& item)
    {
        const auto& binding = *item.binding;
        auto texture = target->GetTexture(binding.name);
        if (texture != nullptr)
        {
            ImGui::Text("Texture: %s", binding.name.c_str());
            ImGui::Image(&texture->GetGfxImage()->GetDefaultImageView(), {100, 100});
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                EditorState::SelectObject(texture);
            }
            std::filesystem::path path;

            auto regionMin = ImGui::GetItemRectMin();
            auto regionMax = ImGui::GetItemRectMax();
            if (EditorGUI::DragDropTarget(path, {regionMin, regionMax}))
            {
                auto tex = dynamic_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(path));
                if (tex)
                    SetTexture(binding.name, tex);
            }

            ImGui::SameLine();
            if (ImGui::Button("x"))
            {
                SetTexture(binding.name, nullptr);
            }
        }
        else
        {
            ImGui::Button(binding.name.c_str());
            std::filesystem::path path;
            if (EditorGUI::DragDropTarget(path))
            {
                auto tex = dynamic_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(path));
                if (tex)
                    SetTexture(binding.name, tex);
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

    void SetTexture(const std::string& param, Texture* tex)
    {
        target->SetTexture(param, tex);
        if (param == "baseColorTex")
        {
            if (tex != nullptr)
                target->EnableFeature("_BaseColorMap");
            else
                target->DisableFeature("_BaseColorMap");
        }
        else if (param == "normalMap")
        {
            if (tex != nullptr)
                target->EnableFeature("_NormalMap");
            else
                target->DisableFeature("_NormalMap");
        }
        else if (param == "emissiveMap")
        {
            if (tex != nullptr)
                target->EnableFeature("_EmissiveMap");
            else
                target->DisableFeature("_EmissiveMap");
        }
        else if (param == "metallicRoughnessMap")
        {
            if (tex != nullptr)
                target->EnableFeature("_MetallicRoughnessMap");
            else
                target->DisableFeature("_MetallicRoughnessMap");
        }
    }
};

const char MaterialInspector::_register = InspectorRegistry::Register<MaterialInspector, Material>();

} // namespace Editor
