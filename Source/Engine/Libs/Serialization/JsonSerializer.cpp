#include "JsonSerializer.hpp"

#define TO_JSON_PTR(x) nlohmann::json::json_pointer(fmt::format("{}{}", "/", x))
void JsonSerializer::Serialize(std::string_view name, const std::string& val)
{
    j[TO_JSON_PTR(name)] = val;
}
void JsonSerializer::Deserialize(std::string_view name, std::string& val)
{
    val = j.value(TO_JSON_PTR(name), "");
}

void JsonSerializer::Serialize(std::string_view name, const UUID& uuid)
{
    j[TO_JSON_PTR(name)] = uuid.ToString();
}
void JsonSerializer::Deserialize(std::string_view name, UUID& uuid)
{
    uuid = j.value(TO_JSON_PTR(name), UUID::GetEmptyUUID());
}

std::unique_ptr<Serializer> JsonSerializer::CreateSubserializer()
{
    return std::make_unique<JsonSerializer>();
}

std::unique_ptr<Serializer> JsonSerializer::CreateSubdeserializer(std::string_view name)
{
    auto ser = std::make_unique<JsonSerializer>();
    ser->j = j[TO_JSON_PTR(name)];
    ser->resolveCallbacks = resolveCallbacks;
    return ser;
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

void JsonSerializer::Serialize(std::string_view name, const uint32_t& v)
{
    j[TO_JSON_PTR(name)] = v;
}

void JsonSerializer::Deserialize(std::string_view name, uint32_t& v)
{
    v = j.value(TO_JSON_PTR(name), (uint32_t)0);
}

void JsonSerializer::Serialize(std::string_view name, const int32_t& v)
{
    j[TO_JSON_PTR(name)] = v;
}

void JsonSerializer::Deserialize(std::string_view name, int32_t& v)
{
    v = j.value(TO_JSON_PTR(name), (int32_t)0);
}

void JsonSerializer::Serialize(std::string_view name, const uint64_t& v)
{
    j[TO_JSON_PTR(name)] = v;
}
void JsonSerializer::Deserialize(std::string_view name, uint64_t& v)
{
    v = j.value(TO_JSON_PTR(name), (uint64_t)0);
}

void JsonSerializer::Serialize(std::string_view name, const int64_t& v)
{
    j[TO_JSON_PTR(name)] = v;
}
void JsonSerializer::Deserialize(std::string_view name, int64_t& v)
{
    v = j.value(TO_JSON_PTR(name), (int64_t)0);
}

void JsonSerializer::Serialize(std::string_view name, const float& v)
{
    j[TO_JSON_PTR(name)] = v;
}

void JsonSerializer::Deserialize(std::string_view name, float& v)
{
    v = j.value(TO_JSON_PTR(name), 0.0f);
}

void JsonSerializer::Serialize(std::string_view name, const glm::mat4& v)
{
    float c0[] = {v[0].x, v[0].y, v[0].z, v[0].w};
    float c1[] = {v[1].x, v[1].y, v[1].z, v[1].w};
    float c2[] = {v[2].x, v[2].y, v[2].z, v[2].w};
    float c3[] = {v[3].x, v[3].y, v[3].z, v[3].w};

    j[TO_JSON_PTR(name)] = {};
    j[TO_JSON_PTR(name)][0] = c0;
    j[TO_JSON_PTR(name)][1] = c1;
    j[TO_JSON_PTR(name)][2] = c2;
    j[TO_JSON_PTR(name)][3] = c3;
}

void JsonSerializer::Deserialize(std::string_view name, glm::mat4& v)
{
    nlohmann::json jc0 = j[TO_JSON_PTR(name)][0];
    nlohmann::json jc1 = j[TO_JSON_PTR(name)][1];
    nlohmann::json jc2 = j[TO_JSON_PTR(name)][2];
    nlohmann::json jc3 = j[TO_JSON_PTR(name)][3];

    float c0[4] = {jc0[0], jc0[1], jc0[2], jc0[3]};
    float c1[4] = {jc1[0], jc1[1], jc1[2], jc1[3]};
    float c2[4] = {jc2[0], jc2[1], jc2[2], jc2[3]};
    float c3[4] = {jc3[0], jc3[1], jc3[2], jc3[3]};

    v[0] = {c0[0], c0[1], c0[2], c0[3]};
    v[1] = {c1[0], c1[1], c1[2], c1[3]};
    v[2] = {c2[0], c2[1], c2[2], c2[3]};
    v[3] = {c3[0], c3[1], c3[2], c3[3]};
}

void JsonSerializer::Serialize(std::string_view name, const glm::quat& v)
{
    nlohmann::json::array_t jq = {0, 0, 0, 0};
    jq[0] = v.w;
    jq[1] = v.x;
    jq[2] = v.y;
    jq[3] = v.z;

    j[TO_JSON_PTR(name)] = jq;
}

void JsonSerializer::Deserialize(std::string_view name, glm::quat& v)
{
    const auto& jq = j[TO_JSON_PTR(name)];

    if (jq.is_array() && jq.size() >= 4)
    {
        v.w = jq[0];
        v.x = jq[1];
        v.y = jq[2];
        v.z = jq[3];
    }
}

void JsonSerializer::Serialize(std::string_view name, const glm::vec4& v)
{
    nlohmann::json::array_t jv = {0, 0, 0, 0};
    jv[0] = v.x;
    jv[1] = v.y;
    jv[2] = v.z;
    jv[3] = v.w;

    j[TO_JSON_PTR(name)] = jv;
}

void JsonSerializer::Deserialize(std::string_view name, glm::vec4& v)
{
    const auto& jq = j[TO_JSON_PTR(name)];

    if (jq.is_array() && jq.size() >= 4)
    {
        v.x = jq[0];
        v.y = jq[1];
        v.z = jq[2];
        v.w = jq[3];
    }
}

void JsonSerializer::Serialize(std::string_view name, const glm::vec3& v)
{
    nlohmann::json jv = {0, 0, 0};
    jv[0] = v.x;
    jv[1] = v.y;
    jv[2] = v.z;

    j[TO_JSON_PTR(name)] = jv;
}

void JsonSerializer::Deserialize(std::string_view name, glm::vec3& v)
{
    const auto& jq = j[TO_JSON_PTR(name)];

    if (jq.is_array() && jq.size() >= 3)
    {
        v.x = jq[0];
        v.y = jq[1];
        v.z = jq[2];
    }
}

void JsonSerializer::Serialize(std::string_view name, const glm::vec2& v)
{
    nlohmann::json jv = {0, 0};
    jv[0] = v.x;
    jv[1] = v.y;

    j[TO_JSON_PTR(name)] = jv;
}

void JsonSerializer::Deserialize(std::string_view name, glm::vec2& v)
{
    const auto& jq = j[TO_JSON_PTR(name)];

    if (jq.is_array() && jq.size() >= 2)
    {
        v.x = jq[0];
        v.y = jq[1];
    }
}

void JsonSerializer::Serialize(std::string_view name, const bool val)
{
    j[TO_JSON_PTR(name)] = val;
}

void JsonSerializer::Deserialize(std::string_view name, bool& val)
{
    val = j.value(TO_JSON_PTR(name), false);
}

void JsonSerializer::Serialize(std::string_view name, nullptr_t)
{
    j[TO_JSON_PTR(name)] = nullptr;
}

bool JsonSerializer::IsNull(std::string_view name)
{
    return j[TO_JSON_PTR(name)].is_null();
}

bool JsonSerializer::IsNull()
{
    return j.is_null();
}
