#pragma once
#include "Core/Asset.hpp"
#include "Core/GameObject.hpp"
#include "Core/Object.hpp"
#include "EditorState.hpp"
#include "Libs/EnumFlags.hpp"
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
        ImGui::Text("%s: ", name.data());
        ImGui::SameLine();
        std::string buttonName = "null";
        Asset* asset = dynamic_cast<Asset*>(curr);
        if (asset)
        {
            buttonName = asset->GetName();
        }
        else if (curr != nullptr)
        {
            buttonName = curr->GetUUID().ToString().c_str();
        }
        if (ImGui::Button(curr == nullptr ? "null" : buttonName.c_str()))
        {
            EditorState::SelectObject(curr ? curr->GetSRef() : nullptr);
        }

        Object* target = nullptr;
        if (DragDropTarget(typeid(T), target))
        {
            curr = (T*)target;
            newValue = true;
        }
        else if (DragDropTarget(typeid(GameObject), target))
        {
            GameObject* go = (GameObject*)target;
            curr = go->GetComponent<T>();
            newValue = curr != nullptr;
        }

        ImGui::PopID();

        return newValue;
    }

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
                if (HasFlag(dragDrop->tags, DragDropTag::Object))
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

    static bool DragDropTarget(const std::type_info& type, Object*& obj, ImRect rect = {{0, 0}, {0, 0}})
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
                if (HasFlag(dragDrop->tags, DragDropTag::Object) && *dragDrop->type == type)
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

private:
    static const char* PayloadType;
};
} // namespace Editor
