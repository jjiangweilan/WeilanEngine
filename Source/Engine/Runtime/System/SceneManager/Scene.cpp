#include "Scene.hpp"
DEFINE_ASSET(Scene, "BE42FB0F-42FF-4951-8D7D-DBD28439D3E7", "scene");

Scene::Scene() : Asset(), renderingScene(), physicsScene(this)
{
    name = "New GameScene";
    renderingScene.scene = this;
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

const std::vector<ObjPtr<GameObject>>& Scene::GetRootObjects()
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
        if (child)
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
    auto&& children = newObj->GetOwningChildren();
    gameObjects.insert(gameObjects.end(), std::move_iterator(children.begin()), std::move_iterator(children.end()));
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
        DestroyGameObjectNestedCall(child);
    }

    obj->SetEnable(false);

    if (GameObject* parent = obj->GetParent())
    {
        parent->RemoveChild(obj);
    }

    auto rootIter = std::find_if(roots.begin(), roots.end(), [&](ObjPtr<GameObject>& f)
                                 { return f.Get() == obj; });
    if (rootIter != roots.end())
    {
        roots.erase(rootIter);
    }

    auto iter = std::find_if(gameObjects.begin(), gameObjects.end(), [obj](auto& o)
                             { return o.get() == obj; });
    if (iter != gameObjects.end())
        gameObjects.erase(iter);
}

void Scene::DestroyGameObjectNestedCall(GameObject* obj)
{
    if (obj == nullptr)
        return;

    for (auto child : obj->GetChildren())
    {
        DestroyGameObjectNestedCall(child);
    }

    obj->SetEnable(false);

    auto iter = std::find_if(gameObjects.begin(), gameObjects.end(), [obj](auto& o)
                             { return o.get() == obj; });
    if (iter != gameObjects.end())
        gameObjects.erase(iter);
}

void Scene::RemoveGameObjectFromRoot(GameObject* obj)
{
    auto it = roots.begin();
    while (it != roots.end())
    {
        if ((*it).Get() == obj)
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
    if (obj->IsEnabled())
    {
        obj->PrePhysicsTick();

        for (auto child : obj->GetChildren())
        {
            PrePhysicsTickGameObject(child);
        }
    }
}

void GetLights(GameObject* go, std::vector<Light*>& lights)
{
    if (go == nullptr)
        return;
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
    s->Serialize(
        "gameObjects",
        gameObjects
    );
    s->Serialize("roots", roots);
    s->Serialize("camera", camera);
    s->Serialize("renderPipelineSetting", renderPipelineSetting);
}

std::unique_ptr<Asset> Scene::Clone()
{
    std::unique_ptr<Scene> copy = std::make_unique<Scene>();

    for (GameObject* root : roots)
    {
        std::unique_ptr<GameObject> go = std::make_unique<GameObject>(*root);
        copy->AddGameObject(std::move(go));
    }

    return copy;
}

void Scene::Deserialize(Serializer* s)
{
    s->Deserialize("gameObjects", gameObjects);
    s->Deserialize("roots", roots);
    s->Deserialize("camera", camera);
    s->Deserialize("renderPipelineSetting", renderPipelineSetting);
}

void Scene::OnLoaded()
{
    for (auto go : GetAllGameObjects())
    {
        if (go != nullptr)
            go->OnLoaded();
    }

    for (auto& g : gameObjects)
    {
        if (g != nullptr)
            g->SetScene(this);
    }
}

std::unique_ptr<GameObject> Scene::RetrieveGameObject(GameObject* obj)
{
    std::unique_ptr<GameObject> target = nullptr;
    for (auto& g : gameObjects)
    {
        if (g.get() == obj)
        {
            std::swap(g, gameObjects.back());
            target = std::move(gameObjects.back());
            gameObjects.pop_back();
            break;
        }
    }

    return target;
}

void Scene::FixUndestroiedGameObjectNotInSceneTree()
{

    auto gos = GetAllGameObjects();

    bool nextErase = true;
    while (nextErase)
    {
        auto& gs = GetGameObjects();
        for (int i = 0; i < gs.size(); ++i)
        {
            auto& g = gs[i];
            auto iter = std::find(gos.begin(), gos.end(), g.get());
            if (iter == gos.end())
            {
                nextErase = true;
                gs.erase(gs.begin() + i);
                break;
            }
            else
            {
                nextErase = false;
            }
        }
    }
}
