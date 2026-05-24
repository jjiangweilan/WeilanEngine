#pragma once
#include "Engine/Core/Ptr.hpp"
#include "NavData.hpp"

class NavSystem
{
public:
    void Init(ObjPtr<NavData> data);
    void SetRelativePosition(const float3& position) { relativePosition = position; }
    const float3& GetRelativePosition() const { return relativePosition; }
    ObjPtr<NavData> GetNavData() const { return navData; }
    void Visualize() const;

private:
    ObjPtr<NavData> navData;
    float3 relativePosition = float3(0.0f);
};
