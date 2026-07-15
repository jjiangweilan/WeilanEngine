#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Core/Object.hpp"
#include "Editor/EditorState.hpp"
#include "Engine/Library/EnumFlags.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/ThirdParty/imgui/imgui_internal.h"
#include <concepts>
#include <initializer_list>
#include <string_view>

class Material;
namespace Editor
{

enum class DragDropTag
{
    Object = 1,
    Path = 1 << 2
};

struct DragDrop
{
    void* objectPayload;
    const std::type_info* type;
    DragDropTag tags;
    char pathString[1024];
};

ENUM_FLAGS(DragDropTag, int);

class EditorGUI
{
public:
    static bool SearchableMenuItems(const std::vector<std::string>& items, std::string& search, int& outSelectedIndex);
    static bool SearchableMenuItems(const std::vector<std::string>& items, std::string& search, int& outSelectedIndex, int& firstItem, bool searchWithoutBlankSpace = false);

    template <class T>
    static bool Property(const char* name, T& val)
    {
        if constexpr (std::is_same_v<T, int>)
        {
            return DragInt(name, &val);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            return DragFloat(name, &val);
        }
        else if constexpr (std::is_same_v<T, float2>)
        {
            return DragFloat2(name, &val[0]);
        }
        else if constexpr (std::is_same_v<T, float3>)
        {
            return DragFloat3(name, &val[0]);
        }
        else if constexpr (std::is_same_v<T, float4>)
        {
            return DragFloat4(name, &val[0]);
        }
        else if constexpr (std::is_same_v<T, float4x4>)
        {
            bool changed = false;
            // For matrices, we'll display them as 4 rows of 4 floats using table layout
            if (ImGui::BeginTable("##matrix_table", 2))
            {
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("Value");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s:", name);

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("matrix");
                for (int r = 0; r < 4; ++r)
                {
                    ImGui::PushID(r);
                    float4 row = glm::row(val, r);
                    if (ImGui::DragFloat4("##row", &row[0]))
                    {
                        val = glm::row(val, r, row);
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
                ImGui::EndTable();
            }
            return changed;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return InputTextLabeled(name, val);
        }
    }

    template <class T, class TGetter, class TSetter>
    static bool ObjectProperty(const char* name, T& obj, TGetter getter, TSetter setter = nullptr)
    {
        using TReturn = decltype((obj.*getter)());

        const TReturn mutableVal = (obj.*getter)();

        bool changed = false;

        // Handle different types using constexpr if
        if constexpr (std::is_same_v<TReturn, int>)
        {
            changed = DragInt(name, &mutableVal);
        }
        else if constexpr (std::is_same_v<TReturn, float>)
        {
            changed = DragFloat(name, &mutableVal);
        }
        else if constexpr (std::is_same_v<TReturn, float2>)
        {
            changed = DragFloat2(name, &mutableVal[0]);
        }
        else if constexpr (std::is_same_v<TReturn, float3>)
        {
            changed = DragFloat3(name, &mutableVal[0]);
        }
        else if constexpr (std::is_same_v<TReturn, float4>)
        {
            changed = DragFloat4(name, &mutableVal[0]);
        }
        else if constexpr (std::is_same_v<TReturn, float4x4>)
        {
            // For matrices, we'll display them as 4 rows of 4 floats using table layout
            if (ImGui::BeginTable("##matrix_table", 2))
            {
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("Value");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s:", name);

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("matrix");
                for (int r = 0; r < 4; ++r)
                {
                    ImGui::PushID(r);
                    float4 row = glm::row(mutableVal, r);
                    if (ImGui::DragFloat4("##row", &row[0]))
                    {
                        mutableVal = glm::row(mutableVal, r, row);
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
                ImGui::EndTable();
            }
        }
        else if constexpr (std::is_same_v<TReturn, std::string>)
        {
            changed = InputTextLabeled(name, mutableVal);
        }

        // If the value changed, call the setter
        if (setter != nullptr && changed)
        {
            (obj.*setter)(mutableVal);
        }

        return changed;
    }

    template <class T, class TGetter, class TSetter>
    static bool ObjectPropertyEnum(
        const char* name, const std::vector<std::string>& nameList, T& obj, TGetter getter, TSetter setter = nullptr
    )
    {
        using TReturn = decltype((obj.*getter)());

        TReturn currentValue = (obj.*getter)();
        int currentIndex = static_cast<int>(currentValue);

        bool changed = false;

        if (ImGui::BeginTable("##enum_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s: ", name);

            ImGui::TableSetColumnIndex(1);

            if (currentIndex >= 0 && currentIndex < static_cast<int>(nameList.size()))
            {
                if (ImGui::BeginCombo("##combo", nameList[currentIndex].c_str()))
                {
                    for (int i = 0; i < static_cast<int>(nameList.size()); ++i)
                    {
                        bool isSelected = (i == currentIndex);
                        if (ImGui::Selectable(nameList[i].c_str(), isSelected))
                        {
                            if (setter != nullptr && i != currentIndex)
                            {
                                TReturn newValue = static_cast<TReturn>(i);
                                (obj.*setter)(newValue);
                                changed = true;
                            }
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                // Handle invalid enum value
                ImGui::Text("Invalid enum value (%d)", currentIndex);
            }

            ImGui::EndTable();
        }

        return changed;
    }

    template <std::derived_from<Object> T>
    static bool ObjectField(std::string_view name, T*& curr)
    {
        bool newValue = false;
        ImGui::PushID(0);
        if (!name.empty())
            ImGui::Text("%s: ", name.data());
        ImGui::SameLine();
        std::string buttonName = "null";
        if (curr != nullptr)
        {
            buttonName = fmt::format(
                "{}({})",
                curr->GetName().empty() ? curr->GetUUID().ToString().substr(0, 6) : curr->GetName(),
                curr->GetTypeName()
            );
        }

        bool stylePushedForNullCurr = false;
        if (curr == nullptr)
        {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.6f, 0.65, 0.45, 1});
            ImGui::PushStyleColor(ImGuiCol_Button, {0.45f, 0.31, 0.28, 1});
            stylePushedForNullCurr = true;
        }
        if (ImGui::Button(
                curr == nullptr ? T::StaticGetTypeName().c_str()
                                : fmt::format("{}##{}", buttonName.c_str(), curr->GetUUID().ToString()).c_str()
            ))
        {
            EditorState::SelectObject(curr);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            curr = nullptr;
            newValue = true;
        }

        if (stylePushedForNullCurr)
            ImGui::PopStyleColor(2);

        Object* source = nullptr;
        if (DragDropTarget(source))
        {
            T* t = dynamic_cast<T*>(source);
            if (t != nullptr)
            {
                curr = (T*)source;
                newValue = true;
            }
            else if (source != nullptr)
            {
                if (GameObject* asGameObject = dynamic_cast<GameObject*>(source))
                {
                    auto sourceFound = asGameObject->GetComponent<T>();
                    if (sourceFound)
                    {
                        curr = (T*)sourceFound;
                        newValue = true;
                    }
                }
            }
        }
        else if (DragDropTarget(typeid(GameObject), source))
        {
            GameObject* go = (GameObject*)source;
            curr = go->GetComponent<T>();
            newValue = curr != nullptr;
        }

        ImGui::PopID();

        return newValue;
    }

    static const char* ShaderPicker(const char* shaderName);

    static bool DragDropSource(const char* text, std::function<void(Object*& obj)> onDrag, ImGuiDragDropFlags flags = 0)
    {
        bool isValid = false;
        if (ImGui::BeginDragDropSource())
        {
            Object* obj = nullptr;
            onDrag(obj);
            DragDrop d;
            d.objectPayload = obj;
            d.tags = DragDropTag::Object;
            d.type = &typeid(*obj);
            ImGui::SetDragDropPayload(PayloadType, &d, sizeof(DragDrop));
            ImGui::Text("%s", text);
            isValid = true;
            ImGui::EndDragDropSource();
        }

        return isValid;
    }

    static bool DragDropSource(const std::filesystem::path& path, ImGuiDragDropFlags flags = 0)
    {
        std::string asString = path.string();
        if (asString.size() > 1024)
            return false;

        bool isValid = false;
        if (ImGui::BeginDragDropSource(flags))
        {
            DragDrop d;
            strcpy(d.pathString, asString.data());
            d.tags = DragDropTag::Path;
            ImGui::SetDragDropPayload(PayloadType, &d, sizeof(DragDrop));
            ImGui::Text("%s", asString.data());
            isValid = true;
            ImGui::EndDragDropSource();
        }

        return isValid;
    }

    static bool DragDropSource(
        const std::filesystem::path& path, std::function<void(Object*& obj)> onDrag, ImGuiDragDropFlags flags = 0
    )
    {
        std::string asString = path.string();
        if (asString.size() > 1024)
            return false;

        bool isValid = false;

        if (ImGui::BeginDragDropSource(flags))
        {
            DragDrop d = {};
            Object* payload = nullptr;
            d.tags = DragDropTag::Path;
            if (onDrag)
            {
                onDrag(payload);
                if (payload != nullptr)
                {
                    d.type = &typeid(*payload);
                    d.objectPayload = payload;
                    d.tags |= DragDropTag::Object;
                }
            }

            strcpy(d.pathString, asString.data());
            ImGui::SetDragDropPayload(PayloadType, &d, sizeof(DragDrop));
            ImGui::Text("%s", asString.data());
            isValid = true;
            ImGui::EndDragDropSource();
        }

        return isValid;
    }

    static bool DragDropSource(const char* text, Object* object, ImGuiDragDropFlags flags = 0)
    {
        bool isValid = false;
        if (ImGui::BeginDragDropSource(flags))
        {
            DragDrop d;
            d.objectPayload = object;
            d.tags = DragDropTag::Object;
            d.type = &typeid(*object);
            ImGui::SetDragDropPayload(PayloadType, &d, sizeof(DragDrop));
            ImGui::Text("%s", text);
            isValid = true;
            ImGui::EndDragDropSource();
        }

        return isValid;
    }

    static bool DragDropTarget(AssetPath& path, ImRect rect = {{0, 0}, {0, 0}})
    {
        bool isValid = false;
        path = "";

        bool begin = false;
        if (rect.Min.x == 0 && rect.Min.y == 0 && rect.Max.x == 0 && rect.Max.y == 0)
        {
            begin = ImGui::BeginDragDropTarget();
        }
        else
        {

            begin = ImGui::BeginDragDropTargetCustom(rect, 999);
        }

        if (begin)
        {
            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            if (payload && payload->IsDataType(PayloadType))
            {
                DragDrop* dragDrop = (DragDrop*)payload->Data;

                if (HasFlag(dragDrop->tags, DragDropTag::Path))
                {
                    ImGui::AcceptDragDropPayload(PayloadType);
                    if (payload->IsDelivery())
                    {
                        path = (char*)dragDrop->pathString;
                        isValid = true;
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        return isValid;
    }

    static bool DragDropTarget(Object*& obj, ImRect rect = {{0, 0}, {0, 0}})
    {
        return DragDropTarget(obj, rect, nullptr);
    }

    static bool DragDropTarget(const std::type_info& type, Object*& obj, ImRect rect = {{0, 0}, {0, 0}})
    {
        return DragDropTarget(obj, rect, &type);
    }

    template <typename T>
    static bool DropZone(const char* label, T*& outObj, float height = 40.0f)
    {
        if constexpr (std::is_same_v<T, Texture>)
        {
            if (outObj != nullptr)
            {
                auto objName = fmt::format("{}: {}", label, outObj->GetName());
                ImGui::Text("%s", objName.c_str());
                ImGui::Image(&outObj->GetGfxImage()->GetDefaultImageView(), {100, 100});
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    EditorState::SelectObject(outObj);
                }
                auto regionMin = ImGui::GetItemRectMin();
                auto regionMax = ImGui::GetItemRectMax();
                Object* obj = nullptr;
                if (DragDropTarget(typeid(T), obj, {regionMin, regionMax}))
                {
                    outObj = static_cast<T*>(obj);
                    return true;
                }
                return false;
            }
            DropZoneVisual(label, -1.0f, height);
            Object* obj = nullptr;
            if (DragDropTarget(typeid(T), obj))
            {
                outObj = static_cast<T*>(obj);
                return true;
            }
            return false;
        }
        else
        {
            DropZoneVisual(label, -1.0f, height);
            Object* obj = nullptr;
            if (DragDropTarget(typeid(T), obj))
            {
                outObj = static_cast<T*>(obj);
                return true;
            }
            return false;
        }
    }

    static bool DropZone(const char* label, AssetPath& outPath, float height = 40.0f)
    {
        DropZoneVisual(label, -1.0f, height);
        return DragDropTarget(outPath);
    }

    static bool EnumDropDown(
        const char* fieldName, int& val, int totalEnums, std::function<std::string(int)> mapToString
    )
    {
        int current = val;
        bool selected = false;
        if (ImGui::BeginCombo(fieldName, mapToString(val).c_str()))
        {
            for (int i = 0; i < totalEnums; i++)
            {
                std::string name = mapToString(i);
                if (ImGui::Selectable(name.c_str(), i == current))
                {
                    val = i;
                    selected = true;
                }
            }

            ImGui::EndCombo();
        }

        return selected;
    }

    static void Image(Gfx::Image& image, const float2& size)
    {
        ImGui::Image(&image.GetDefaultImageView(), {size.x, size.y});
    }

    static void DrawMaterial(Material& material, const std::vector<std::string>& disabledFields = {});

    static void Image(Gfx::Image& image, const float2& minPos, const float2 maxPos)
    {
        ImGui::GetWindowDrawList()->AddImage(&image.GetDefaultImageView(), minPos, maxPos);
    }

    static Texture* TextureField(const std::string& name, Texture* texture);

    static bool InputText(const char* label, std::string& text, const char* hind = nullptr, ImGuiInputTextFlags flags = 0)
    {
        if (textArea.size() < text.size() + 1)
        {
            textArea.resize((text.size() + 1) * 2);
        }

        bool output = false;
        std::strcpy(textArea.data(), text.data());
        std::memset(textArea.data() + text.size(), 0, textArea.size() - text.size());
        if (hind != nullptr)
        {
            if (ImGui::InputTextWithHint(label, hind, textArea.data(), textArea.size(), flags))
                output = true;
        }

        else
        {
            if (ImGui::InputText(label, textArea.data(), textArea.size(), flags))
                output = true;
        }

        text.assign(textArea.data());
        return output;
    }

    static void AutoObjectInspector(const Object* target);
    static void AutoObjectInspector(
        Object* target,
        bool readOnly = false,
        std::initializer_list<std::string_view> excludedKeys = {}
    );
    static bool JsonInspector(nlohmann::json& j, const std::vector<std::string>& keys = {});
    static void JsonInspector(nlohmann::json& j, bool& valueChanged, const std::vector<std::string>& keys = {});

    // Wrapper functions for consistent UI layout using table API
    static void Text(const char* label, const char* value)
    {
        if (ImGui::BeginTable("##text_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", value);

            ImGui::EndTable();
        }
    }

    static void Text(const char* label, const std::string& value)
    {
        Text(label, value.c_str());
    }

    template <typename... Args>
    static void TextFormatted(const char* label, const char* format, Args... args)
    {
        if (ImGui::BeginTable("##text_formatted_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text(format, args...);

            ImGui::EndTable();
        }
    }

    static bool DragFloat(const char* label, float* value, float speed = 1.0f, float min = 0.0f, float max = 0.0f)
    {
        bool changed = false;
        if (ImGui::BeginTable("##dragfloat_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::DragFloat("##value", value, speed, min, max);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool DragFloat2(const char* label, float* value, float speed = 1.0f, float min = 0.0f, float max = 0.0f)
    {
        bool changed = false;
        if (ImGui::BeginTable("##dragfloat2_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::DragFloat2("##value", value, speed, min, max);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool DragFloat3(const char* label, float* value, float speed = 1.0f, float min = 0.0f, float max = 0.0f)
    {
        bool changed = false;
        if (ImGui::BeginTable("##dragfloat3_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::DragFloat3("##value", value, speed, min, max);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool DragFloat4(const char* label, float* value, float speed = 1.0f, float min = 0.0f, float max = 0.0f)
    {
        bool changed = false;
        if (ImGui::BeginTable("##dragfloat4_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::DragFloat4("##value", value, speed, min, max);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool DragInt(const char* label, int* value, float speed = 1.0f, int min = 0, int max = 0)
    {
        bool changed = false;
        if (ImGui::BeginTable("##dragint_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::DragInt("##value", value, speed, min, max);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool Checkbox(const char* label, bool* value)
    {
        bool changed = false;
        if (ImGui::BeginTable("##checkbox_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::Checkbox("##value", value);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool Button(const char* label, const char* buttonText)
    {
        bool clicked = false;
        if (ImGui::BeginTable("##button_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            clicked = ImGui::Button(buttonText);

            ImGui::EndTable();
        }
        return clicked;
    }

    static bool InputTextLabeled(const char* label, char* buffer, size_t bufferSize, ImGuiInputTextFlags flags = 0)
    {
        bool changed = false;
        // if (ImGui::BeginTable("##inputtext_table", 2, ImGuiTableFlags_Resizable))
        {
            // ImGui::TableSetupColumn("Label");
            // ImGui::TableSetupColumn("Value");
            //
            // ImGui::TableNextRow();
            // ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            // ImGui::TableSetColumnIndex(1);
            changed = ImGui::InputText("##value", buffer, bufferSize, flags);

            // ImGui::EndTable();
        }
        return changed;
    }

    static bool InputTextLabeled(const char* label, std::string& text, ImGuiInputTextFlags flags = 0)
    {
        if (textArea.size() < text.size() + 1)
        {
            textArea.resize((text.size() + 1) * 2);
        }

        std::strcpy(textArea.data(), text.data());

        bool changed = false;
        if (ImGui::BeginTable("##inputtext_string_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::InputText("##value", textArea.data(), textArea.size(), flags);

            ImGui::EndTable();
        }

        if (changed)
        {
            text = textArea.data();
        }

        return changed;
    }

    static bool ComboLabeled(const char* label, int* currentItem, const char* const items[], int itemsCount)
    {
        bool changed = false;
        if (ImGui::BeginTable("##combo_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::Combo("##value", currentItem, items, itemsCount);

            ImGui::EndTable();
        }
        return changed;
    }

    static bool ComboLabeled(const char* label, int* currentItem, const char* itemsSeparatedByZeros)
    {
        bool changed = false;
        if (ImGui::BeginTable("##combo_separated_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            changed = ImGui::Combo("##value", currentItem, itemsSeparatedByZeros);

            ImGui::EndTable();
        }
        return changed;
    }

    // Image display with label
    static void ImageLabeled(const char* label, Gfx::ImageView* imageView, const ImVec2& size)
    {
        if (ImGui::BeginTable("##image_table", 2))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s:", label);

            ImGui::TableSetColumnIndex(1);
            if (imageView)
            {
                ImGui::Image(imageView, size);
            }
            else
            {
                ImGui::Text("No image");
            }

            ImGui::EndTable();
        }
    }

    // Utility function for consistent spacing
    static void SeparatorTextLabeled(const char* text)
    {
        ImGui::SeparatorText(text);
    }

    // For simple buttons without labels (maintain backward compatibility)
    static bool ButtonSimple(const char* buttonText)
    {
        return ImGui::Button(buttonText);
    }

private:
    static const char* PayloadType;
    static std::vector<char> textArea;

    static void DropZoneVisual(const char* label, float width, float height)
    {
        float w = (width < 0) ? ImGui::GetContentRegionAvail().x : width;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(w, height);
        ImVec2 end(pos.x + w, pos.y + height);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, end, IM_COL32(35, 40, 50, 255), 6.0f);
        dl->AddRect(pos, end, IM_COL32(100, 110, 130, 255), 6.0f, 0, 2.0f);

        ImVec2 textSize = ImGui::CalcTextSize(label);
        dl->AddText(
            ImVec2(pos.x + (w - textSize.x) * 0.5f, pos.y + (height - textSize.y) * 0.5f),
            IM_COL32(160, 170, 190, 255), label);

        ImGui::InvisibleButton("##DropZone", size);
    }

    static bool DragDropTarget(Object*& obj, ImRect rect, const std::type_info* type)
    {
        bool isValid = false;
        obj = nullptr;

        bool begin = false;
        if (rect.Min.x == 0 && rect.Min.y == 0 && rect.Max.x == 0 && rect.Max.y == 0)
        {
            begin = ImGui::BeginDragDropTarget();
        }
        else
        {

            begin = ImGui::BeginDragDropTargetCustom(rect, 999);
        }

        if (begin)
        {
            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            if (payload && payload->IsDataType(PayloadType))
            {
                DragDrop* dragDrop = (DragDrop*)payload->Data;
                bool validObject = HasFlag(dragDrop->tags, DragDropTag::Object);
                bool validType = validObject;
                if (type != nullptr && validObject)
                {
                    validType = *dragDrop->type == *type;
                }
                if (validObject && validType)
                {
                    ImGui::AcceptDragDropPayload(PayloadType);
                    if (payload->IsDelivery())
                    {
                        obj = (Object*)dragDrop->objectPayload;
                        isValid = true;
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        return isValid;
    }
};
} // namespace Editor
