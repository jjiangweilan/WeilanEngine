#include <string>
#include <string_view>
#include "Libs/DynamicArray.hpp"

namespace Editor
{
class HudDebug
{
public:
    static void Print(std::string_view log);
    DynamicArray<std::string>&& FlushLogs()
    {
        return std::move(logs);
    }
    static HudDebug& Singleton();

private:
    DynamicArray<std::string> logs;
};
} // namespace Editor
  //

#if ENGINE_EDITOR
#define HUD_DEBUG_PRINT(Log) Editor::HudDebug::Singleton().Print(Log);
#else
#define HUD_DEBUG_PRINT(Log)
#endif
