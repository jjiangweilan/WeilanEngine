#include "Engine/Core/Object.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include <gtest/gtest.h>

class IterTestBase : public Object
{
    DECLARE_OBJECT();

public:
    int baseVar = 1;
};

class IterTestDerived : public IterTestBase
{
    DECLARE_OBJECT();

public:
    float derivedVar = 2.0f;
};

DEFINE_OBJECT(Object, IterTestBase, "20000000-0000-0000-0000-000000000001");
DEFINE_OBJECT(IterTestBase, IterTestDerived, "20000000-0000-0000-0000-000000000002");

TYPE_REFLECTION_MEMBER_VARIABLES(IterTestBase, TYPE_REFLECTION_MEM(IterTestBase, baseVar))

TYPE_REFLECTION_MEMBER_VARIABLES(IterTestDerived, TYPE_REFLECTION_MEM(IterTestDerived, derivedVar))

TEST(ObjectInfoTest, IterateFields)
{
    auto derived = std::make_unique<IterTestDerived>();
    const ObjectTypeInfo* typeInfo = derived->GetTypeInfo();

    std::vector<std::string> foundFields;
    for (auto it = typeInfo->GetVariables(); it; ++it)
    {
        foundFields.push_back(it->first);
    }

    // Expect "baseVar" and "derivedVar"
    bool foundBase = false;
    bool foundDerived = false;
    for (const auto& name : foundFields)
    {
        if (name == "baseVar")
            foundBase = true;
        if (name == "derivedVar")
            foundDerived = true;
    }

    EXPECT_TRUE(foundBase) << "baseVar not found";
    EXPECT_TRUE(foundDerived) << "derivedVar not found";
    EXPECT_EQ(foundFields.size(), 4);
}

TEST(ObjectInfoTest, GetVariable)
{
    auto derived = std::make_unique<IterTestDerived>();
    const ObjectTypeInfo* typeInfo = derived->GetTypeInfo();

    int* val = typeInfo->GetVariable<int>(*derived, "baseVar");

    EXPECT_EQ(*val, 1);
}

TEST(ObjectInfoTest, Serialization)
{
    auto derived = std::make_unique<IterTestDerived>();

    auto typeInfo = derived->GetTypeInfo();
    JsonSerializer s;
    typeInfo->Serialize(*derived, s);

    auto& j = s.GetJson();
    EXPECT_EQ(j["baseVar"], 1);
    EXPECT_EQ(j["derivedVar"], 2.0f);
}

TEST(ObjectInfoTest, Deserialization)
{
    nlohmann::json j;
    j["baseVar"] = 10;
    j["derivedVar"] = 20.0f;

    JsonSerializer s(j);
    auto derived = std::make_unique<IterTestDerived>();
    auto typeInfo = derived->GetTypeInfo();

    typeInfo->Deserialize(*derived, s);

    EXPECT_EQ(derived->baseVar, 10);
    EXPECT_EQ(derived->derivedVar, 20.0f);
}

TEST(ObjectInfoTest, EmptyIteratorComparison)
{
    ObjectTypeInfo::PropertyIterator it1;
    ObjectTypeInfo::PropertyIterator it2;

    EXPECT_TRUE(it1 == it2);
}
