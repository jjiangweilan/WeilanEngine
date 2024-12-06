#include "RenderingScene.hpp"
#include "Core/Component/MeshRenderer.hpp"

void RenderingScene::Tick()
{
    for (auto m : meshRenderers)
    {
        m->UpdateSkinning();
    }
}
