#include "Scene.hpp"
DEFINE_ASSET(Scene, "BE42FB0F-42FF-4951-8D7D-DBD28439D3E7", "scene");

Scene::Scene() : Asset(), renderingScene(), physicsScene(this)
{
    name = "New GameScene";
}

Scene::~Scene()
{
    gameObjects.clear();
}

GameObject* Scene::CreateGameObject()
{
    std::unique_ptr<GameObject> newObj = std::make_unique<GameObject>(this);
    gameObjects.push_back(std::move(newObj));
    GameObject* refObj = gameObjects.back().get();
    roots.push_back(refObj);
    refObj->SetEnable(true);
    return refObj;
}

GameObject* Scene::AddGameObject(std::unique_ptr<GameObject>&& newGameObject)
{
    GameObject* temp = newGameObject.get();
    gameObjects.push_back(std::move(newGameObject));

    temp->SetScene(this);

    if (temp->GetParent() == nullptr)
    {
        roots.push_back(temp);
    }

    if (temp->GetWantsTobeEnabledStateAndReset())
    {
        temp->SetEnable(true);
    }

    for (auto& owningChild : temp->GetOwningChildren())
    {
        AddGameObject(std::move(owningChild));
    }

    return temp;
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

void Scene::PrePhysicsTick()
{
    for (auto obj : roots)
    {
        PrePhysicsTickGameObject(obj);
    }
}

void Scene::MoveGameObjectToRoot(GameObject* obj)
{
    roots.push_back(obj);
}

static void GetAllGameObjects(GameObject* current, std::vector<GameObject*>& objs)
{
    objs.push_back(current);
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
        ::GetAllGameObjects(obj, objs);
    }

    return objs;
}

GameObject* Scene::CopyGameObject(GameObject& gameObject)
{
    auto newObj = std::make_unique<GameObject>(gameObject);
    newObj->SetScene(this);

    GameObject* top = newObj.get();
    gameObjects.push_back(std::move(newObj));

    if (gameObject.GetParent() == nullptr)
    {
        roots.push_back(top);
    }
    else
    {
        top->SetParent(gameObject.GetParent());
    }

    if (top->GetWantsTobeEnabledStateAndReset())
    {
        top->SetEnable(true);
    }

    return top;
}

void Scene::DestroyGameObject(GameObject* obj)
{
    if (obj == nullptr)
        return;

    for (auto child : obj->GetChildren())
    {
        DestroyGameObject(child);
    }

    obj->SetEnable(false);

    if (GameObject* parent = obj->GetParent())
    {
        parent->RemoveChild(obj);
    }

    auto rootIter = std::find(roots.begin(), roots.end(), obj);
    if (rootIter != roots.end())
    {
        roots.erase(rootIter);
    }

    auto iter = std::find_if(gameObjects.begin(), gameObjects.end(), [obj](auto& o) { return o.get() == obj; });
    if (iter != gameObjects.end())
        gameObjects.erase(iter);
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

    for (auto child : obj->GetChildren())
    {
        TickGameObject(child);
    }
}

void Scene::PrePhysicsTickGameObject(GameObject* obj)
{
    obj->PrePhysicsTick();

    for (auto child : obj->GetChildren())
    {
        PrePhysicsTickGameObject(child);
    }
}

void GetLights(GameObject* go, std::vector<Light*>& lights)
{
    for (auto& child : go->GetChildren())
    {
        GetLights(child, lights);
    }

    auto light = go->GetComponent<Light>();
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
        GetLights(child, lights);
    }

    return lights;
}

void Scene::AddGameObjects(std::vector<std::unique_ptr<GameObject>>&& gameObjects)
{
    for (auto&& v : gameObjects)
    {
        AddGameObject(std::move(v));
    }
    gameObjects.clear();
}

void Scene::Serialize(Serializer* s) const
{
    s->Serialize("gameObjects", gameObjects);
    s->Serialize("roots", roots);
    s->Serialize("camera", camera);
}

std::unique_ptr<Asset> Scene::Clone()
{
    std::unique_ptr<Scene> copy = std::make_unique<Scene>();

    for (GameObject* root : roots)
    {
        std::unique_ptr<GameObject> go = std::unique_ptr<GameObject>(static_cast<GameObject*>(root->Clone().release()));
        copy->AddGameObject(std::move(go));
    }

    return copy;
}

void Scene::Deserialize(Serializer* s)
{
    s->Deserialize("gameObjects", gameObjects);
    s->Deserialize("roots", roots);
    s->Deserialize("camera", camera);
}

void Scene::OnLoadingFinished()
{
    for (auto go : GetAllGameObjects())
    {
        go->OnLoadingFinished();
    }

    for (auto& g : gameObjects)
    {
        g->SetScene(this);
    }
}
