#include "Editor/HudDebug.hpp"
#include <ranges>

namespace Editor
{
void HudDebug::Print(std::string_view log)
{
    Singleton().logs.push_back(std::string(log));
}

HudDebug& HudDebug::Singleton()
{
    static HudDebug d;
    return d;
}
} // namespace Editor
