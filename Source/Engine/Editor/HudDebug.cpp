#include "HudDebug.hpp"

namespace Editor
{
void HudDebug::Print(std::string_view log)
{
    logs.push_back(std::string(log));
}

HudDebug& HudDebug::Singleton()
{
    static HudDebug d;
    return d;
}
} // namespace Editor
