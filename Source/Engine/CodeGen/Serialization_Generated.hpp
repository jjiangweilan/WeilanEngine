// GENERATED FILE - DO NOT EDIT
#pragma once

class Serializer;

struct TerrainLayer;
void Serialize(Serializer* s, const TerrainLayer* val);
void Deserialize(Serializer* s, TerrainLayer* val);

struct GrassPatch;
void Serialize(Serializer* s, const GrassPatch* val);
void Deserialize(Serializer* s, GrassPatch* val);

struct GrassConfig;
void Serialize(Serializer* s, const GrassConfig* val);
void Deserialize(Serializer* s, GrassConfig* val);

struct GrassPatchGroup;
void Serialize(Serializer* s, const GrassPatchGroup* val);
void Deserialize(Serializer* s, GrassPatchGroup* val);

struct SceneEnvironmentData;
void Serialize(Serializer* s, const SceneEnvironmentData* val);
void Deserialize(Serializer* s, SceneEnvironmentData* val);

struct FogPassParameters;
void Serialize(Serializer* s, const FogPassParameters* val);
void Deserialize(Serializer* s, FogPassParameters* val);
