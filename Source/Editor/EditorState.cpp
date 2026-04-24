#include "Editor/EditorState.hpp"

namespace Editor
{
GameLoop*& EditorState::GetGameLoop()
{
    static GameLoop* gameLoop = nullptr;
    return gameLoop;
}
void EditorState::DeselectObject(Object* obj)
{
    if (obj == nullptr)
        return;

    auto findIter = std::find_if(
        StaticGetSelectedObjects().begin(),
        StaticGetSelectedObjects().end(),
        [obj](ObjPtr<Object> o)
        { return o.Get() == obj; }
    );

    if (findIter != StaticGetSelectedObjects().end())
    {
        StaticGetSelectedObjects().erase(findIter);
    }
}

void EditorState::SelectObject(ObjPtr<Object> obj, bool multiSelect)
{
    Object* ptr = obj.Get();
    auto& selectedObjects = StaticGetSelectedObjects();
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
            [ptr](ObjPtr<Object> o)
            { return o.Get() == ptr; }
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
    if (!StaticGetSelectedObjects().empty())
        return StaticGetSelectedObjects()[0].Get();

    return nullptr;
}

std::vector<ObjPtr<Object>> EditorState::GetSelectedObjects()
{
    return StaticGetSelectedObjects();
}

std::vector<ObjPtr<Object>>& EditorState::StaticGetSelectedObjects()
{
    static std::vector<ObjPtr<Object>> selectedObjectsStatic{};
    return selectedObjectsStatic;
}

} // namespace Editor
