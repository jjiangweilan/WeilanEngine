#include "MainMenuModule.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
static void SplitPath(std::string_view path, DynamicArray<std::string>& pathComponents)
{
    std::string_view::size_type pos = 0;
    std::string_view::size_type prev = 0;
    while ((pos = path.find_first_of('/', prev)) != std::string_view::npos)
    {
        pathComponents.push_back(std::string(path.substr(prev, pos - prev)));
        prev = pos + 1;
    }
    pathComponents.push_back(std::string(path.substr(prev)));
}

bool MainMenuModule::RegisterMenuItem(std::string_view path, const std::function<void()>& f)
{
    DynamicArray<std::string> pathComponents;
    SplitPath(path, pathComponents);
    items.push_back({std::string(path), pathComponents, f});
    return true;
}
MainMenuModule& MainMenuModule::GetSingleton()
{
    static MainMenuModule singleton;
    return singleton;
}

static void MenuVisitor(DynamicArray<std::string>::iterator iter, DynamicArray<std::string>::iterator end, bool& clicked)
{
    if (iter == end)
    {
        return;
    }
    else if (iter == end - 1)
    {
        if (ImGui::MenuItem(iter->c_str()))
        {
            clicked = true;
        }
        return;
    }

    if (ImGui::BeginMenu(iter->c_str()))
    {
        iter += 1;
        MenuVisitor(iter, end, clicked);
        ImGui::EndMenu();
    }
}

void MainMenuModule::IterateThroughAllMenuItems()
{

    for (auto& item : items)
    {
        auto& comps = item.pathComponents;
        bool clicked = false;
        MenuVisitor(comps.begin(), comps.end(), clicked);
        if (clicked)
        {
            item.func();
        }
    }
}
} // namespace Editor
