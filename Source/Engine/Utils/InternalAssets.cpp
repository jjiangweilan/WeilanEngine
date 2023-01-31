#include "InternalAssets.hpp"

#if GAME_EDITOR
#include "Editor/ProjectManagement/ProjectManagement.hpp"
#endif

namespace Engine::Utils
{
std::filesystem::path InternalAssets::GetInternalAssetPath()
{
#if GAME_EDITOR
    return Editor::GetSysConfigPath();
#endif
}
} // namespace Engine::Utils
