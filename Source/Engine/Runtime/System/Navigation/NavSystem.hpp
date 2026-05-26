#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include "NavData.hpp"

struct RuntimeNavData
{
    NavCell cell;
    bool occupied = false;
    int occupiedCount = 0;
};

struct NavObjectHandle
{
    using HandleID = uint64_t;

    HandleID GenerateHandleID();
    HandleID handleId;
};

struct NavPathQuery
{
    float maxSlopeRadians = 0.7853982f;
    bool allowDiagonal = true;
    bool smoothPath = true;
};

struct NavPathResult
{
    bool success = false;
    std::vector<float3> waypoints;
};

struct NavSteeringQuery
{
    float waypointReachDistance = 0.2f;
    float lookAheadDistance = 1.0f;
};

class MeshRenderer;
class NavSystem
{
public:
    void Init(ObjPtr<NavData> data);
    void SetRelativePosition(const float3& position) { relativePosition = position; }
    const float3& GetRelativePosition() const { return relativePosition; }
    ObjPtr<NavData> GetNavData() const { return navData; }
    void Visualize() const;

    NavPathResult FindPath(const float3& startWorld, const float3& endWorld, const NavPathQuery& query = {});
    bool GetSteeringTarget(const std::vector<float3>& waypoints, const float3& currentPosition, const NavSteeringQuery& query, float3& outTarget) const;

    static NavSystem* GetGlobalInstance();
    static void SetGlobalInstance(NavSystem* system);
    static void ClearGlobalInstance(NavSystem* system);

    void Init(int2 size);

    NavObjectHandle AddNavObject(GameObject* gameObject);
    void UpdateRuntimeNavObject(NavObjectHandle handle);
    void RemoveNavObject(NavObjectHandle handle);

private:
    struct AStarPathfinder;

    struct RegisteredMeshRenderer
    {
        MeshRenderer* renderer;
        float4x4 previousWorldMatrix;
    };

    struct CandidateCellRange
    {
        int minX = 0;
        int maxX = -1;
        int minY = 0;
        int maxY = -1;
    };

    struct RegisteredNavObject
    {
        GameObject* go;
        std::vector<RegisteredMeshRenderer> meshRenderers;
        AABB previousWorldAabb;
        bool hasPreviousOccupancy = false;
    };

    bool EnsureRuntimeCells();
    void RefreshNavObjectMeshes(RegisteredNavObject& registered);
    bool CalculateObjectWorldAABB(const RegisteredNavObject& registered, AABB& outWorldAabb) const;
    bool GetCandidateCellRange(const AABB& worldAabb, CandidateCellRange& outRange) const;
    void ApplyNavObjectOccupancy(const RegisteredNavObject& registered, const AABB& worldAabb, int delta);
    bool IsCellInBounds(int2 cell) const;
    int CellIndex(int2 cell) const;
    bool WorldToCell(const float3& world, int2& outCell) const;
    float3 CellToWorldCenter(int2 cell) const;
    bool IsCellWalkable(int2 cell) const;
    bool CanMoveBetween(int2 from, int2 to, float maxSlopeRadians) const;
    bool HasLineOfSight(int2 from, int2 to, float maxSlopeRadians) const;
    std::vector<float3> SmoothPath(const std::vector<int2>& cells, float maxSlopeRadians) const;

    std::unordered_map<NavObjectHandle::HandleID, RegisteredNavObject> registeredNavObjects;
    ObjPtr<NavData> navData;
    std::vector<RuntimeNavData> runtimeCells;
    float3 relativePosition = float3(0.0f);
};
