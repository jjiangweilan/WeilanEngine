#pragma once
#include "Core/Texture.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <vector>

class MeshRenderer;

struct EnvironmentLighting
{
    glm::vec4 sh[9];
    Texture* specularEnv;
};
class RenderingScene
{
public:
    RenderingScene(){};

    void SetEnvironmentLighting(EnvironmentLighting environmentLighting);

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
