#include "EditorState.hpp"

namespace Editor
{

void SelectObject(SRef<Object> obj, bool multiSelect) {}
std::vector<SRef<Object>> EditorState::selectedObjects = {};
Scene* EditorState::activeScene = nullptr;
GameLoop* EditorState::gameLoop = nullptr;

void EditorState::DeselectObject(Object* obj)
{
    if (obj == nullptr)
        return;

    auto findIter =
        std::find_if(selectedObjects.begin(), selectedObjects.end(), [obj](SRef<Object> o) { return o.Get() == obj; });

    if (findIter != selectedObjects.end())
    {
        selectedObjects.erase(findIter);
    }
}

void EditorState::SelectObject(SRef<Object> obj, bool multiSelect)
{
    Object* ptr = obj.Get();
    if (ptr == nullptr)
        return;

    auto findIter =
        std::find_if(selectedObjects.begin(), selectedObjects.end(), [ptr](SRef<Object> o) { return o.Get() == ptr; });

    if (findIter == selectedObjects.end())
    {
        if (multiSelect)
        {
            selectedObjects.push_back(obj);
        }
        else
        {
            selectedObjects.clear();
            selectedObjects.push_back(obj);
        }
    }
}

Object* EditorState::GetMainSelectedObject()
{
    if (!selectedObjects.empty())
        return selectedObjects[0].Get();

    return nullptr;
}

std::vector<SRef<Object>> EditorState::GetSelectedObjects()
{
    return selectedObjects;
}
} // namespace Editor
