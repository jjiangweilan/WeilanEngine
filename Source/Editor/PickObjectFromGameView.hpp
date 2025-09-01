#pragma once
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/JobSystem.hpp"
#include "Core/Scene/Scene.hpp"
#include "Libs/DynamicArray.hpp"
#include "Libs/EnumFlags.hpp"

enum class PickObjectLayer : int
{
    GameObject = 1,
    Gizmos = 1 << 1
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
        std::function<void(const Ray& ray, const std::function<void(GameObject*, float)>& intersectedPushback)>
            intersectionTest;
    };

    // main thread populates pending vectors, while worker threads process the pending. workers takes the target
    // GameObject using the `consumerIndex`, each process of a GameObject appends an `Intersected` in `results` if it's
    // intersected with the GameObject. `consumerIndex` is incrementally increased by 1 each time the worker takes a
    // GameObject to process
private:
    std::vector<PickCandidate> pending;
    std::atomic<int> consumerIndex{0};
    std::mutex mutexLock;
    std::vector<Intersected> results;

public:
    std::vector<Intersected> operator()(Scene& scene, const Ray& ray, glm::vec2 screenUV)
    {
        results.clear();
        pending.clear();

        pending = GetCandidateFromScene(scene);

        JobSystem& jobSystem = JobSystem::Instance();
        std::vector<JobHandle> jobs;

        for (int i = 0; i < pending.size(); ++i)
        {
            jobs.push_back(jobSystem.Schedule(
                [this, &ray, i]()
                {
                    pending[i].intersectionTest(
                        ray,
                        [this](GameObject* go, float distance) { this->PushbackIntersected(go, distance); }
                    );
                }
            ));
        }

        for (auto& j : jobs)
        {
            j.Wait();
        }

        return results;
    }

    void PushbackIntersected(GameObject* obj, float distance)
    {
        std::scoped_lock lock(mutexLock);
        results.push_back(Intersected{obj, distance});
    }

    static std::vector<PickCandidate> GetCandidateFromScene(Scene& scene)
    {
        std::vector<PickCandidate> pending;
        auto gameObjects = scene.GetAllGameObjects();
        for (auto obj : gameObjects)
        {
            if (obj != nullptr && obj->IsActiveInScene())
            {
                auto mr = obj->GetComponent<MeshRenderer>();
                if (mr)
                {
                    pending.push_back(PickCandidate{
                        PickObjectLayer::GameObject,
                        [obj](const Ray& ray, const std::function<void(GameObject*, float)>& intersectedPushback)
                        {
                            auto ori = ray.origin;
                            auto dir = ray.direction;
                            float distance = std::numeric_limits<float>::max();

                            if (IsRayObjectIntersect(ori, dir, obj, distance))
                            {
                                intersectedPushback(obj, distance);
                            }
                        }
                    });
                }
            }
        }
        return pending;
    }

    static bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj, float& distance)
    {
        distance = std::numeric_limits<float>::max();
        auto mr = obj->GetComponent<MeshRenderer>();
        bool intersected = false;
        if (mr)
        {
            auto model = obj->GetWorldMatrix();
            auto meshes = mr->GetMeshes();
            for (auto mesh : meshes)
            {
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
                                // newDistance > 0 means the triangle is in front of the camera
                                if (newDistance > 0)
                                {
                                    intersected = true;
                                    distance = glm::min(distance, newDistance);
                                }
                            }
                        }
                    }
                }
            }
        }

        return intersected;
    }
};
