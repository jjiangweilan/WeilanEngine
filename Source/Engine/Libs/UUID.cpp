#include "UUID.hpp"
#include <cassert>
#include <random>
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

UUID::UUID(const char* uuid) : UUID(std::string(uuid)) {}

UUID::~UUID() {}

bool UUID::IsEmpty() const
{
    return id.is_nil();
}

UUID::UUID(const std::string& str, FromStrTag) : id(nameGenerator(str)), strID(uuids::to_string(id)) {}

const std::mt19937 CreateGenerator()
{
    std::random_device rd;
    auto seed_data = std::array<int, std::mt19937::state_size>{};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937 generator(seq);

    return generator;
}

const std::string& UUID::ToString() const
{
    return strID;
}

UUID::UUID(UUID::EmptyTag) : id() {}

const UUID& UUID::operator=(const UUID& other)
{
    id = other.id;
    strID = other.strID;

    return *this;
}

const UUID& UUID::GetEmptyUUID()
{
    static const UUID empty = UUID(EmptyTag{});
    return empty;
}

std::mt19937 UUID::generator = CreateGenerator();

uuids::uuid_name_generator UUID::nameGenerator =
    uuids::uuid_name_generator(uuids::uuid::from_string("73B6D45A-5A1A-42D7-B75C-7C39F976A620").value());
