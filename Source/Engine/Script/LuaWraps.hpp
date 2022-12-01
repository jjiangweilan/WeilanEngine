#pragma once
#include "ThirdParty/lua/lua.hpp"
#include "Core/GameObject.hpp"
#include "Core/Component/Transform.hpp"
#include <functional>
#include <type_traits>
#include <tuple>

namespace Engine
{
    using LuaRef=int;

    class LuaWraps
    {
        public:
            LuaWraps(lua_State* state) : L(state)
            {
                WrapGameObject();
                WrapTransform();
            }

            static int GameObject_GetName(lua_State* L)
            {
                GameObject** gameObjectPtr = (GameObject**)luaL_checkudata(L, 1, "GameObject");
                if (gameObjectPtr)
                {
                    GameObject* gameObject = *gameObjectPtr;
                    const std::string& (GameObject::*ptr)()  = &GameObject::GetName;
                    lua_pushstring(L, std::invoke(ptr, gameObject).c_str());
                    return 1;
                }

                return 0;
            }

            static int GameObject_GetComponent(lua_State* L)
            {
                GameObject** gameObjectPtr = (GameObject**)luaL_checkudata(L, 1, "GameObject");
                if (gameObjectPtr)
                {
                    GameObject* gameObject = *gameObjectPtr;
                    const char* compName = luaL_checkstring(L, 2);
                    if (compName)
                    {
                        Component* c = gameObject->GetComponent(compName).Get();
                        if (c)
                        {
                            Component** cPtr = (Component**)lua_newuserdata(L, sizeof(void*));
                            *cPtr = c;
                            luaL_setmetatable(L, compName);
                            return 1;
                        }
                    }
                }

                return 0;
            }

            void WrapGameObject()
            {
                luaL_newmetatable(L, "GameObject");
                lua_pushvalue(L, -1);
                lua_setfield(L, -2, "__index");

                lua_pushcfunction(L, GameObject_GetName);
                lua_setfield(L, -2, "GetName");
                lua_pushcfunction(L, GameObject_GetComponent);
                lua_setfield(L, -2, "GetComponent");
            }

            static int Transform_SetPosition(lua_State* L)
            {
                Transform** transformPtr = (Transform**)luaL_checkudata(L, -1, "Transform");
                if (transformPtr)
                {
                    Transform* transform = *transformPtr;
                    lua_rawgeti(L, -1, 1);
                    lua_rawgeti(L, -1, 2);
                    lua_rawgeti(L, -1, 3);

                    float x = luaL_checknumber(L, -3);
                    float y = luaL_checknumber(L, -2);
                    float z = luaL_checknumber(L, -1);

                    transform->SetPostion({x,y,z});
                }

                return 0;
            }

            static int Transform_GetPosition(lua_State* L)
            {
                Transform** transformPtr = (Transform**) luaL_checkudata(L, 1, "Transform");
                if (transformPtr)
                {
                    Transform* transform = *transformPtr;
                    const glm::vec3& p = transform->GetPosition();

                    lua_createtable(L, 3, 0);
                    lua_pushnumber(L, p.x);
                    lua_seti(L, -2, 1);
                    lua_pushnumber(L, p.y);
                    lua_seti(L, -2, 2);
                    lua_pushnumber(L, p.z);
                    lua_seti(L, -2, 3);

                    return 1;
                }

                return 0;
            }

#define REGISTER_CFUNCTION(ClassName, Function) \
            lua_pushcfunction(L, Wrap<&ClassName::Function>(&ClassName::Function)); \
            lua_setfield(L, -2, #Function);

            void WrapTransform()
            {
                luaL_newmetatable(L, "Transform");
                lua_pushvalue(L, -1);
                lua_setfield(L, -2, "__index");

                REGISTER_CFUNCTION(Transform, SetPostion);
                REGISTER_CFUNCTION(Transform, GetPosition);
            }

        private:

            template<int index, class Type, class... Args>
            static void WrapParamAtIndex(lua_State* L, std::tuple<Args...>& args)
            {
                if constexpr (std::is_same_v<Type, glm::vec3>)
                {
                    lua_rawgeti(L, 2, 1);
                    lua_rawgeti(L, 2, 2);
                    lua_rawgeti(L, 2, 3);

                    float x = luaL_checknumber(L, -3);
                    float y = luaL_checknumber(L, -2);
                    float z = luaL_checknumber(L, -1);

                    std::get<index>(args) = glm::vec3(x, y, z);
                }
                // extend parameter types here...

                if constexpr (index + 1 < std::tuple_size_v<std::tuple<Args...>>)
                    WrapParamAtIndex<index + 1, std::tuple_element<index + 1, std::tuple<Args...>>::type, Args>(L, args);
            }

            template<class... Args>
            static void WrapParam(lua_State* L, std::tuple<Args...>& args)
            {
                if constexpr (std::tuple_size_v<std::tuple<Args...>> > 0)
                    WrapParamAtIndex<0, std::tuple_element<0, std::tuple<Args...>>::type, Args...>(L, args);
            }

            template<class R, class T, class F>
            static void WrapReturn(lua_State* L, T obj, F f)
            {
                if constexpr (std::is_same_v<R, const glm::vec3&>)
                {
                    auto val = (obj->*f)();

                    lua_createtable(L, 3, 0);
                    lua_pushnumber(L, val.x);
                    lua_seti(L, -2, 1);
                    lua_pushnumber(L, val.y);
                    lua_seti(L, -2, 2);
                    lua_pushnumber(L, val.z);
                    lua_seti(L, -2, 3);
                }
                // extend return types here...
                else
                    lua_pushstring(L, "Lua Error: return type not implemented");
            }

            template<auto ff, class T, class R, class... Args>
            auto Wrap(R (T::*f)(Args...))
            {
                return [](lua_State* L) -> int
                {
                    T** ptr = (T**)lua_touserdata(L, 1); // maybe we can wrap the name in a class' template arg which can make this lambda a c comptiable function, so that we can test the data using luaL_checkudata
                    if (ptr != nullptr)
                    {
                        std::tuple<typename std::remove_const<typename std::remove_reference<Args>::type>::type...> args;
                        WrapParam<typename std::remove_const<typename std::remove_reference<Args>::type>::type...>(L, args);
                        std::apply(ff, std::tuple_cat(std::make_tuple(*ptr), args));

                        if constexpr (std::is_void<R>::value)
                        {
                            return 0;
                        }
                        else
                        {
                            WrapReturn<R>(L, *ptr, ff);
                            return 1;
                        }
                    }

                    return 0;
                };
            }

            lua_State* L;
    };
}
