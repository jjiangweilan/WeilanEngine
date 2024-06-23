#pragma once
#include <functional>

namespace Editor
{
class EditorTickModule
{
public:
    static EditorTickModule& GetSingleton();
    bool AddTick(const std::function<void(bool& disable)>& f);
    void TickAll();

private:
    std::vector<std::function<void(bool& close)>> registeredTick;
};

} // namespace Editor

#define ADD_EDITOR_TICK()                                                                                              \
    static void EditorTickFunc(bool& disableTick);                                                                     \
    static bool _EDITOR_TICK_REGISTERED = Editor::EditorTickModule::GetSingleton().Register(EditorTickFunc);           \
    void EditorTickFunc(bool& disableTick)
