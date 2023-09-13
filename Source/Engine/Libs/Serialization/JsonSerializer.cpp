#include "JsonSerializer.hpp"

namespace Engine
{
#define TO_JSON_PTR(x) nlohmann::json::json_pointer(fmt::format("{}{}", "/", x))
void JsonSerializer::Serialize(std::string_view name, const std::string& val)
{
    j[TO_JSON_PTR(name)] = val;
}
void JsonSerializer::Deserialize(std::string_view name, std::string& val)
{
    val = j[TO_JSON_PTR(name)];
}

void JsonSerializer::Serialize(std::string_view name, const UUID& uuid)
{
    j[TO_JSON_PTR(name)] = uuid.ToString();
}
void JsonSerializer::Deserialize(std::string_view name, UUID& uuid)
{
    uuid = j[TO_JSON_PTR(name)];
}

std::unique_ptr<Serializer> JsonSerializer::CreateSubserializer()
{
    return std::make_unique<JsonSerializer>();
}

std::unique_ptr<Serializer> JsonSerializer::CreateSubdeserializer(std::string_view name)
{
    return std::make_unique<JsonSerializer>(j[TO_JSON_PTR(name)], resolveCallbacks);
}

void JsonSerializer::AppendSubserializer(std::string_view name, Serializer* s)
{
    j[TO_JSON_PTR(name)] = ((JsonSerializer*)s)->j;
}

void JsonSerializer::Serialize(std::string_view name, unsigned char* p, size_t size)
{
    std::vector<std::uint8_t> d(p, p + size);
    j[TO_JSON_PTR(name)] = d;
}
void JsonSerializer::Deserialize(std::string_view name, unsigned char* p, size_t size)
{
    std::vector<std::uint8_t> d = j[TO_JSON_PTR(name)].get<std::vector<std::uint8_t>>();
    memcpy(p, d.data(), d.size());
}

} // namespace Engine
