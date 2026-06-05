#include "LuaBindings.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Game/Input.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "LuaBindings_Private.hpp"
#include <glm/gtx/quaternion.hpp>

std::unordered_map<void*, std::unique_ptr<Asset>>& GetLuaCreatedRuntimeAssets()
{
    static std::unordered_map<void*, std::unique_ptr<Asset>> luaCreatedAssets{};
    return luaCreatedAssets;
}

void ClearLuaCreatedRuntimeAssets()
{
    GetLuaCreatedRuntimeAssets().clear();
}

// define typeToName
std::unordered_map<std::type_index, std::string> LuaTypeRegistery::typeToName =
    std::unordered_map<std::type_index, std::string>();

// this function will leave a table on stack
// Components
// ObjPtr
// Objects
// Assets
void LuaBindings::BindClasses(lua_State* L)
{
    lua_newtable(L);

    // clang-format off
        LuaBinder<GameScript> gameScript(L);
        gameScript
            .Begin("GameScript", false)
            .BindStaticFn("New", [](lua_State* L){ // GameScript(string className)
                    const char* className = luaL_checkstring(L, -1);

                    lua_newtable(L);

                    lua_pushstring(L, className);
                    lua_setfield(L, -2, LuaEngineTableField::className);

                    lua_pushvalue(L, -1);
                    lua_setfield(L, -2, "__index");

                    lua_getglobal(L, "wl");
                    lua_getfield(L, -1, "GameScript");
                    lua_setmetatable(L, -3);
                    lua_pop(L, 1); // pop wl table
                    return 1;
                    })
            .BindMemFn("GetGameObject", &GameScript::GetGameObject) // GameObject*()
            .End();

        LuaBinder<Time> time(L);
        time
            .Begin("Time")
            .BindStaticFn("DeltaTime", Time::DeltaTime) // float()
            .End();

        LuaBinder<glm::quat> quat(L);
        quat
            .Begin("Quaternion")
            .BindStaticFn("New", [](float w, float x, float y, float z){ return glm::quat{w, x, y, z}; }) // glm::quat(float w, float x, float y, float z)
            .BindStaticFn("FromEuler", [](const float3& euler){ return glm::quat{euler}; }) // glm::quat(float3 euler)
            .BindStaticFn("AngleAxis", [](float angle, const float3& axis){ return glm::angleAxis(angle, axis); }) // glm::quat(float angle, float3 axis)
            .BindStaticFn("Identity", [](){ return glm::quat{1.0f, 0.0f, 0.0f, 0.0f}; }) // glm::quat()
            .BindStaticFn("Slerp", [](const glm::quat& a, const glm::quat& b, float t){ return glm::slerp(a, b, t); }) // glm::quat(glm::quat a, glm::quat b, float t)
            .BindStaticFn("Dot", [](const glm::quat& a, const glm::quat& b){ return glm::dot(a, b); }) // float(glm::quat a, glm::quat b)
            .BindStaticFn("Inverse", [](const glm::quat& q){ return glm::inverse(q); }) // glm::quat(glm::quat q)
            .BindStaticFn("Normalize", [](const glm::quat& q){ return glm::normalize(q); }) // glm::quat(glm::quat q)
            .BindStaticFn("ToEulerAngles", [](const glm::quat& q){ return glm::eulerAngles(q); }) // float3(glm::quat q)
            .BindStaticFn("FromTo", [](const float3& from, const float3& to){ return glm::rotation(from, to); }) // glm::quat(float3 from, float3 to)
            .BindStaticFn("RotateVec", [](const glm::quat& q, const float3& v){ return q * v; }) // float3(glm::quat q, float3 v)
            .BindStaticFn("__eq", [](const glm::quat& l, const glm::quat& r){return l == r;}) // bool(glm::quat l, glm::quat r)
            .BindStaticFn("__mul", [](const glm::quat& l, const glm::quat& r){return l * r;}) // glm::quat(glm::quat l, glm::quat r)
            .BindProperty("x", &glm::quat::x) // float
            .BindProperty("y", &glm::quat::y) // float
            .BindProperty("z", &glm::quat::z) // float
            .BindProperty("w", &glm::quat::w) // float
            .End();

        LuaBinder<float2> vec2(L);
        vec2.Begin("Float2")
            .BindStaticFn("New", [](float x, float y){ return glm::vec2{x,y}; }) // float2(float x, float y)
            .BindStaticFn("Dot", &glm::dot<2, float, glm::packed_highp>) // float(float2 l, float2 r)
            .BindStaticFn("__eq", [](const float2& l, const float2& r){return l == r;}) // bool(float2 l, float2 r)
            .BindStaticFn("__add", [](const float2& l, const float2& r){return l + r;}) // float2(float2 l, float2 r)
            .BindStaticFn("__sub", [](const float2& l, const float2& r){return l - r;}) // float2(float2 l, float2 r)
            .BindStaticFn("__div", [](const float2& l, const float2& r){return l / r;}) // float2(float2 l, float2 r)
            .BindStaticFn("__mul", [](const float2& l, const float2& r){return l * r;}) // float2(float2 l, float2 r)
            .BindProperty("x", &glm::vec2::x) // float
            .BindProperty("y", &glm::vec2::y) // float
            .BindFn("GetX", [](glm::vec2& val) {return val[0]; }) // float()
            .BindFn("GetY", [](glm::vec2& val) {return val[1]; }) // float()
            .BindFn("SetX", [](glm::vec2& val, float v) {val[0] = v; }) // void(float v)
            .BindFn("SetY", [](glm::vec2& val, float v) {val[1] = v; }) // void(float v)
            .End();

        LuaBinder<float3> vec3(L);
        vec3
            .Begin("Float3")
            .BindStaticFn("New", [](float x, float y, float z){ return glm::vec3{x,y,z}; }) // float3(float x, float y, float z)
            .BindStaticFn("Dot", &glm::dot<3, float, glm::packed_highp>) // float(float3 l, float3 r)
            .BindStaticFn("__eq", [](const float3& l, const float3& r){return l == r;}) // bool(float3 l, float3 r)
            .BindStaticFn("__add", [](const float3& l, const float3& r){return l + r;}) // float3(float3 l, float3 r)
            .BindStaticFn("__sub", [](const float3& l, const float3& r){return l - r;}) // float3(float3 l, float3 r)
            .BindStaticFn("__div", [](const float3& l, const float3& r){return l / r;}) // float3(float3 l, float3 r)
            .BindStaticFn("__mul", [](const float3& l, const float3& r){return l * r;}) // float3(float3 l, float3 r)
            .BindProperty("x", &glm::vec3::x) // float
            .BindProperty("y", &glm::vec3::y) // float
            .BindProperty("z", &glm::vec3::z) // float
            .BindFn("GetX", [](glm::vec3& val) {return val[0]; }) // float()
            .BindFn("GetY", [](glm::vec3& val) {return val[1]; }) // float()
            .BindFn("GetZ", [](glm::vec3& val) {return val[2]; }) // float()
            .BindFn("SetX", [](glm::vec3& val, float v) {val[0] = v; }) // void(float v)
            .BindFn("SetY", [](glm::vec3& val, float v) {val[1] = v; }) // void(float v)
            .BindFn("SetZ", [](glm::vec3& val, float v) {val[2] = v; }) // void(float v)
            .End();

        LuaBinder<float4> vec4(L);
        vec4.Begin("Float4")
            .BindStaticFn("New", [](float x, float y, float z, float w){ return glm::vec4{x,y,z,w}; }) // float4(float x, float y, float z, float w)
            .BindStaticFn("Dot", &glm::dot<4, float, glm::packed_highp>) // float(float4 l, float4 r)
            .BindStaticFn("__eq", [](const float4& l, const float4& r){return l == r;}) // bool(float4 l, float4 r)
            .BindStaticFn("__add", [](const float4& l, const float4& r){return l + r;}) // float4(float4 l, float4 r)
            .BindStaticFn("__sub", [](const float4& l, const float4& r){return l - r;}) // float4(float4 l, float4 r)
            .BindStaticFn("__div", [](const float4& l, const float4& r){return l / r;}) // float4(float4 l, float4 r)
            .BindStaticFn("__mul", [](const float4& l, const float4& r){return l * r;}) // float4(float4 l, float4 r)
            .BindProperty("x", &glm::vec4::x) // float
            .BindProperty("y", &glm::vec4::y) // float
            .BindProperty("z", &glm::vec4::z) // float
            .BindProperty("w", &glm::vec4::w) // float
            .BindFn("GetX", [](glm::vec4& val) {return val[0]; }) // float()
            .BindFn("GetY", [](glm::vec4& val) {return val[1]; }) // float()
            .BindFn("GetZ", [](glm::vec4& val) {return val[2]; }) // float()
            .BindFn("GetW", [](glm::vec4& val) {return val[3]; }) // float()
            .BindFn("SetX", [](glm::vec4& val, float v) {val[0] = v; }) // void(float v)
            .BindFn("SetY", [](glm::vec4& val, float v) {val[1] = v; }) // void(float v)
            .BindFn("SetZ", [](glm::vec4& val, float v) {val[2] = v; }) // void(float v)
            .BindFn("SetW", [](glm::vec4& val, float v) {val[3] = v; }) // void(float v)
            .End();

        LuaBinder<float3x3> mat3(L);
        mat3.Begin("Float3x3")
            .BindStaticFn("New", []() { return glm::mat3(1.0f); }) // float3x3()
            .BindStaticFn("__mul", [](const float3x3& l, const float3x3& r) { return l * r; }) // float3x3(float3x3 l, float3x3 r)
            .BindStaticFn("Inverse", [](const float3x3& m) { return glm::inverse(m); }) // float3x3(float3x3 m)
            .BindStaticFn("Transpose", [](const float3x3& m) { return glm::transpose(m); }) // float3x3(float3x3 m)
            .BindFn("GetRow", [](float3x3& val, int i) { return glm::row(val, i); }) // float3(int i)
            .BindFn("GetColumn", [](float3x3& val, int i) { return glm::column(val, i); }) // float3(int i)
            .End();

        LuaBinder<float4x4> mat4(L);
        mat4.Begin("Float4x4")
            .BindStaticFn("New", []() { return glm::mat4(1.0f); }) // float4x4()
            .BindStaticFn("__mul", [](const float4x4& l, const float4x4& r) { return l * r; }) // float4x4(float4x4 l, float4x4 r)
            .BindStaticFn("Inverse", [](const float4x4& m) { return glm::inverse(m); }) // float4x4(float4x4 m)
            .BindStaticFn("Transpose", [](const float4x4& m) { return glm::transpose(m); }) // float4x4(float4x4 m)
            .BindFn("GetRow", [](float4x4& val, int i) { return glm::row(val, i); }) // float4(float4x4 m, int i)
            .BindFn("GetColumn", [](float4x4& val, int i) { return glm::column(val, i); }) // float4(float4x4 m, int i)
            .End();

        // Components

        LuaBinder<Camera> camera(L);
        camera
            .Begin("Camera")
            .BindMemFn("LookAt", &Camera::LookAt) // void(float3 lookAtPos)
            .End();

        LuaBinder<Light> light(L);
        light
            .Begin("Light")
            .BindMemFn("GetIntensity", &Light::GetIntensity) // float()
            .BindMemFn("SetIntensity", &Light::SetIntensity) // void(float intensity)
            .End();

        LuaBinder<MeshRenderer> meshRenderer(L);
        meshRenderer
            .Begin("MeshRenderer")
            .BindMemFn("SetMaterial", &MeshRenderer::SetMaterial) // void(Material* material)
            .End();

        // ObjPtr
        LuaBinder<ObjPtr<Object>> objPtr(L);
        objPtr
            .Begin("ObjPtr")
            .BindStaticFn("New", [](lua_State* L) -> int { // ObjPtr(string typeName)
                    const char* typeName = luaL_checkstring(L, -1);
                    luaL_getmetatable(L, typeName);
                    if (lua_istable(L, -1))
                    {
                        LuaUserDataPack<ObjPtr<Object>>* m = (LuaUserDataPack<ObjPtr<Object>>*)lua_newuserdata(L, sizeof(LuaUserDataPack<ObjPtr<Object>>));
                        new (m)LuaUserDataPack<ObjPtr<Object>>();
                        m->dataType = LuaEngineUserDataType::ObjPtr;
                        m->val = nullptr;
                        lua_pushvalue(L, -2);
                        lua_setmetatable(L, -2);
                    }
                    else
                    {
                        lua_pop(L, 1);
                        lua_pushnil(L);
                    }
                    return 1;
            })
            .BindStaticFn("IsValid", [](ObjPtr<Object> val) { return val != nullptr; }) // bool(ObjPtr<Object> val)
            .End();

        // Assets
        LuaBinder<Texture> texture(L);
            texture.Begin("Texture")
            .End();

        LuaBinder<Material> material(L);
            material
            .Begin("Material")
            .BindMemFn("SetTexture", &Material::SetTexture_Lua) // void(std::string param, ObjPtr<Texture> texture)
            .BindMemFn("SetShader", &Material::SetShader_Lua) // void(const char* shaderName)
            .BindMemFn("GetTexture", &Material::GetTexture) // Texture*(std::string param)
            .BindMemFn("GetShader", &Material::GetShader) // ObjPtr<Shader>()
            .BindMemFn("SetFloat", static_cast<void (Material::*)(const std::string&, float)>(&Material::SetFloat)) // void(std::string name, float value)
            .BindMemFn("SetVector", static_cast<void (Material::*)(const std::string&, const glm::vec4&)>(&Material::SetVector)) // void(std::string name, float4 value)
            .BindMemFn("SetName", &Material::SetName_Lua) // void(const char* name)
            .BindMemFn("GetName", &Material::GetName) // std::string()
            .End();
    // clang-format on

    BindGeneratedClasses(L);

    lua_setglobal(L, "wl");
}
