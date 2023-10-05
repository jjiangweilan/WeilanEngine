#include "Scene.hpp"
namespace Engine
{
DEFINE_ASSET(Scene, "BE42FB0F-42FF-4951-8D7D-DBD28439D3E7", "scene");

Scene::Scene() : Asset(), systemEventCallbacks()
{
    name = "New GameScene";
}

GameObject* Scene::CreateGameObject()
{
    std::unique_ptr<GameObject> newObj = std::make_unique<GameObject>(this);
    gameObjects.push_back(std::move(newObj));
    GameObject* refObj = gameObjects.back().get();
    roots.push_back(refObj);
    return refObj;
}

void Scene::AddGameObject(GameObject* newGameObject)
{
    newGameObject->SetGameScene(this);
    roots.push_back(newGameObject);
    externalGameObjects.push_back(newGameObject);
}

const std::vector<GameObject*>& Scene::GetRootObjects()
{
    return roots;
}

void Scene::Tick()
{
    for (auto obj : roots)
    {
        TickGameObject(obj);
    }
}

void Scene::MoveGameObjectToRoot(GameObject* obj)
{
    roots.push_back(obj);
}

static void GetAllGameObjects(Transform* current, std::vector<GameObject*>& objs)
{
    objs.push_back(current->GetGameObject());
    for (auto& child : current->GetChildren())
    {
        GetAllGameObjects(child, objs);
    }
}

std::vector<GameObject*> Scene::GetAllGameObjects()
{
    std::vector<GameObject*> objs;
    objs.reserve(256);

    for (auto& obj : roots)
    {
        ::Engine::GetAllGameObjects(obj->GetTransform(), objs);
    }

    return objs;
}

void Scene::RemoveGameObjectFromRoot(GameObject* obj)
{
    auto it = roots.begin();
    while (it != roots.end())
    {
        if (*it == obj)
        {
            roots.erase(it);
            return;
        }
        it += 1;
    }
}

void Scene::TickGameObject(GameObject* obj)
{
    obj->Tick();

    for (auto child : obj->GetTransform()->GetChildren())
    {
        TickGameObject(child->GetGameObject());
    }
}

void GetLights(Transform* tsm, std::vector<Light*>& lights)
{
    for (auto& child : tsm->GetChildren())
    {
        GetLights(child, lights);
    }

    auto light = tsm->GetGameObject()->GetComponent<Light>();
    if (light != nullptr)
    {
        lights.push_back(light);
    }
}

std::vector<Light*> Scene::GetActiveLights()
{
    std::vector<Light*> lights;
    for (auto child : roots)
    {
        GetLights(child->GetTransform(), lights);
    }

    return lights;
}

void Scene::Serialize(Serializer* s) const
{
    s->Serialize("gameObjects", gameObjects);
    s->Serialize("externalGameObjects", externalGameObjects);
    s->Serialize("roots", roots);
    s->Serialize("camera", camera);
}

void Scene::Deserialize(Serializer* s)
{
    s->Deserialize("gameObjects", gameObjects);
    s->Deserialize("externalGameObjects", externalGameObjects);
    s->Deserialize("roots", roots);
    s->Deserialize("camera", camera);

    //// find all the root object
    // for (auto& obj : gameObjects)
    //{
    //     obj->SetGameScene(this);
    //     if (obj->GetTransform()->GetParent() == nullptr)
    //     {
    //         roots.push_back(obj);
    //     }
    // }

    // for (auto obj : externalGameObjects)
    //{
    //     if (obj != nullptr)
    //     {
    //         obj->SetGameScene(this);
    //         if (obj->GetTransform()->GetParent() == nullptr)
    //         {
    //             roots.push_back(obj);
    //         }
    //     }
    // }
}

} // namespace Engine
