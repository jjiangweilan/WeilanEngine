// GENERATED FILE - DO NOT EDIT
// This file registers serialization for classes marked with [[SerClass]]

#include "Engine/Library/Serialization/Serializer.hpp"
#include "Serialization_Generated.hpp"

#include "../Runtime/System/Rendering/SceneEnvironmentData.hpp"
#include "../Runtime/System/Rendering/RenderPipeline/Passes/FogPassParameters.hpp"

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

