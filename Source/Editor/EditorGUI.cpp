
#include "EditorGUI.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
const char* GUI::PayloadType = "_DragDropIntenralTypeID";
std::vector<char> GUI::textArea = std::vector<char>(1024);
void GUI::AutoObjectInspector(Object* target)
{
    if (target == nullptr)
        return;
    JsonSerializer ser;
    (static_cast<Serializable*>(target))->Serialize(&ser);
    auto j = ser.GetJson();
    bool valueChanged = false;
    JsonInspectorInternal(j, valueChanged);

    if (valueChanged)
    {
        JsonSerializer newSer(j);
        (static_cast<Serializable*>(target))->Deserialize(&newSer);
        static_cast<Asset*>(target)->SetDirty();
    }
}

bool GUI::JsonInspector(nlohmann::json& j)
{
    bool valueChanged = false;
    JsonInspectorInternal(j, valueChanged);
    return valueChanged;
}

void GUI::JsonInspectorInternal(nlohmann::json& j, bool& valueChanged)
{
    for (auto& item : j.items())
    {
        auto& key = item.key();
        auto& value = item.value();
        if (value.is_object())
        {
            if (ImGui::TreeNode(key.c_str()))
            {
                JsonInspectorInternal(value, valueChanged);
                ImGui::TreePop();
            }
        }
        else if (value.is_number_float())
        {
            float val = value;
            ImGui::SetNextItemWidth(80);
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
            ImGui::SetNextItemWidth(80);
            if (length == 2)
            {
                glm::float2 val;
                val.x = value[0];
                val.y = value[1];
                if (ImGui::InputFloat2(key.c_str(), &val[0]))
                {
                    value[0] = val.x;
                    value[1] = val.y;
                    valueChanged = true;
                }
            }
            else if (length == 3)
            {
                glm::float3 val;
                val.x = value[0];
                val.y = value[1];
                val.z = value[2];
                if (ImGui::InputFloat3(key.c_str(), &val[0]))
                {
                    value[0] = val.x;
                    value[1] = val.y;
                    value[2] = val.z;
                    valueChanged = true;
                }
            }
            else if (length == 4)
            {
                glm::float4 val;
                val.x = value[0];
                val.y = value[1];
                val.z = value[2];
                val.w = value[3];
                if (ImGui::InputFloat4(key.c_str(), &val[0]))
                {
                    value[0] = val.x;
                    value[1] = val.y;
                    value[2] = val.z;
                    value[3] = val.w;
                    valueChanged = true;
                }
            }
        }
        else if (value.is_string())
        {
            bool isUUID = false;
            std::string text = value;
            if (text.size() == 36) // potentially a UUID
            {
                auto uuid = UUID(text);
                if (!uuid.IsEmpty())
                {
                    isUUID = true;

                    // 'uuid' is treated specially because it's most likely the uuid of the object itself
                    if (std::strcmp(key.c_str(), "uuid") != 0)
                    {
                        ObjPtr<Object> objPtr(uuid);
                        Object* obj = objPtr.Get();
                        if (ObjectField(key.c_str(), obj))
                        {
                            value = obj->GetUUID().ToString();
                        }
                    }
                }
            }

            if (!isUUID)
            {
                if (GUI::InputText(key.c_str(), text))
                {
                    value = text;
                }
            }
        }
    }
}
} // namespace Editor
