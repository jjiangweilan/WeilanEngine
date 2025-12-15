#include "Editor/EditorState.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Editor/EditorGUI.hpp"
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Driver/GfxDriver/ShaderProgram.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
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

        {
            bool alphaClip = target->IsFeatureEnabled("_AlphaClip");
            if (ImGui::Checkbox("Alpha Clip", &alphaClip))
            {
                if (alphaClip)
                    target->EnableFeature("_AlphaClip");
                else
                    target->DisableFeature("_AlphaClip");
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

        Draw(shader);

        if (ImGui::TreeNode("Auto Inspector"))
        {
            EditorGUI::AutoObjectInspector(target, true);
            ImGui::TreePop();
        }
    }

    void Draw(Gfx::ShaderProgram* shader)
    {
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
                                }
                            }
                            if (member.rowCount == 3)
                            {
                                if (ImGui::DragFloat3(member.name.c_str(), &val[0]))
                                {
                                    target->SetVector("", member.name, val);
                                }
                            }
                            else if (member.rowCount == 2)
                            {
                                if (ImGui::DragFloat2(member.name.c_str(), &val[0]))
                                {
                                    target->SetVector("", member.name, val);
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
                                }
                            }
                            else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Int)
                            {
                                int ival = val;
                                if (ImGui::DragInt(member.name.c_str(), &ival))
                                {
                                    target->SetFloat("", member.name, val);
                                }
                            }
                            else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::UInt)
                            {
                                int ival = val;
                                if (ImGui::DragInt(member.name.c_str(), &ival, 1, 0, std::numeric_limits<int>::max()))
                                {
                                    target->SetFloat("", member.name, val);
                                }
                            }
                        }
                    }
                }

                for (int i = 0; i < set->GetBindingCount(); ++i)
                {
                    const auto& binding = set->GetBinding(i);
                    if (binding->descriptorType == Gfx::DescriptorType::CombinedImageSampler ||
                        binding->descriptorType == Gfx::DescriptorType::SampledImage)
                    {
                        auto texture = target->GetTexture(binding->name);
                        if (texture != nullptr)
                        {
                            ImGui::Text("Texture: %s", binding->name.c_str());
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
                                    SetTexture(binding->name, tex);
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("x"))
                            {
                                SetTexture(binding->name, nullptr);
                            }
                        }
                        else
                        {
                            ImGui::Button(binding->name.c_str());
                            std::filesystem::path path;
                            if (EditorGUI::DragDropTarget(path))
                            {
                                auto tex = dynamic_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(path));
                                if (tex)
                                    SetTexture(binding->name, tex);
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
