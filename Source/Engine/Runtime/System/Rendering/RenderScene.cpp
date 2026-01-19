#include "RenderScene.hpp"
#include "RenderScene/RenderSceneImpl.hpp"

StaticMeshRenderer RenderScene::CreateStaticMeshRenderer()
{
    return impl->CreateStaticMeshRenderer();
}

    void RenderScene::DestroyStaticMeshRenderer(StaticMeshRenderer& renderer)
{

}

RenderCamera RenderScene::CreateRenderCamera()
{
    return impl->CreateRenderCamera();
}
