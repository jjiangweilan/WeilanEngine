#pragma once
#include "Core/Asset.hpp"
#include "Core/Object.hpp"
#include "Core/Ptr.hpp"
#include <typeindex>

enum class LuaEngineUserDataType
{
    Value,
    RawPtr,
    ObjPtr,
    RuntimeObject
};

template <class T>
struct LuaUserDataPack
{
    LuaEngineUserDataType dataType;
    T val;

    // expectedClassName: class name of val
    void Assign(const char* expectedClassName, ObjPtr<Object> assigningObject)
    {
        if constexpr (IsObject<T>)
        {
            if (assigningObject == nullptr || strcmp(expectedClassName, assigningObject->GetTypeName().c_str()) == 0)
            {
                this->val = assigningObject;
            }
            else
            {
                spdlog::warn(
                    "Lua: type mismatch, expected: {}, got: {}",
                    expectedClassName,
                    assigningObject->GetTypeName()
                );
            }
        }
    }
};

struct LuaTypeRegistery
{
    static std::unordered_map<std::type_index, std::string> typeToName;
};

struct LuaEngineTableField
{
    inline static const char* dataType = "__wl_dataType";
    inline static const char* propertiesGet = "__wl_properties_get";
    inline static const char* propertiesSet = "__wl_properties_set";
    inline static const char* className = "__wl_className";
};

std::unordered_map<void*, std::unique_ptr<Asset>>& GetLuaCreatedRuntimeAssets();
