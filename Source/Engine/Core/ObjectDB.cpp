#include "ObjectDB.hpp"
#include "Libs/Serialization/Serializer.hpp"

void ObjectDB::Serialize(Serializer* s) const
{

    std::vector<std::unique_ptr<Object>> objects;

    for (int i = 0; i < allocatedObjects.size(); ++i)
    {
        auto& obj = allocatedObjects[i];
        if (obj.tag == ObjectDBTag::Save)
        {}
    }
}
void ObjectDB::Deserialize(Serializer* s) {}

void ObjectDB::AllocatedObject::Serialize(Serializer* s) const
{
    if (tag == ObjectDBTag::Save)
    {
        s->Serialize("object", object);
    }
}
void ObjectDB::AllocatedObject::Deserialize(Serializer* s)
{
    s->Deserialize("object", object);
    tag = ObjectDBTag::Save;
}

static ObjectDB* Singleton()
{
    static std::unique_ptr<ObjectDB> db;
    if (db == nullptr)
    {
        db = std::make_unique<ObjectDB>();
    }
    return db.get();
}
