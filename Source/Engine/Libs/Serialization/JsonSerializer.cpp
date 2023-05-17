#include "JsonSerializer.hpp"

namespace Engine
{
void JsonSerializer::Serialize(std::string_view name, const std::string& val) { j[std::string(name)] = val; }
void JsonSerializer::Deserialize(std::string_view name, std::string& val) { val = j[std::string(name)]; }

void JsonSerializer::Serialize(std::string_view name, const UUID& uuid) { j[std::string(name)] = uuid.ToString(); }
void JsonSerializer::Deserialize(std::string_view name, UUID& uuid) { uuid = j[std::string(name)]; }

void JsonSerializer::Serialize(std::string_view name, unsigned char* p, size_t size)
{
    std::vector<std::uint8_t> d(p, p + size);
    j[std::string(name)] = d;
}
void JsonSerializer::Deserialize(std::string_view name, unsigned char* p, size_t size)
{
    std::vector<std::uint8_t> d = j[std::string(name)].get<std::vector<std::uint8_t>>();
    memcpy(p, d.data(), d.size());
}

} // namespace Engine
