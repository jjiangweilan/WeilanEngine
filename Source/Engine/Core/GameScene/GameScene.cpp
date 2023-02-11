#include "GameScene.hpp"
namespace Engine
{
GameScene::GameScene() : AssetObject()
{
    name = "New GameScene";
    SERIALIZE_MEMBER(gameObjects);
    SERIALIZE_MEMBER(externalGameObjects);
}

RefPtr<GameObject> GameScene::CreateGameObject()
{
    UniPtr<GameObject> newObj = MakeUnique<GameObject>(this);
    gameObjects.push_back(std::move(newObj));
    RefPtr<GameObject> refObj = gameObjects.back();
    roots.push_back(refObj);
    return refObj;
}

void GameScene::AddGameObject(GameObject* newGameObject)
{
    newGameObject->SetGameScene(this);
    roots.push_back(newGameObject);
    externalGameObjects.push_back(newGameObject);
}

const std::vector<RefPtr<GameObject>>& GameScene::GetRootObjects() { return roots; }

void GameScene::Tick()
{
    for (auto obj : roots)
    {
        TickGameObject(obj);
    }
}

void GameScene::MoveGameObjectToRoot(RefPtr<GameObject> obj) { roots.push_back(obj); }

void GameScene::RemoveGameObjectFromRoot(RefPtr<GameObject> obj)
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

void GameScene::TickGameObject(RefPtr<GameObject> obj)
{
    obj->Tick();

    for (auto child : obj->GetTransform()->GetChildren())
    {
        TickGameObject(child->GetGameObject());
    }
}

void GetLights(RefPtr<Transform> tsm, std::vector<RefPtr<Light>>& lights)
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

std::vector<RefPtr<Light>> GameScene::GetActiveLights()
{
    std::vector<RefPtr<Light>> lights;
    for (auto child : roots)
    {
        GetLights(child->GetTransform(), lights);
    }

    return lights;
}

void GameScene::Deserialize(AssetSerializer& serializer, ReferenceResolver& refResolver)
{
    AssetObject::Deserialize(serializer, refResolver);

    // find all the root object
    for (auto& obj : gameObjects)
    {
        obj->SetGameScene(this);
        if (obj->GetTransform()->GetParent() == nullptr)
        {
            roots.push_back(obj);
        }
    }

    for (auto obj : externalGameObjects)
    {
        if (obj)
        {
            obj->SetGameScene(this);
            if (obj->GetTransform()->GetParent() == nullptr)
            {
                roots.push_back(obj);
            }
        }
    }
}

void GameScene::OnReferenceResolve(void* ptr, AssetObject* resolved)
{
    for (auto externalGameObject : externalGameObjects)
    {
        if (externalGameObject.Get() == resolved)
        {
            if (auto go = static_cast<GameObject*>(externalGameObject.Get()))
            {
                go->SetGameScene(this);
                Transform* goParent = go->GetTransform()->GetParent().Get();
                if (!goParent)
                {
                    roots.push_back(go);
                }
            }
        }
    }
}

// const UUID& World::Serialize(RefPtr<Serializer> serializer)
// {
//     auto& infoJson = serializer->GetInfo();
//     auto& gameObjectsJson = infoJson["members"]["gameObjects"];

//     for(auto& obj : gameObjects)
//     {
//         UUID refID = obj->Serialize(serializer);
//         gameObjectsJson.push_back(refID.ToString());
//     }

//     auto& rootsJson = infoJson["members"]["roots"];
//     for(auto root : roots)
//     {
//         rootsJson.push_back(root->GetUUID().ToString());
//     }

//     return GetUUID();
// }

// void World::Deserialize(RefPtr<Deserializer> deserializer, RefPtr<AssetDatabase> assetDatabase)
// {
//     auto& infoJson = deserializer->GetInfo();
//     auto& gameObjectsJson = infoJson["members"]["gameObjects"];
//     auto& rootsJson = infoJson["members"]["roots"];
// }
} // namespace Engine
