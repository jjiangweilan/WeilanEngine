#pragma once
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "Libs/EnumFlags.hpp"
#include <vector>

enum class PickObjectLayer : int
{
    GameObject,
    Gizmos
};
ENUM_FLAGS(PickObjectLayer, int);

struct PickGameObjectFromScene
{
    struct Intersected
    {
        GameObject* go;
        float distance;
    };

    struct PickCandidate
    {
        PickObjectLayer layer;
        std::function<void(const Ray& ray)> intersectionTest;
    };

    // main thread populates pending vectors, while worker threads process the pending. workers takes the target
    // GameObject using the `consumerIndex`, each process of a GameObject appends an `Intersected` in `results` if it's
    // intersected with the GameObject. `consumerIndex` is incrementally increased by 1 each time the worker takes a
    // GameObject to process
private:
    std::vector<GameObject*> pending;
    std::atomic<int> consumerIndex{0};

public:
    std::vector<Intersected> results;

    void operator()(Scene& scene, const Ray& ray, glm::vec2 screenUV, std::vector<Intersected>& intersected)
    {
        pending = scene.GetAllGameObjects();
        std::vector<std::thread> threads;
        std::mutex mutexLock;
        int maxThreads = std::max(1.0f, std::thread::hardware_concurrency() - 2.0f);
        for (int i = 0; i < maxThreads; ++i)
        {
            threads.push_back(std::thread(
                [this, &mutexLock, &intersected, &ray]()
                {
                    while (consumerIndex < pending.size())
                    {
                        int index = consumerIndex.fetch_add(1);

                        GameObject* obj = pending[index];
                        if (obj == nullptr || !obj->IsEnabled())
                            continue;

                        auto ori = ray.origin;
                        auto dir = ray.direction;
                        float distance = std::numeric_limits<float>::max();
                        if (IsRayObjectIntersect(ori, dir, obj, distance))
                        {
                            std::scoped_lock lock(mutexLock);
                            intersected.push_back(Intersected{obj, distance});
                        }
                    };
                }
            ));
        }

        for (auto& t : threads)
            t.join();
    }

    bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj, float& distance)
    {
        distance = std::numeric_limits<float>::max();
        auto mr = obj->GetComponent<MeshRenderer>();
        if (mr)
        {
            auto model = obj->GetWorldMatrix();
            auto mesh = mr->GetMesh();
            if (mesh)
            {
                for (const Submesh& submesh : mesh->GetSubmeshes())
                {
                    auto& indices = submesh.GetIndices();
                    auto& positions = submesh.GetPositions();

                    // I just assume binding zero is a vec3 position, this is not robust
                    for (int i = 0; i < submesh.GetIndexCount(); i += 3)
                    {
                        int j = i + 1;
                        int k = i + 2;

                        glm::vec3 v0, v1, v2;
                        v0 = model * glm::vec4(positions[indices[i]], 1.0);
                        v1 = model * glm::vec4(positions[indices[j]], 1.0);
                        v2 = model * glm::vec4(positions[indices[k]], 1.0);

                        glm::vec2 bary;
                        float newDistance = -1;
                        if (glm::intersectRayTriangle(ori, dir, v0, v1, v2, bary, newDistance))
                        {
                            distance = glm::min(distance, newDistance);
                        }
                    }
                }
            }
        }

        return distance > 0;
    }
};
