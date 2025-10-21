
#pragma once
#include "Serializable.hpp"
#include "Serializer.hpp"

class SerializationSequenceFetcher : public Serializer
{
    std::vector<std::string> keySequence;

public:
    virtual void Serialize(std::string_view name, const std::string& val) = 0;
    virtual void Deserialize(std::string_view name, std::string& val) = 0;

    virtual void Serialize(std::string_view name, const nlohmann::json& json) = 0;
    virtual void Deserialize(std::string_view name, nlohmann::json& json) = 0;

    virtual void Serialize(std::string_view name, const bool val) = 0;
    virtual void Deserialize(std::string_view name, bool& val) = 0;

    virtual void Serialize(std::string_view name, const UUID& uuid) = 0;
    virtual void Deserialize(std::string_view name, UUID& uuid) = 0;

    virtual void Serialize(std::string_view name, const uint32_t& v) = 0;
    virtual void Deserialize(std::string_view name, uint32_t& v) = 0;

    virtual void Serialize(std::string_view name, const int32_t& v) = 0;
    virtual void Deserialize(std::string_view name, int32_t& v) = 0;

    virtual void Serialize(std::string_view name, const uint64_t& v) = 0;
    virtual void Deserialize(std::string_view name, uint64_t& v) = 0;

    virtual void Serialize(std::string_view name, const int64_t& v) = 0;
    virtual void Deserialize(std::string_view name, int64_t& v) = 0;

    virtual void Serialize(std::string_view name, const float& v) = 0;
    virtual void Deserialize(std::string_view name, float& v) = 0;

    virtual void Serialize(std::string_view name, const glm::mat4& v) = 0;
    virtual void Deserialize(std::string_view name, glm::mat4& v) = 0;

    virtual void Serialize(std::string_view name, const glm::quat& v) = 0;
    virtual void Deserialize(std::string_view name, glm::quat& v) = 0;

    virtual void Serialize(std::string_view name, const glm::vec4& v) = 0;
    virtual void Deserialize(std::string_view name, glm::vec4& v) = 0;

    virtual void Serialize(std::string_view name, const glm::vec3& v) = 0;
    virtual void Deserialize(std::string_view name, glm::vec3& v) = 0;

    virtual void Serialize(std::string_view name, const glm::vec2& v) = 0;
    virtual void Deserialize(std::string_view name, glm::vec2& v) = 0;

    virtual void Serialize(std::string_view name, nullptr_t) = 0;
    virtual bool IsNull(std::string_view name) = 0;
    virtual bool IsNull() = 0;

    virtual std::vector<uint8_t> GetBinary() = 0;

    virtual std::unique_ptr<Serializer> CreateSubserializer() = 0;
    virtual std::unique_ptr<Serializer> CreateSubdeserializer(std::string_view name) = 0;
    virtual void AppendSubserializer(std::string_view name, Serializer* s) = 0;
};
