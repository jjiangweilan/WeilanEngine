#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "NavData.hpp"

struct RuntimeNavData
{
    NavCell cell;
    bool occupied = false;
};

struct NavObjectHandle
{
    using HandleID = uint64_t;

    HandleID GenerateHandleID();
    HandleID handleId;
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

    static NavSystem* GetGlobalInstance();
    static void SetGlobalInstance(NavSystem* system);
    static void ClearGlobalInstance(NavSystem* system);

    void Init(int2 size);

    NavObjectHandle AddNavObject(GameObject* gameObject);
    void UpdateRuntimeNavObject(NavObjectHandle handle);
    void RemoveNavObject(NavObjectHandle handle);

private:
    struct RegisteredNavObject
    {
        GameObject* go;
        std::vector<MeshRenderer*> meshRenderers;
    };

    void RebuildRuntimeOccupancy();

    std::unordered_map<NavObjectHandle::HandleID, RegisteredNavObject> registeredNavObjects;
    ObjPtr<NavData> navData;
    std::vector<RuntimeNavData> runtimeCells;
    float3 relativePosition = float3(0.0f);
};
