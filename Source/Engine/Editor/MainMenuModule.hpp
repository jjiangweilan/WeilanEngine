#pragma once
#include <functional>
#include <string_view>
#include <vector>

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
        std::vector<std::string> pathComponents;
        std::function<void()> func;
    };
    std::vector<RegisteredMenuItem> items;
};

} // namespace Editor

#define REGISTER_MAIN_MENU_ITEM(path)                                                                                  \
    static void MenuClickCallback();                                                                                   \
    static bool _EDITOR_MENU_REGISTERED =                                                                              \
        Editor::MainMenuModule::GetSingleton().RegisterMenuItem(path, MenuClickCallback);                              \
    void MenuClickCallback()
