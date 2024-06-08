#include "AssetDatabase/Importers.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Utils/AssetLoader.hpp"
#include "WeilanEngine.hpp"
#include "spdlog/spdlog.h"
#include <gtest/gtest.h>
using namespace Engine;
TEST(Gameplay, Test0)
{
    auto engine = std::make_unique<WeilanEngine>();
    engine->Init({});

    Scene scene;

    // set camera
    GameObject* gameObject = scene.CreateGameObject();
    gameObject->SetName("Camera");
    auto cam = gameObject->AddComponent<Camera>();
    scene.SetMainCamera(cam);

    // load model
    // auto model2 = engine->ImportModel("Source/Test/Resources/DamagedHelmet.glb");
    // scene.AddGameObject(model2->GetGameObject()[0].get());
    // auto lightGO = scene.CreateGameObject();
    // auto light = lightGO->AddComponent<Light>();
    // light->SetIntensity(10);
}
