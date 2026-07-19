#pragma once
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/Terrain.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Library/EnumFlags.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"

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
        scene.ForEachGameObject([&pending](GameObject* obj)
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
        });
        return pending;
    }

    static bool IsRayObjectIntersect(glm::vec3 ori, glm::vec3 dir, GameObject* obj, float& distance)
    {
        glm::vec3 dummyPoint, dummyNormal;
        return IsRayObjectIntersect(ori, dir, obj, distance, dummyPoint, dummyNormal);
    }

    static bool IsRayObjectIntersect(
        glm::vec3 ori, glm::vec3 dir, GameObject* obj,
        float& outDistance, glm::vec3& outPoint, glm::vec3& outNormal
    )
    {
        outDistance = std::numeric_limits<float>::max();
        auto mr = obj->GetComponent<MeshRenderer>();
        bool intersected = false;
        if (mr)
        {
            if (auto* terrain = dynamic_cast<Terrain*>(mr))
                return terrain->Raycast(Ray{ori, dir}, outDistance, outPoint, outNormal);

            Ray worldRay{ori, dir};
            float aabbDistance;
            if (!RayVsAABB(worldRay, mr->GetAABB(), aabbDistance))
                return false;

            auto model = obj->GetWorldMatrix();
            auto invModel = glm::inverse(model);
            auto normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            glm::vec3 localOri = invModel * glm::vec4(ori, 1.0f);
            glm::vec3 localDir = invModel * glm::vec4(dir, 0.0f);

            auto meshes = mr->GetMeshes();
            for (auto mesh : meshes)
            {
                if (mesh)
                {
                    for (const Submesh& submesh : mesh->GetSubmeshes())
                    {
                        auto& indices = submesh.GetIndices();
                        auto& positions = submesh.GetPositions();

                        for (int i = 0; i < submesh.GetIndexCount(); i += 3)
                        {
                            int j = i + 1;
                            int k = i + 2;

                            const glm::vec3& v0 = positions[indices[i]];
                            const glm::vec3& v1 = positions[indices[j]];
                            const glm::vec3& v2 = positions[indices[k]];

                            glm::vec2 bary;
                            float newDistance = -1;
                            if (glm::intersectRayTriangle(localOri, localDir, v0, v1, v2, bary, newDistance))
                            {
                                if (newDistance > 0 && newDistance < outDistance)
                                {
                                    intersected = true;
                                    outDistance = newDistance;
                                    outPoint = ori + dir * newDistance;
                                    outNormal = glm::normalize(normalMatrix * glm::cross(v1 - v0, v2 - v0));
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

struct SurfaceHit
{
    GameObject* go = nullptr;
    float distance = std::numeric_limits<float>::max();
    glm::vec3 point = glm::vec3(0);
    glm::vec3 normal = glm::vec3(0, 1, 0);
};

inline SurfaceHit RaycastSceneSurface(const Ray& ray, Scene& scene)
{
    SurfaceHit bestHit;
    auto candidates = scene.GetBVHScene().QueryRay(ray, BVHNodeType::MeshRenderer);
    for (BVHNode* node : candidates)
    {
        if (node == nullptr || node->owner == nullptr)
            continue;

        auto mr = static_cast<MeshRenderer*>(node->owner);
        GameObject* obj = mr->GetGameObject();
        if (obj != nullptr && obj->IsActiveInScene())
        {
            float distance;
            glm::vec3 point, normal;
            if (PickGameObjectFromScene::IsRayObjectIntersect(
                    ray.origin, ray.direction, obj, distance, point, normal)
                && distance > 0 && distance < bestHit.distance)
            {
                bestHit.go = obj;
                bestHit.distance = distance;
                bestHit.point = point;
                bestHit.normal = normal;
            }
        }
    }
    return bestHit;
}
