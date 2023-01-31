#include "UUID.hpp"
#include <cassert>
#include <random>
namespace Engine
{
UUID::UUID()
{
    id = uuids::uuid_random_generator{generator}();
    strID = uuids::to_string(id);
}

UUID::UUID(const UUID& uuid) : id(uuid.id), strID(uuid.strID) {}

UUID::UUID(const std::string& uuid)
{
    auto optionalUUID = uuids::uuid::from_string(uuid);
    if (optionalUUID.has_value())
    {
        id = optionalUUID.value();
        strID = uuid;
    }
    else
    {
        id = uuids::uuid();
        strID = uuids::to_string(id);
    }
}

UUID::~UUID() {}

bool UUID::IsEmpty() const { return id.is_nil(); }

const std::mt19937 CreateGenerator()
{
    std::random_device rd;
    auto seed_data = std::array<int, std::mt19937::state_size>{};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937 generator(seq);

    return generator;
}

const std::string& UUID::ToString() const { return strID; }

UUID::UUID(bool empty) : id() {}

const UUID& UUID::operator=(const UUID& other)
{
    id = other.id;
    strID = other.strID;

    return *this;
}

const UUID UUID::empty = UUID(true);

std::mt19937 UUID::generator = CreateGenerator();
} // namespace Engine