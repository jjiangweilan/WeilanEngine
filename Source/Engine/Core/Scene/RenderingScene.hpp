#pragma once
#include "GfxDriver/Image.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <vector>

class MeshRenderer;
class SceneEnvironment;
class RenderingScene
{
public:
    RenderingScene(){};
    RenderingScene(const RenderingScene& other) = delete;
    RenderingScene(RenderingScene&& other) = delete;

    void SetSceneEnvironment(SceneEnvironment& sceneEnvironment)
    {
        if (this->sceneEnvironment == nullptr)
        {
            this->sceneEnvironment = &sceneEnvironment;
        }
    }

    void RemoveSceneEnvironment(SceneEnvironment& sceneEnvironment)
    {
        if (this->sceneEnvironment == &sceneEnvironment)
        {
            this->sceneEnvironment = nullptr;
        }
    }

    void AddRenderer(MeshRenderer& meshRenderer)
    {
        meshRenderers.push_back(&meshRenderer);
    }

    void RemoveRenderer(MeshRenderer& meshRenderer)
    {
        auto iter = std::find(meshRenderers.begin(), meshRenderers.end(), &meshRenderer);
        if (iter != meshRenderers.end())
        {
            std::swap(*iter, meshRenderers.back());
            meshRenderers.pop_back();
        }
    };

    const std::vector<MeshRenderer*>& GetRenderers()
    {
        return meshRenderers;
    }

    SceneEnvironment* GetSceneEnvironment()
    {
        return sceneEnvironment;
    }

private:
    std::vector<MeshRenderer*> meshRenderers;
    SceneEnvironment* sceneEnvironment = nullptr;
};
