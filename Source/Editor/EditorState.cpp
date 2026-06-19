#include "Editor/EditorState.hpp"

namespace Editor
{
namespace
{
constexpr size_t MaxSelectionHistory = 32;
bool recordSelectionHistory = true;
} // namespace

GameLoop*& EditorState::GetGameLoop()
{
    static GameLoop* gameLoop = nullptr;
    return gameLoop;
}

UndoManager& EditorState::GetUndoManager()
{
    static UndoManager undoManager;
    return undoManager;
}

bool& EditorState::GetScaleLock()
{
    static bool scaleLock = false;
    return scaleLock;
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
    ObjPtr<Object> previousMain = GetMainSelectedObject();
    Object* ptr = obj.Get();
    auto& selectedObjects = StaticGetSelectedObjects();
    if (obj == nullptr)
    {
        selectedObjects.clear();
        if (recordSelectionHistory)
            PushSelectionHistory(previousMain);
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

    Object* newMain = GetMainSelectedObject();
    if (recordSelectionHistory && previousMain != nullptr && previousMain.Get() != newMain)
        PushSelectionHistory(previousMain);
}

Object* EditorState::GetMainSelectedObject()
{
    if (!StaticGetSelectedObjects().empty())
        return StaticGetSelectedObjects()[0].Get();

    return nullptr;
}

bool EditorState::CanSelectPreviousObject()
{
    Object* current = GetMainSelectedObject();
    auto& history = StaticGetSelectionHistory();
    return std::any_of(
        history.begin(),
        history.end(),
        [current](const ObjPtr<Object>& obj)
        { return obj != nullptr && obj.Get() != current; }
    );
}

void EditorState::SelectPreviousObject()
{
    Object* current = GetMainSelectedObject();
    auto& history = StaticGetSelectionHistory();
    while (!history.empty())
    {
        ObjPtr<Object> previous = history.back();
        history.pop_back();
        if (previous == nullptr || previous.Get() == current)
            continue;

        recordSelectionHistory = false;
        SelectObject(previous);
        recordSelectionHistory = true;
        return;
    }
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

std::vector<ObjPtr<Object>>& EditorState::StaticGetSelectionHistory()
{
    static std::vector<ObjPtr<Object>> selectionHistoryStatic{};
    return selectionHistoryStatic;
}

void EditorState::PushSelectionHistory(ObjPtr<Object> obj)
{
    if (obj == nullptr)
        return;

    auto& history = StaticGetSelectionHistory();
    if (!history.empty() && history.back().Get() == obj.Get())
        return;

    history.push_back(obj);
    if (history.size() > MaxSelectionHistory)
        history.erase(history.begin());
}

} // namespace Editor
