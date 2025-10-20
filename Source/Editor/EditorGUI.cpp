
#include "EditorGUI.hpp"
#include "Rendering/Material.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
const char* EditorGUI::PayloadType = "_DragDropIntenralTypeID";
std::vector<char> EditorGUI::textArea = std::vector<char>(1024);
void EditorGUI::AutoObjectInspector(Object* target, bool readOnly)
{
    if (target == nullptr)
        return;
    JsonSerializer ser;
    (static_cast<Serializable*>(target))->Serialize(&ser);
    auto j = ser.GetJson();
    bool valueChanged = false;
    JsonInspector(j, valueChanged);

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
    (static_cast<const Serializable*>(target))->Serialize(&ser);
    auto j = ser.GetJson();
    bool valueChanged = false;
    JsonInspector(j, valueChanged);
}

bool EditorGUI::JsonInspector(nlohmann::json& j)
{
    bool valueChanged = false;
    JsonInspector(j, valueChanged);
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

void EditorGUI::JsonInspector(nlohmann::json& j, bool& valueChanged)
{
    const float BaseInputWidth = 60;
    for (auto& item : j.items())
    {
        auto& key = item.key();
        auto& value = item.value();
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

bool EditorGUI::SearchableMenuItems(const std::vector<std::string>& items, std::string& search, int& outSelectedIndex, int& firstItem)
{
    firstItem = -1;
    bool selected = false;

    ImGui::SetKeyboardFocusHere(0);
    InputText("##search", search, "Search Bar");
    for (int idx = 0; idx < items.size(); idx++)
    {
        if (Utils::strContians(Utils::strToLower(items[idx]), Utils::strToLower(search)))
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

void EditorGUI::DrawMaterial(Material& material)
{
}
} // namespace Editor
