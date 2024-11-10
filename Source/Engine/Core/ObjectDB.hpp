#pragma once
#include "Libs/Serialization/Serializable.hpp"
#include "Object.hpp"

enum class ObjectDBTag
{
    Save,
    DontSave
};

class ObjectDB : public Serializable
{
public:
    template<class T>
    static T* Create(ObjectDBTag tag = ObjectDBTag::Save)
    {
        Singleton()->CreateImple<T>(tag);
    }

private:
    ObjectDB() {}
    static ObjectDB* Singleton();

    template <IsObject T>
    T* CreateImple(ObjectDBTag tag = ObjectDBTag::Save);

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    struct AllocatedObject : Serializable
    {
        std::unique_ptr<Object> object;
        ObjectDBTag tag;

        void Serialize(Serializer* s) const override;
        void Deserialize(Serializer* s) override;
    };

    std::vector<AllocatedObject> allocatedObjects;
};

template<class T>
T* CreateImple(ObjectDBTag = ObjectDBTag::Save)
{
}
