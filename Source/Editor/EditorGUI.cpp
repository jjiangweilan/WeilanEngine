
#include "EditorGUI.hpp"

namespace Editor
{
const char* GUI::PayloadType = "_DragDropIntenralTypeID";
std::vector<char> GUI::textArea = std::vector<char>(1024);
void GUI::AutoObjectInspector(Object* target)
{
    JsonSerializer ser;
    (static_cast<Serializable*>(target))->Serialize(&ser);
    auto j = ser.GetJson();

    bool valueChanged = false;
    for (auto& item : j.items())
    {
        auto& key = item.key();
        auto& value = item.value();
        if (value.is_number_float())
        {
            float val = value;
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat(key.c_str(), &val))
            {
                ser.Serialize(key, val);
                valueChanged = true;
            }
        }
    }

    if (valueChanged)
    {
        (static_cast<Serializable*>(target))->Deserialize(&ser);
    }
}
} // namespace Editor
