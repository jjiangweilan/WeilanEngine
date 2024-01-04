#include "SceneEnvironment.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Texture.hpp"

DEFINE_OBJECT(SceneEnvironment, "7E237F73-396E-455F-B227-657432A66877");

const std::string& SceneEnvironment::GetName()
{
    static std::string name = "SceneEnvironment";
    return name;
}

void SceneEnvironment::EnableImple()
{
    auto scene = GetScene();

    if (scene)
    {
        auto& renderingScene = scene->GetRenderingScene();
        renderingScene.SetSceneEnvironment(*this);
    }
}
void SceneEnvironment::DisableImple()
{
    auto scene = GetScene();

    if (scene)
    {
        auto& renderingScene = scene->GetRenderingScene();
        renderingScene.RemoveSceneEnvironment(*this);
    }
}

void SceneEnvironment::SetDiffuseCube(Texture* diffuseCube)
{
    if (diffuseCube && diffuseCube->GetDescription().img.isCubemap)
        this->diffuseCube = diffuseCube;
    else if (diffuseCube == nullptr)
        this->diffuseCube = nullptr;
}

void SceneEnvironment::SetSpecularCube(Texture* specularCube)
{
    if (specularCube && specularCube->GetDescription().img.isCubemap)
        this->specularCube = specularCube;
    else if (specularCube == nullptr)
        this->specularCube = nullptr;
}

void SceneEnvironment::Serialize(Serializer* s) const
{
    s->Serialize("diffuseCube", diffuseCube);
    s->Serialize("specularCube", specularCube);
}

void SceneEnvironment::Deserialize(Serializer* s)
{
    s->Deserialize("diffuseCube", diffuseCube);
    s->Deserialize("specularCube", specularCube);
}
