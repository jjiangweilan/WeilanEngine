#pragma once

#include "../Inspector.hpp"
namespace Engine::Editor
{
void InitializeBuiltInInspector();

#define DECLARE_INSPECTOR(Type)                                                                                        \
    class Type##Inspector : public Inspector                                                                           \
    {                                                                                                                  \
    public:                                                                                                            \
        Type##Inspector(RefPtr<AssetObject> target) : Inspector(target){};                                             \
        virtual void Tick(RefPtr<EditorContext> editorContext) override;                                               \
                                                                                                                       \
    private:                                                                                                           \
    };
DECLARE_INSPECTOR(AssetObject);

DECLARE_INSPECTOR(Material);
DECLARE_INSPECTOR(Shader);
DECLARE_INSPECTOR(GameScene);
DECLARE_INSPECTOR(RenderPipelineAsset);

DECLARE_INSPECTOR(GameObject);
DECLARE_INSPECTOR(LuaScript);
DECLARE_INSPECTOR(MeshRenderer);
DECLARE_INSPECTOR(Transform);
DECLARE_INSPECTOR(Texture);
DECLARE_INSPECTOR(Model);
} // namespace Engine::Editor
