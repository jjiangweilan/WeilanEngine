
#include "Editor/EditorGUI.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Library/Serialization/SerializationSequenceFetcher.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{
const char* EditorGUI::PayloadType = "_DragDropIntenralTypeID";
std::vector<char> EditorGUI::textArea = std::vector<char>(1024);
void EditorGUI::AutoObjectInspector(Object* target, bool readOnly)
{
    if (target == nullptr)
        return;
    JsonSerializer ser;
    SerializationSequenceFetcher keySequenceFetcher;
    (static_cast<Serializable*>(target))->Serialize(&ser);
    (static_cast<Serializable*>(target))->Serialize(&keySequenceFetcher);
    auto j = ser.GetJson();
    std::vector<std::string> keys = keySequenceFetcher.GetKeySequence();
    std::erase_if(keys, [](auto& key)
                  { return key == "gameObject" || key == "uuid" || key == "name" || key == "enabled"; });
    bool valueChanged = false;
    JsonInspector(j, valueChanged, keys);

    if (valueChanged && !readOnly)
    {
        JsonSerializer newSer(j);
        (static_cast<Serializable*>(target))->Deserialize(&newSer);
        static_cast<Asset*>(target)->SetDirty();
    }
}

void EditorGUI::AutoObjectInspector(const Object* target)
{
    if (target == nullptr)
        return;
    JsonSerializer ser;
    SerializationSequenceFetcher keySequenceFetcher;
    (static_cast<const Serializable*>(target))->Serialize(&ser);
    (static_cast<const Serializable*>(target))->Serialize(&keySequenceFetcher);
    auto j = ser.GetJson();
    bool valueChanged = false;
    std::vector<std::string> keys = keySequenceFetcher.GetKeySequence();
    std::erase_if(keys, [](auto& key)
                  { return key == "gameObject" || key == "uuid" || key == "name" || key == "enabled"; });
    JsonInspector(j, valueChanged, keys);
}

bool EditorGUI::JsonInspector(nlohmann::json& j, const std::vector<std::string>& keys)
{
    bool valueChanged = false;
    JsonInspector(j, valueChanged, keys);
    return valueChanged;
}

const char* EditorGUI::ShaderPicker(const char* shaderName)
{
    int currentIdx = -1;
    for (int i = 0; i < (int)Shaders::MAX_COUNT; i++)
    {
        if (strcmp(shaderName, ShaderLibrary::ShaderNameMap[i]) == 0)
        {
            currentIdx = i;
            break;
        }
    }

    ImGui::Combo("Shader", &currentIdx, ShaderLibrary::ShaderNameMap, (int)Shaders::MAX_COUNT);

    if (currentIdx != -1)
    {
        return ShaderLibrary::GetShaderName((Shaders)currentIdx);
    }

    return nullptr;
}

void EditorGUI::JsonInspector(nlohmann::json& j, bool& valueChanged, const std::vector<std::string>& keys)
{
    const float BaseInputWidth = 60;

    const std::vector<std::string>* realKeys = &keys;
    std::vector<std::string> copies;
    if (keys.empty())
    {
        for (auto& i : j.items())
        {
            copies.push_back(i.key());
        }

        realKeys = &copies;
    }

    for (auto& key : *realKeys)
    {
        if (!j.contains(key))
            continue;

        auto& value = j[key];
        if (value.is_object())
        {
            if (ImGui::TreeNode(key.c_str()))
            {
                JsonInspector(value, valueChanged);
                ImGui::TreePop();
            }
        }
        else if (value.is_number_integer())
        {
            int val = value;
            ImGui::SetNextItemWidth(BaseInputWidth);
            if (ImGui::DragInt(key.c_str(), &val))
            {
                value = val;
                valueChanged = true;
            }
        }
        else if (value.is_number_float())
        {
            float val = value;
            ImGui::SetNextItemWidth(BaseInputWidth);
            if (ImGui::DragFloat(key.c_str(), &val))
            {
                value = val;
                valueChanged = true;
            }
        }
        else if (value.is_boolean())
        {
            bool val = value;
            if (ImGui::Checkbox(key.c_str(), &val))
            {
                value = val;
                valueChanged = true;
            }
        }
        else if (value.is_array())
        {
            int length = value.size();
            if (length == 2 && value[0].is_number() && value[1].is_number())
            {
                glm::float2 val;
                val.x = value[0];
                val.y = value[1];
                ImGui::SetNextItemWidth(BaseInputWidth * 2);
                if (ImGui::DragFloat2(key.c_str(), &val[0]))
                {
                    value[0] = val.x;
                    value[1] = val.y;
                    valueChanged = true;
                }
            }
            else if (length == 3 && value[0].is_number() && value[1].is_number() && value[2].is_number())
            {
                glm::float3 val;
                val.x = value[0];
                val.y = value[1];
                val.z = value[2];
                ImGui::SetNextItemWidth(BaseInputWidth * 3);
                if (ImGui::DragFloat3(key.c_str(), &val[0]))
                {
                    value[0] = val.x;
                    value[1] = val.y;
                    value[2] = val.z;
                    valueChanged = true;
                }
            }
            else if (length == 4 && value[0].is_number() && value[1].is_number() && value[2].is_number() &&
                     value[3].is_number())
            {
                glm::float4 val;
                val.x = value[0];
                val.y = value[1];
                val.z = value[2];
                val.w = value[3];
                ImGui::SetNextItemWidth(BaseInputWidth * 4);
                if (ImGui::DragFloat4(key.c_str(), &val[0]))
                {
                    value[0] = val.x;
                    value[1] = val.y;
                    value[2] = val.z;
                    value[3] = val.w;
                    valueChanged = true;
                }
            }
            else
            {
                if (ImGui::TreeNode(key.c_str()))
                {
                    JsonInspector(value, valueChanged);
                    ImGui::TreePop();
                }
            }
        }
        else if (value.is_string())
        {
            bool isUUID = false;
            std::string text = value;
            if (text.size() == 36 && text[8] == '-' && text[13] == '-' && text[18] == '-' &&
                text[23] == '-') // potentially a UUID
            {
                isUUID = true;
                auto uuid = UUID(text);
                // 'uuid' is treated specially because it's most likely the uuid of the object itself
                if (std::strcmp(key.c_str(), "uuid") != 0)
                {
                    ObjPtr<Object> objPtr(uuid);
                    Object* obj = objPtr.Get();
                    if (ObjectField(key.c_str(), obj))
                    {
                        if (obj)
                            value = obj->GetUUID().ToString();
                        else
                            value = UUID::GetEmptyUUID().ToString();
                        valueChanged = true;
                    }
                }
            }

            if (!isUUID)
            {
                if (EditorGUI::InputText(key.c_str(), text))
                {
                    value = text;
                    valueChanged = true;
                }
            }
        }
    }
}

bool EditorGUI::SearchableMenuItems(const std::vector<std::string>& items, std::string& search, int& outSelectedIndex)
{
    int firstItemIdx = -1;
    return SearchableMenuItems(items, search, outSelectedIndex, firstItemIdx);
}

bool EditorGUI::SearchableMenuItems(const std::vector<std::string>& items, std::string& search, int& outSelectedIndex, int& firstItem, bool searchWithoutBlankSpace)
{
    firstItem = -1;
    bool selected = false;

    ImGui::SetKeyboardFocusHere(0);
    InputText("##search", search, "Search Bar");
    for (int idx = 0; idx < items.size(); idx++)
    {
        if (Utils::strContians(Utils::strToLower(searchWithoutBlankSpace ? Utils::strRemoveBlanks(items[idx]) : items[idx]), Utils::strToLower(search)))
        {
            if (firstItem == -1)
                firstItem = idx;

            if (ImGui::MenuItem(items[idx].c_str()))
            {
                outSelectedIndex = idx;
                selected = true;
                break;
            }
        }
    }
    return selected;
}

void EditorGUI::DrawMaterial(Material& material, const std::vector<std::string>& disabledFields)
{
    auto SetTexture = [&material](const std::string& param, Texture* tex)
    {
        material.SetTexture(param, tex);
        if (param == "baseColorTex")
        {
            if (tex != nullptr)
                material.EnableFeature("_BaseColorMap");
            else
                material.DisableFeature("_BaseColorMap");
        }
        else if (param == "normalMap")
        {
            if (tex != nullptr)
                material.EnableFeature("_NormalMap");
            else
                material.DisableFeature("_NormalMap");
        }
        else if (param == "emissiveMap")
        {
            if (tex != nullptr)
                material.EnableFeature("_EmissiveMap");
            else
                material.DisableFeature("_EmissiveMap");
        }
        else if (param == "metallicRoughnessMap")
        {
            if (tex != nullptr)
                material.EnableFeature("_MetallicRoughnessMap");
            else
                material.DisableFeature("_MetallicRoughnessMap");
        }
    };

    auto IsColorAttribute = [](const Gfx::ShaderPipelineInfo::BufferMember& member) -> bool
    {
        for (const auto& attr : member.attributes)
        {
            if (attr == "Color" || attr == "color" || attr == "COLOR")
            {
                return true;
            }
        }
        return false;
    };

    auto shader = material.GetShader()->GetShaderProgram();
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
                    if (std::find(disabledFields.begin(), disabledFields.end(), member.name) != disabledFields.end())
                    {
                        continue;
                    }
                    if (member.IsVector())
                    {
                        glm::float4 val = material.GetVector("", member.name);

                        if (member.rowCount == 4)
                        {
                            if (IsColorAttribute(member))
                            {
                                if (ImGui::ColorEdit4(member.name.c_str(), &val[0]))
                                {
                                    material.SetVector("", member.name, val);
                                }
                            }
                            else if (EditorGUI::DragFloat4(member.name.c_str(), &val[0]))
                            {
                                material.SetVector("", member.name, val);
                            }
                        }
                        if (member.rowCount == 3 && Utils::strContians(Utils::strToLower(member.name), "color"))
                        {
                            if (IsColorAttribute(member))
                            {
                                if (ImGui::ColorPicker3(member.name.c_str(), &val[0]))
                                {
                                    material.SetVector("", member.name, val);
                                }
                            }
                            else if (EditorGUI::DragFloat3(member.name.c_str(), &val[0]))
                            {
                                material.SetVector("", member.name, val);
                            }
                        }
                        else if (member.rowCount == 2)
                        {
                            if (EditorGUI::DragFloat2(member.name.c_str(), &val[0]))
                            {
                                material.SetVector("", member.name, val);
                            }
                        }
                    }
                    else if (member.IsElement())
                    {
                        float val = material.GetFloat("", member.name);

                        if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Float)
                        {
                            if (EditorGUI::DragFloat(member.name.c_str(), &val))
                            {
                                material.SetFloat("", member.name, val);
                            }
                        }
                        else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::Int)
                        {
                            int ival = val;
                            if (EditorGUI::DragInt(member.name.c_str(), &ival))
                            {
                                material.SetFloat("", member.name, val);
                            }
                        }
                        else if (member.type == Gfx::ShaderPipelineInfo::MemberDataType::UInt)
                        {
                            int ival = val;
                            if (EditorGUI::DragInt(member.name.c_str(), &ival, 1, 0, std::numeric_limits<int>::max()))
                            {
                                material.SetFloat("", member.name, val);
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
                    if (std::find(disabledFields.begin(), disabledFields.end(), binding->name) != disabledFields.end())
                    {
                        continue;
                    }

                    auto texture = material.GetTexture(binding->name);
                    Texture* newTexture = EditorGUI::TextureField(binding->name, texture);
                    if (newTexture != texture)
                    {
                        SetTexture(binding->name, newTexture);
                    }
                }
            }
        }
    }
}

Texture* EditorGUI::TextureField(const std::string& name, Texture* texture)
{
    if (texture != nullptr)
    {
        ImGui::Text("Texture: %s", name.c_str());
        ImGui::Image(&texture->GetGfxImage()->GetDefaultImageView(), {100, 100});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            EditorState::SelectObject(texture);
        }
        AssetPath path;

        auto regionMin = ImGui::GetItemRectMin();
        auto regionMax = ImGui::GetItemRectMax();
        if (EditorGUI::DragDropTarget(path, {regionMin, regionMax}))
        {
            Texture* tex = dynamic_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(path));
            if (tex)
                return tex;
        }

        ImGui::SameLine();
        if (ImGui::Button("x"))
        {
            return nullptr;
        }
    }
    else
    {
        AssetPath path;
        if (EditorGUI::DropZone(name.c_str(), path))
        {
            auto tex = dynamic_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(path));
            if (tex)
                return tex;
        }
    }

    return texture;
}
} // namespace Editor
