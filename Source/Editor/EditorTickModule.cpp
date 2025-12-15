#include "EditorTickModule.hpp"

namespace Editor
{

EditorTickModule& EditorTickModule::GetSingleton()
{
    static EditorTickModule singleton;
    return singleton;
}

bool EditorTickModule::AddTick(const std::function<void(bool& disable)>& f)
{
    registeredTick.push_back(f);
    return true;
}

void EditorTickModule::TickAll()
{
    std::vector<std::vector<std::function<void(bool& close)>>::iterator> toDeletes;

    for (auto iter = registeredTick.begin(); iter != registeredTick.end(); ++iter)
    {
        bool disable = false;
        (*iter)(disable);
        if (disable)
        {
            toDeletes.push_back(iter);
        }
    }

    for (auto toDelete : toDeletes)
    {
        registeredTick.erase(toDelete);
    }
}
} // namespace Editor
