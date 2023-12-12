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

void Scene::AddGameObject(std::unique_ptr<GameObject>&& newGameObject)
{
    GameObject* temp = newGameObject.get();
    gameObjects.push_back(std::move(newGameObject));
    if (newGameObject->GetTransform()->GetParent() == nullptr)
    {
        roots.push_back(temp);
    }
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

GameObject* Scene::CopyGameObject(GameObject& gameObject)
{
    auto newObj = std::make_unique<GameObject>(gameObject);
    newObj->SetGameScene(this);

    for (auto tsm : gameObject.GetTransform()->GetChildren())
    {
        GameObject* child = CopyGameObject(*tsm->GetGameObject());
        child->GetTransform()->SetParent(newObj->GetTransform());
    }

    GameObject* top = newObj.get();
    gameObjects.push_back(std::move(newObj));

    if (gameObject.GetTransform()->GetParent() == nullptr)
    {
        roots.push_back(top);
    }

    return top;
}

void Scene::DestroyGameObject(GameObject* obj)
{
    auto iter = std::find_if(gameObjects.begin(), gameObjects.end(), [obj](auto& o) { return o.get() == obj; });
    if (iter != gameObjects.end())
    {
        for (auto child : obj->GetTransform()->GetChildren())
        {
            DestroyGameObject(obj);
        }

        if (Transform* parent = obj->GetTransform()->GetParent())
        {
            parent->RemoveChild(obj->GetTransform());
            gameObjects.erase(iter);
        }
    }

    auto rootIter = std::find(roots.begin(), roots.end(), obj);
    if (rootIter != roots.end())
    {
        roots.erase(rootIter);
    }

    auto externalIter = std::find(externalGameObjects.begin(), externalGameObjects.end(), obj);
    if (externalIter != externalGameObjects.end())
    {
        externalGameObjects.erase(rootIter);
    }
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

void Scene::AddGameObjects(std::vector<std::unique_ptr<GameObject>>&& gameObjects)
{
    for (auto& go : gameObjects)
    {
        go->SetGameScene(this);

        if (go->GetTransform()->GetParent() == nullptr)
        {
            MoveGameObjectToRoot(go.get());
        }
    }

    this->gameObjects.insert(
        this->gameObjects.end(),
        std::make_move_iterator(gameObjects.begin()),
        std::make_move_iterator(gameObjects.end())
    );

    gameObjects.clear();
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
