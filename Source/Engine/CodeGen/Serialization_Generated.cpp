// GENERATED FILE - DO NOT EDIT
// This file registers serialization for classes marked with [[SerClass]]

#include "Engine/Library/Serialization/Serializer.hpp"
#include "Serialization_Generated.hpp"

#include "../Runtime/Object/Component/GrassSurface.hpp"
#include "../Runtime/System/Rendering/SceneEnvironmentData.hpp"
#include "../Runtime/System/Rendering/RenderPipeline/Passes/FogPassParameters.hpp"

void Serialize(Serializer* s, const GrassPatch* val)
{
    s->Serialize("position", val->position);
    s->Serialize("meshIndex", val->meshIndex);
}

void Deserialize(Serializer* s, GrassPatch* val)
{
    s->Deserialize("position", val->position);
    s->Deserialize("meshIndex", val->meshIndex);
}


void Serialize(Serializer* s, const GrassConfig* val)
{
    s->Serialize("albedo", val->albedo);
    s->Serialize("scale", val->scale);
    s->Serialize("grassShadowMask0", val->grassShadowMask0);
    s->Serialize("grassShadowMask1", val->grassShadowMask1);
    s->Serialize("windTex", val->windTex);
    s->Serialize("grassColorRamp_Bottom", val->grassColorRamp_Bottom);
    s->Serialize("grassColorRamp_Top", val->grassColorRamp_Top);
    s->Serialize("grassColorRamp2_Bottom", val->grassColorRamp2_Bottom);
    s->Serialize("grassColorRamp2_Top", val->grassColorRamp2_Top);
    s->Serialize("grassColorRamp3_Bottom", val->grassColorRamp3_Bottom);
    s->Serialize("grassColorRamp3_Top", val->grassColorRamp3_Top);
    s->Serialize("grassMaskUVScaler", val->grassMaskUVScaler);
    s->Serialize("hueShift_0", val->hueShift_0);
    s->Serialize("hueShift_1", val->hueShift_1);
    s->Serialize("windScale", val->windScale);
}

void Deserialize(Serializer* s, GrassConfig* val)
{
    s->Deserialize("albedo", val->albedo);
    s->Deserialize("scale", val->scale);
    s->Deserialize("grassShadowMask0", val->grassShadowMask0);
    s->Deserialize("grassShadowMask1", val->grassShadowMask1);
    s->Deserialize("windTex", val->windTex);
    s->Deserialize("grassColorRamp_Bottom", val->grassColorRamp_Bottom);
    s->Deserialize("grassColorRamp_Top", val->grassColorRamp_Top);
    s->Deserialize("grassColorRamp2_Bottom", val->grassColorRamp2_Bottom);
    s->Deserialize("grassColorRamp2_Top", val->grassColorRamp2_Top);
    s->Deserialize("grassColorRamp3_Bottom", val->grassColorRamp3_Bottom);
    s->Deserialize("grassColorRamp3_Top", val->grassColorRamp3_Top);
    s->Deserialize("grassMaskUVScaler", val->grassMaskUVScaler);
    s->Deserialize("hueShift_0", val->hueShift_0);
    s->Deserialize("hueShift_1", val->hueShift_1);
    s->Deserialize("windScale", val->windScale);
}


void Serialize(Serializer* s, const GrassPatchGroup* val)
{
    s->Serialize("patchMeshes", val->patchMeshes);
    s->Serialize("patches", val->patches);
    s->Serialize("config", val->config);
}

void Deserialize(Serializer* s, GrassPatchGroup* val)
{
    s->Deserialize("patchMeshes", val->patchMeshes);
    s->Deserialize("patches", val->patches);
    s->Deserialize("config", val->config);
}


void Serialize(Serializer* s, const SceneEnvironmentData* val)
{
    s->Serialize("fogPassParameters", val->fogPassParameters);
}

void Deserialize(Serializer* s, SceneEnvironmentData* val)
{
    s->Deserialize("fogPassParameters", val->fogPassParameters);
}


void Serialize(Serializer* s, const FogPassParameters* val)
{
    s->Serialize("enabled", val->enabled);
    s->Serialize("fogColor", val->fogColor);
    s->Serialize("fogDensity", val->fogDensity);
}

void Deserialize(Serializer* s, FogPassParameters* val)
{
    s->Deserialize("enabled", val->enabled);
    s->Deserialize("fogColor", val->fogColor);
    s->Deserialize("fogDensity", val->fogDensity);
}

