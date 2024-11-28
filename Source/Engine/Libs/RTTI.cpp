#include <iostream>
#include <unordered_map>
#include <functional>
#include <typeinfo>

template<class T>
class RTTI
{
public:
    template<class MemType>
    static void RegisterMemberVariable(const std::string& name, MemType T::* memPtr)
    {
        Singleton().variables[name] = { &typeid(MemType), [memPtr](T& obj, void* val) { *((MemType*)val) = obj.*memPtr; } };
    }

    template<class MemType>
    static bool Get(T& obj, const std::string& name, MemType& val)
    {
        auto iter = Singleton().variables.find(name);

        if (iter == Singleton().variables.end())
        {
            val = MemType();
            return false;
        }

        auto& pair = iter->second;
        auto& typeInfo = pair.first;
        if (typeid(MemType) != *typeInfo)
        {
            val = MemType();
            return false;
        }

        auto& f = pair.second;
        f(obj, &val);

        return true;
    }

    template<class MemType>
    bool Get(const std::string& name, MemType& val)
    {
        return RTTI<T>::Get(*static_cast<T*>(this), name, val);
    }

private:

    static RTTI<T>& Singleton()
    {
        static RTTI<T> instance;
        return instance;
    };

    std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*)>>> variables;
};

class GameObject : public RTTI<GameObject>
{
public:
    [[Serialized]] float x;
    [[Serialized]] float y;
    float z;
};

#define REGISTER_RTTI_MEMBER_VARIABLE(Type, memName) \
    RTTI<Type>::RegisterMemberVariable(#memName, &Type::memName);

int main()
{
    float GameObject::* xx = &GameObject::x;

    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, x);
    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, y);
    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, z);

    GameObject go;
    go.x = 1.0f;
    go.y = 3.0f;
    go.z = 5.0f;

    float x, y, z, t;
    go.Get("x", x);
    go.Get("y", y);
    go.Get("z", z);
    go.Get("t", t);

    std::cout << x << " " << y << " " << z << " " << t << std::endl;
}
