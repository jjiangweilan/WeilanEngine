#pragma once
#include "Core/Asset.hpp"
#include "Core/GameObject.hpp"
#include "Core/Object.hpp"
#include "EditorState.hpp"
#include "Libs/EnumFlags.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include <concepts>
#include <string_view>

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

class GUI
{
public:
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

    static bool DragDropSource(const char* text, std::function<void(Object*& obj)> onDrag)
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

    static bool DragDropSource(const std::filesystem::path& path)
    {
        std::string asString = path.string();
        if (asString.size() > 1024)
            return false;

        bool isValid = false;
        if (ImGui::BeginDragDropSource())
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

    static bool DragDropSource(const std::filesystem::path& path, std::function<void(Object*& obj)> onDrag)
    {
        std::string asString = path.string();
        if (asString.size() > 1024)
            return false;

        bool isValid = false;

        if (ImGui::BeginDragDropSource())
        {
            DragDrop d;
            Object* payload = nullptr;
            if (onDrag)
            {
                onDrag(payload);
                if (payload == nullptr)
                {
                    ImGui::EndDragDropSource();
                    return false;
                }
                d.type = &typeid(*payload);
            }

            d.objectPayload = payload;
            strcpy(d.pathString, asString.data());
            d.tags = DragDropTag::Path | DragDropTag::Object;
            ImGui::SetDragDropPayload(PayloadType, &d, sizeof(DragDrop));
            ImGui::Text("%s", asString.data());
            isValid = true;
            ImGui::EndDragDropSource();
        }

        return isValid;
    }

    static bool DragDropSource(const char* text, Object* object)
    {
        bool isValid = false;
        if (ImGui::BeginDragDropSource())
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

    static bool DragDropTarget(std::filesystem::path& path, ImRect rect = {{0, 0}, {0, 0}})
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

    static void Image(Gfx::Image& image, const float2& minPos, const float2 maxPos)
    {
        ImGui::GetWindowDrawList()->AddImage(&image.GetDefaultImageView(), minPos, maxPos);
    }

    static bool InputText(const char* label, std::string& text, const char* hind = nullptr)
    {
        if (textArea.size() < text.size() + 1)
        {
            textArea.resize((text.size() + 1) * 2);
        }

        std::strcpy(textArea.data(), text.data());
        if (hind != nullptr)
        {
            if (ImGui::InputTextWithHint(label, hind, textArea.data(), textArea.size()))
            {
                text = textArea.data();
                return true;
            }
        }

        else
        {
            if (ImGui::InputText(label, textArea.data(), textArea.size()))
            {
                text = textArea.data();
                return true;
            }
        }

        return false;
    }

    static void AutoObjectInspector(const Object* target);
    static void AutoObjectInspector(Object* target, bool readOnly = false);
    static bool JsonInspector(nlohmann::json& j);
    static void JsonInspector(nlohmann::json& j, bool& valueChanged);

private:
    static const char* PayloadType;
    static DynamicArray<char> textArea;

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
                bool validType = true;
                if (type != nullptr)
                {
                    validType = *dragDrop->type == *type;
                }
                if (HasFlag(dragDrop->tags, DragDropTag::Object) && validType)
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
