#include "AssetDatabase/Importers.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Editor/Tool.hpp"
#include "Rendering/SceneRenderer.hpp"
#include "Utils/AssetLoader.hpp"
#include "WeilanEngine.hpp"
#include "spdlog/spdlog.h"
#include <gtest/gtest.h>
using namespace Engine;
TEST(Gameplay, Test0)
{
    auto engine = std::make_unique<Engine::WeilanEngine>();
    engine->Init({});

    Engine::Scene scene;
    engine->sceneManager->SetActiveScene(scene);

    // set camera
    Engine::GameObject* gameObject = scene.CreateGameObject();
    gameObject->SetName("Camera");
    auto cam = gameObject->AddComponent<Engine::Camera>();
    scene.SetMainCamera(cam);

    // handle swapchain change
    scene.RegisterSystemEventCallback(
        [cam](SDL_Event& event)
        {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                float width = event.window.data1;
                float height = event.window.data2;
                cam->SetProjectionMatrix(glm::radians(45.0f), width / (float)height, 0.01f, 1000.f);
            }
        }
    );

    // load model
    auto model2 = engine->ImportModel("Source/Test/Resources/DamagedHelmet.glb");
    scene.AddGameObject(model2->GetGameObject()[0].get());
    auto lightGO = scene.CreateGameObject();
    auto light = lightGO->AddComponent<Engine::Light>();
    light->SetIntensity(10);

    // create cube
    auto cubeObj = scene.CreateGameObject();
    auto cubeObjRenderer = cubeObj->AddComponent<MeshRenderer>();
    auto cubeShader = Shader("Assets/Shaders/Cubemap.shad");
    auto cubeModel = Importers::GLB("Assets/cube.glb", &cubeShader);
    auto cubeMat = Material(&cubeShader);
    cubeObjRenderer->SetMesh(cubeModel->GetMeshes()[0].get());
    Material* mats[] = {&cubeMat};
    cubeObjRenderer->SetMaterials(mats);

    engine->Loop();
}
