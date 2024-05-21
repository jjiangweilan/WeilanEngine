#include "Window.hpp"

namespace Editor
{
WindowRegistery& WindowRegistery::GetSingleton()
{
    static WindowRegistery registry;
    return registry;
}
} // namespace Editor
