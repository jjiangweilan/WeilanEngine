#include "LuaBindings.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Game/Input.hpp"
#include "LuaBindings_Private.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

std::unordered_map<void*, std::unique_ptr<Asset>>& GetLuaCreatedRuntimeAssets()
{
    static std::unordered_map<void*, std::unique_ptr<Asset>> luaCreatedAssets{};
    return luaCreatedAssets;
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
            .BindFn("New", [](lua_State* L){
                    // expecting a `self` table on top of the stack
                    // ASSERT(lua_istable(L, 1));
                    // ASSERT(lua_isstring(L, 2));
                    const char* className = lua_tostring(L, 2);

                    lua_newtable(L);

                    lua_pushstring(L, className);
                    lua_setfield(L, 3, LuaEngineTableField::className);

                    lua_pushvalue(L, -1);
                    lua_setfield(L, 3, "__index");

                    lua_pushvalue(L, 1);
                    lua_setmetatable(L, 3);
                    return 1;
                    })
            .BindMemFn("GetGameObject", &GameScript::GetGameObject)
            .End();

        LuaBinder<GameObject> gameObject(L);
        gameObject
            .Begin("GameObject")
            .BindMemFn("GetPosition", &GameObject::GetPosition)
            .BindMemFn("SetPosition", &GameObject::SetPosition)
            .BindMemFn("GetComponentInHierachy", &GameObject::GetComponentInHierachy)
            .BindMemFn("LookAt", &GameObject::LookAt)
            .BindFn("GetComponent", [](lua_State* L) -> int {
                    GameObject* go = GetLuaUserDataPackValue<GameObject>(L, 1);
                    const char* className = luaL_checkstring(L, 2);

                    if (go == nullptr || className == nullptr)
                        return 0;
                    
                    auto v = go->GetComponent(className);
                    if (v != nullptr)
                    {
                        LuaBinder<GameObject>::ProcessRtn(L, std::move(v));
                        return 1;
                    }

                    // failed to get Engine Component, try Lua Script
                    auto gameScript = go->GetComponent<GameScript>();
                    if(gameScript != nullptr)
                    {
                        auto& luaClassName = gameScript->GetLuaClassName();
                        if (luaClassName == className)
                        {
                            return gameScript->LuaPushReferenceToStack();
                        }
                    }

                    return 0;
                })
            .End();

        LuaBinder<Time> time(L);
        time
            .Begin("Time")
            .BindStaticFn("DeltaTime", Time::DeltaTime)
            .End();

        LuaBinder<float2> vec2(L);
        vec2.Begin("Float2")
            .BindStaticFn("New", [](float x, float y){ return glm::vec2{x,y}; })
            .BindStaticFn("Dot", &glm::dot<2, float, glm::packed_highp>)
            .BindStaticFn("__eq", [](const float2& l, const float2& r){return l == r;})
            .BindStaticFn("__add", [](const float2& l, const float2& r){return l + r;})
            .BindStaticFn("__sub", [](const float2& l, const float2& r){return l - r;})
            .BindStaticFn("__div", [](const float2& l, const float2& r){return l / r;})
            .BindStaticFn("__mul", [](const float2& l, const float2& r){return l * r;})
            .BindProperty("x", &glm::vec2::x)
            .BindProperty("y", &glm::vec2::y)
            .BindFn("GetX", [](glm::vec2& val) {return val[0]; })
            .BindFn("GetY", [](glm::vec2& val) {return val[1]; })
            .BindFn("SetX", [](glm::vec2& val, float v) {val[0] = v; })
            .BindFn("SetY", [](glm::vec2& val, float v) {val[1] = v; })
            .End();

        LuaBinder<float3> vec3(L);
        vec3
            .Begin("Float3")
            .BindStaticFn("New", [](float x, float y, float z){ return glm::vec3{x,y,z}; })
            .BindStaticFn("Dot", &glm::dot<3, float, glm::packed_highp>)
            .BindStaticFn("__eq", [](const float3& l, const float3& r){return l == r;})
            .BindStaticFn("__add", [](const float3& l, const float3& r){return l + r;})
            .BindStaticFn("__sub", [](const float3& l, const float3& r){return l - r;})
            .BindStaticFn("__div", [](const float3& l, const float3& r){return l / r;})
            .BindStaticFn("__mul", [](const float3& l, const float3& r){return l * r;})
            .BindProperty("x", &glm::vec3::x)
            .BindProperty("y", &glm::vec3::y)
            .BindProperty("z", &glm::vec3::z)
            .BindFn("GetX", [](glm::vec3& val) {return val[0]; })
            .BindFn("GetY", [](glm::vec3& val) {return val[1]; })
            .BindFn("GetZ", [](glm::vec3& val) {return val[2]; })
            .BindFn("SetX", [](glm::vec3& val, float v) {val[0] = v; })
            .BindFn("SetY", [](glm::vec3& val, float v) {val[1] = v; })
            .BindFn("SetZ", [](glm::vec3& val, float v) {val[2] = v; })
            .End();

        LuaBinder<float4> vec4(L);
        vec4.Begin("Float4")
            .BindStaticFn("New", [](float x, float y, float z, float w){ return glm::vec4{x,y,z,w}; })
            .BindStaticFn("Dot", &glm::dot<4, float, glm::packed_highp>)
            .BindStaticFn("__eq", [](const float4& l, const float4& r){return l == r;})
            .BindStaticFn("__add", [](const float4& l, const float4& r){return l + r;})
            .BindStaticFn("__sub", [](const float4& l, const float4& r){return l - r;})
            .BindStaticFn("__div", [](const float4& l, const float4& r){return l / r;})
            .BindStaticFn("__mul", [](const float4& l, const float4& r){return l * r;})
            .BindProperty("x", &glm::vec4::x)
            .BindProperty("y", &glm::vec4::y)
            .BindProperty("z", &glm::vec4::z)
            .BindProperty("w", &glm::vec4::w)
            .BindFn("GetX", [](glm::vec4& val) {return val[0]; })
            .BindFn("GetY", [](glm::vec4& val) {return val[1]; })
            .BindFn("GetZ", [](glm::vec4& val) {return val[2]; })
            .BindFn("GetW", [](glm::vec4& val) {return val[3]; })
            .BindFn("SetX", [](glm::vec4& val, float v) {val[0] = v; })
            .BindFn("SetY", [](glm::vec4& val, float v) {val[1] = v; })
            .BindFn("SetZ", [](glm::vec4& val, float v) {val[2] = v; })
            .BindFn("SetW", [](glm::vec4& val, float v) {val[3] = v; })
            .End();
            
        // Components

        LuaBinder<Camera> camera(L);
        camera
            .Begin("Camera")
            .BindMemFn("LookAt", &Camera::LookAt)
            .End();

        LuaBinder<Light> light(L);
        light
            .Begin("Light")
            .BindMemFn("GetIntensity", &Light::GetIntensity)
            .BindMemFn("SetIntensity", &Light::SetIntensity)
            .End();

        LuaBinder<MeshRenderer> meshRenderer(L);
        meshRenderer
            .Begin("MeshRenderer")
            .BindMemFn("SetMaterial", &MeshRenderer::SetMaterial)
            .End();

        // ObjPtr
        LuaBinder<ObjPtr<Object>> objPtr(L);
        objPtr
            .Begin("ObjPtr")
            .BindStaticFn("New", [](lua_State* L) -> int { 
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
            .BindStaticFn("IsValid", [](ObjPtr<Object> val) { return val != nullptr; })
            .End();

        // Assets
        LuaBinder<Texture> texture(L);
            texture.Begin("Texture")
            .End();

        LuaBinder<Material> material(L);
            material
            .Begin("Material")
            .BindMemFn("SetTexture", &Material::SetTexture_Lua)
            .BindMemFn("SetShader", &Material::SetShader_Lua)
            .BindMemFn("GetTexture", &Material::GetTexture)
            .BindMemFn("GetShader", &Material::GetShader)
            .BindMemFn("SetFloat", static_cast<void (Material::*)(const std::string&, float)>(&Material::SetFloat))
            .BindMemFn("SetVector", static_cast<void (Material::*)(const std::string&, const glm::vec4&)>(&Material::SetVector))
            .BindMemFn("SetName", &Material::SetName_Lua)
            .BindMemFn("GetName", &Material::GetName)
            .End();
    // clang-format on

    BindGeneratedClasses(L);

    lua_setglobal(L, "wl");
}
