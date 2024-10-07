#pragma once
#include "GfxDriver/Image.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <vector>

class MeshRenderer;
class SceneEnvironment;
class Terrain;
class GrassSurface;
class Cloud;
class RenderingScene
{
public:
    RenderingScene() {};
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

    void SetTerrain(Terrain& terrain)
    {
        this->terrain = &terrain;
    }

    void RemoveTerrain(Terrain& terrain)
    {
        if (this->terrain == &terrain)
        {
            this->terrain = nullptr;
        }
    }

    Terrain* GetTerrain()
    {
        return terrain;
    }

    void AddRenderer(MeshRenderer& renderingObject)
    {
        meshRenderers.push_back(&renderingObject);
    }

    void AddRenderer(Cloud& renderingObject)
    {
        clouds.push_back(&renderingObject);
    }

    void AddGrassSurface(GrassSurface& grassSurface)
    {
        grassSurfaces.push_back(&grassSurface);
    }

    void RemoveGrassSurface(GrassSurface& grassSurface)
    {
        auto iter = std::find(grassSurfaces.begin(), grassSurfaces.end(), &grassSurface);
        if (iter != grassSurfaces.end())
        {
            std::swap(*iter, grassSurfaces.back());
            grassSurfaces.pop_back();
        }
    }

    void RemoveRenderer(MeshRenderer& renderingObject)
    {
        auto iter = std::find(meshRenderers.begin(), meshRenderers.end(), &renderingObject);
        if (iter != meshRenderers.end())
        {
            std::swap(*iter, meshRenderers.back());
            meshRenderers.pop_back();
        }
    }

    void RemoveRenderer(Cloud& renderingObject)
    {
        auto iter = std::find(clouds.begin(), clouds.end(), &renderingObject);
        if (iter != clouds.end())
        {
            std::swap(*iter, clouds.back());
            clouds.pop_back();
        }
    }

    std::span<GrassSurface*> GetGrassSurface()
    {
        return grassSurfaces;
    }

    std::span<MeshRenderer*> GetMeshRenderers()
    {
        return meshRenderers;
    }

    const std::vector<Cloud*>& GetClouds()
    {
        return clouds;
    }

    SceneEnvironment* GetSceneEnvironment()
    {
        return sceneEnvironment;
    }

private:
    std::vector<MeshRenderer*> meshRenderers;
    std::vector<GrassSurface*> grassSurfaces;
    std::vector<Cloud*> clouds;
    SceneEnvironment* sceneEnvironment = nullptr;
    Terrain* terrain = nullptr;
};
