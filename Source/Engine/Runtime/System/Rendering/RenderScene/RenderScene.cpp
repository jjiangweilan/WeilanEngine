#include "RenderScene.hpp"
#include "Engine/Library/Hive.hpp"
#include "RenderSceneBVH.hpp"

class RenderSceneImpl
{
public:
    StaticMeshInstanceHandle CreateStaticMeshInstance()
    {
        auto iter = staticMeshes.insert(StaticMeshInstance{});
        return StaticMeshInstanceHandle{&*iter, this};
    }

    RenderCamera CreateRenderCamera()
    {
    }

private:
    plf::hive<StaticMeshInstance> staticMeshes;

    struct
    {
        RenderSceneBVH bvh;
        bool rebuild = true;
    } sceneBvh;
};

StaticMeshInstanceHandle RenderScene::CreateStaticMeshInstance()
{
    return impl->CreateStaticMeshInstance();
}

RenderCamera RenderScene::CreateRenderCamera()
{
    return impl->CreateRenderCamera();
}
