#pragma once

#include <algorithm>
#include <vector>

class MeshRenderer;
class RenderingScene
{
public:
    RenderingScene(){};
    void AddRenderer(MeshRenderer& meshRenderer)
    {
        meshRenderers.push_back(&meshRenderer);
    }
    void RemoveRenderer(MeshRenderer& meshRenderer)
    {
        auto iter = std::find(meshRenderers.begin(), meshRenderers.end(), &meshRenderer);
        std::swap(*iter, meshRenderers.back());
        meshRenderers.pop_back();
    };

    const std::vector<MeshRenderer*>& GetRenderers()
    {
        return meshRenderers;
    }

private:
    std::vector<MeshRenderer*> meshRenderers;
};
