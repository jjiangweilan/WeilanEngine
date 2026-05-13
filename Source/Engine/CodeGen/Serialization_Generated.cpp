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
}

void Deserialize(Serializer* s, GrassConfig* val)
{
    s->Deserialize("albedo", val->albedo);
    s->Deserialize("scale", val->scale);
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

