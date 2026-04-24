
#pragma once
#include "Serializable.hpp"
#include "Serializer.hpp"

class SerializationSequenceFetcher : public Serializer
{
    std::vector<std::string> keySequence;

public:
    const std::vector<std::string>& GetKeySequence() { return keySequence; }

    void Serialize(std::string_view name, const std::string& val) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, std::string& val) override {}

    void Serialize(std::string_view name, const nlohmann::json& json) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, nlohmann::json& json) override {}

    void Serialize(std::string_view name, const bool val) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, bool& val) override {}

    void Serialize(std::string_view name, const UUID& uuid) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, UUID& uuid) override {}

    void Serialize(std::string_view name, const uint32_t& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, uint32_t& v) override {}

    void Serialize(std::string_view name, const int32_t& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, int32_t& v) override {}

    void Serialize(std::string_view name, const uint64_t& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, uint64_t& v) override {}

    void Serialize(std::string_view name, const int64_t& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, int64_t& v) override {}

    void Serialize(std::string_view name, const float& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, float& v) override {}

    void Serialize(std::string_view name, const glm::mat4& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, glm::mat4& v) override {}

    void Serialize(std::string_view name, const glm::quat& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, glm::quat& v) override {}

    void Serialize(std::string_view name, const glm::vec4& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, glm::vec4& v) override {}

    void Serialize(std::string_view name, const glm::vec3& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, glm::vec3& v) override {}

    void Serialize(std::string_view name, const glm::vec2& v) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, glm::vec2& v) override {}

    void Serialize(std::string_view name, nullptr_t) override { keySequence.push_back(std::string(name)); }
    bool IsNull(std::string_view name) override { return true; }
    bool IsNull() override { return true; }

    std::vector<uint8_t> GetBinary() override { return {}; }

    std::unique_ptr<Serializer> CreateSubserializer() override { return std::make_unique<SerializationSequenceFetcher>(); }
    std::unique_ptr<Serializer> CreateSubdeserializer(std::string_view name) override { return std::make_unique<SerializationSequenceFetcher>(); }
    void AppendSubserializer(std::string_view name, Serializer* s) override { keySequence.push_back(std::string(name)); }

protected:
    void Serialize(std::string_view name, unsigned char* p, size_t size) override { keySequence.push_back(std::string(name)); }
    void Deserialize(std::string_view name, unsigned char* p, size_t size) override {}

    size_t GetArraySize(std::string_view name) override { return 0; }

    const nlohmann::json& GetJsonObject(std::string_view name) override
    {
        static nlohmann::json j;
        return j;
    }
};
