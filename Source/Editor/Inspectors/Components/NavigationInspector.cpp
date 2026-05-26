#include "../Inspector.hpp"

#include "Editor/EditorGUI.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Runtime/Object/Component/Navigation.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"

#include <glm/gtc/constants.hpp>

class NavigationTest : public Component
{
    DECLARE_COMPONENT(NavigationTest);

public:
    void OnStart() override
    {
        if (resetToStartOnStart && movingObject)
        {
            ResetToStart();
            RecalculatePath();
        }
    }

    void Tick() override
    {
        if (!movingObject || !endObject)
            return;

        NavSystem* navSystem = NavSystem::GetGlobalInstance();
        if (!navSystem)
            return;

        const float3 endPos = endObject->GetPosition();
        if (endPos != cachedEndPosition)
        {
            RecalculatePath();
            cachedEndPosition = endPos;
        }

        if (path.empty() || currentWaypointIndex >= static_cast<int>(path.size()))
            return;

        while (currentWaypointIndex < static_cast<int>(path.size()))
        {
            const float distance = glm::length(path[currentWaypointIndex] - movingObject->GetPosition());
            if (distance > waypointReachDistance)
                break;

            ++currentWaypointIndex;
        }

        if (currentWaypointIndex >= static_cast<int>(path.size()))
        {
            movingObject->SetPosition(path.back());
            return;
        }

        const float3 currentPos = movingObject->GetPosition();
        const float3 targetWaypoint = path[currentWaypointIndex];
        const float3 toTarget = targetWaypoint - currentPos;
        const float distance = glm::length(toTarget);
        if (distance <= 0.0001f)
            return;

        const float step = moveSpeed * Time::DeltaTime();
        if (step >= distance)
        {
            movingObject->SetPosition(targetWaypoint);
            ++currentWaypointIndex;
        }
        else
        {
            movingObject->SetPosition(currentPos + glm::normalize(toTarget) * step);
        }
    }

    void DebugDraw() override
    {
        if (path.size() < 2)
            return;

        std::vector<Graphics::Line> lines;
        lines.reserve(path.size() - 1);
        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            lines.push_back({path[i], path[i + 1], float4(0.0f, 1.0f, 1.0f, 1.0f)});
        }
        Graphics::DrawLines(lines);
    }

    void Serialize(Serializer* s) const override
    {
        Component::Serialize(s);
        s->Serialize("movingObject", movingObject);
        s->Serialize("startObject", startObject);
        s->Serialize("endObject", endObject);
        s->Serialize("moveSpeed", moveSpeed);
        s->Serialize("waypointReachDistance", waypointReachDistance);
        s->Serialize("resetToStartOnStart", resetToStartOnStart);
    }

    void Deserialize(Serializer* s) override
    {
        Component::Deserialize(s);
        s->Deserialize("movingObject", movingObject);
        s->Deserialize("startObject", startObject);
        s->Deserialize("endObject", endObject);
        s->Deserialize("moveSpeed", moveSpeed);
        s->Deserialize("waypointReachDistance", waypointReachDistance);
        s->Deserialize("resetToStartOnStart", resetToStartOnStart);
    }

    std::unique_ptr<Component> Clone(GameObject& owner) override
    {
        std::unique_ptr<NavigationTest> clone = std::make_unique<NavigationTest>(&owner);
        clone->movingObject = movingObject;
        clone->startObject = startObject;
        clone->endObject = endObject;
        clone->moveSpeed = moveSpeed;
        clone->waypointReachDistance = waypointReachDistance;
        clone->resetToStartOnStart = resetToStartOnStart;
        return clone;
    }

    void RecalculatePath()
    {
        path.clear();
        currentWaypointIndex = 0;

        NavSystem* navSystem = NavSystem::GetGlobalInstance();
        if (!navSystem || !movingObject || !endObject)
            return;

        const float3 startPos = movingObject->GetPosition();
        const float3 endPos = endObject->GetPosition();

        NavPathResult result = navSystem->FindPath(startPos, endPos);
        if (result.success)
        {
            path = std::move(result.waypoints);
            currentWaypointIndex = 0;
        }
    }

    void ResetToStart()
    {
        path.clear();
        currentWaypointIndex = 0;
        if (startObject && movingObject)
        {
            movingObject->SetPosition(startObject->GetPosition());
        }
    }

    void ClearPath()
    {
        path.clear();
        currentWaypointIndex = 0;
    }

    ObjPtr<GameObject> movingObject;
    ObjPtr<GameObject> startObject;
    ObjPtr<GameObject> endObject;
    float moveSpeed = 3.0f;
    float waypointReachDistance = 0.2f;
    bool resetToStartOnStart = true;

private:
    std::vector<float3> path;
    int currentWaypointIndex = 0;
    float3 cachedEndPosition = float3(0.0f);
};

DEFINE_COMPONENT(NavigationTest, "5BC08B62-0810-4888-9907-4DADA3632573")

namespace Editor
{
class NavigationInspector : public Inspector<Navigation>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Navigation>::DrawInspector(editor);

        NavData* navData = target->GetNavData().Get();
        if (EditorGUI::ObjectField("Nav Data", navData))
        {
            target->SetNavData(navData);
        }

        ImGui::SeparatorText("Pathfinding");
        {
            NavPathQuery& query = target->GetPathQuery();
            float maxSlopeDegrees = glm::degrees(query.maxSlopeRadians);
            if (EditorGUI::DragFloat("Max Slope", &maxSlopeDegrees, 0.1f, 0.0f, 89.0f))
            {
                query.maxSlopeRadians = glm::radians(maxSlopeDegrees);
            }
            ImGui::TextUnformatted("degrees");
            EditorGUI::Checkbox("Allow Diagonal", &query.allowDiagonal);
            EditorGUI::Checkbox("Smooth Path", &query.smoothPath);
        }

        ImGui::SeparatorText("Steering");
        {
            NavSteeringQuery& query = target->GetSteeringQuery();
            EditorGUI::DragFloat("Waypoint Reach Dist", &query.waypointReachDistance, 0.01f, 0.0f);
            EditorGUI::DragFloat("Look Ahead Distance", &query.lookAheadDistance, 0.01f, 0.0f);
        }
    }

private:
    static const char _register;
};

const char NavigationInspector::_register = InspectorRegistry::Register<NavigationInspector, Navigation>();

class NavigationTestInspector : public Inspector<NavigationTest>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<NavigationTest>::DrawInspector(editor);

        ImGui::SeparatorText("Objects");
        {
            GameObject* moving = target->movingObject.Get();
            if (EditorGUI::ObjectField("Moving Object", moving))
            {
                target->movingObject = moving;
            }
            GameObject* start = target->startObject.Get();
            if (EditorGUI::ObjectField("Start Object", start))
            {
                target->startObject = start;
            }
            GameObject* end = target->endObject.Get();
            if (EditorGUI::ObjectField("End Object", end))
            {
                target->endObject = end;
            }
        }

        ImGui::SeparatorText("Movement");
        EditorGUI::DragFloat("Move Speed", &target->moveSpeed, 0.1f, 0.0f);
        EditorGUI::DragFloat("Waypoint Reach Dist", &target->waypointReachDistance, 0.01f, 0.0f);
        EditorGUI::Checkbox("Reset To Start On Start", &target->resetToStartOnStart);

        ImGui::SeparatorText("Controls");
        if (EditorGUI::Button("Reset To Start", "Reset"))
        {
            target->ResetToStart();
        }
        if (EditorGUI::Button("Recalculate Path", "Recalc"))
        {
            target->RecalculatePath();
        }
        if (EditorGUI::Button("Clear Path", "Clear"))
        {
            target->ClearPath();
        }
    }

private:
    static const char _register;
};

const char NavigationTestInspector::_register = InspectorRegistry::Register<NavigationTestInspector, NavigationTest>();
} // namespace Editor
