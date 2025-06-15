#pragma once
#include "Libs/DynamicArray.hpp"
#include <functional>
#include <string>
#include <string_view>
namespace Editor
{
class MainMenuModule
{
public:
    static MainMenuModule& GetSingleton();
    bool RegisterMenuItem(std::string_view path, const std::function<void()>& f);

    void IterateThroughAllMenuItems();

private:
    MainMenuModule() = default;
    struct RegisteredMenuItem
    {
        std::string path;
        DynamicArray<std::string> pathComponents;
        std::function<void()> func;
    };
    DynamicArray<RegisteredMenuItem> items;
};

} // namespace Editor

#define REGISTER_MAIN_MENU_ITEM(path)                                                                                  \
    static void MenuClickCallback();                                                                                   \
    static bool _EDITOR_MENU_REGISTERED =                                                                              \
        Editor::MainMenuModule::GetSingleton().RegisterMenuItem(path, MenuClickCallback);                              \
    void MenuClickCallback()
