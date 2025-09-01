#include "EditorState.hpp"

namespace Editor
{
std::vector<ObjPtr<Object>> EditorState::selectedObjects = {};
GameLoop* EditorState::gameLoop = nullptr;

void EditorState::DeselectObject(Object* obj)
{
    if (obj == nullptr)
        return;

    auto findIter = std::find_if(
        selectedObjects.begin(),
        selectedObjects.end(),
        [obj](ObjPtr<Object> o) { return o.Get() == obj; }
    );

    if (findIter != selectedObjects.end())
    {
        selectedObjects.erase(findIter);
    }
}

void EditorState::SelectObject(ObjPtr<Object> obj, bool multiSelect)
{
    Object* ptr = obj.Get();
    if (obj == nullptr)
    {
        selectedObjects.clear();
        return;
    }

    if (multiSelect && !selectedObjects.empty() && typeid(*ptr) != typeid(*selectedObjects[0].Get()))
    {
        return;
    }

    if (multiSelect)
    {
        auto findIter = std::find_if(
            selectedObjects.begin(),
            selectedObjects.end(),
            [ptr](ObjPtr<Object> o) { return o.Get() == ptr; }
        );
        if (findIter == selectedObjects.end())
        {
            selectedObjects.push_back(obj);
        }
    }
    else
    {
        selectedObjects.clear();
        selectedObjects.push_back(obj);
    }
}

Object* EditorState::GetMainSelectedObject()
{
    if (!selectedObjects.empty())
        return selectedObjects[0].Get();

    return nullptr;
}

std::vector<ObjPtr<Object>> EditorState::GetSelectedObjects()
{
    return selectedObjects;
}
} // namespace Editor
