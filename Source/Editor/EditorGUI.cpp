
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
    }
}
} // namespace Editor
