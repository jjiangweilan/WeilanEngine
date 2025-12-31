#include "Engine/Core/Object.hpp"
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

TEST(FieldIteratorTest, IterateFields)
{
    auto derived = std::make_unique<IterTestDerived>();
    const ObjectTypeInfo* typeInfo = derived->GetTypeInfo();

    std::vector<std::string> foundFields;
    for (auto it = typeInfo->GetFieldIterator(); it != typeInfo->FieldEnd(); ++it)
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
    EXPECT_EQ(foundFields.size(), 2);
}

TEST(FieldIteratorTest, GetVariable)
{
    auto derived = std::make_unique<IterTestDerived>();
    const ObjectTypeInfo* typeInfo = derived->GetTypeInfo();

    int* val = typeInfo->GetVariable<int>(*derived, "baseVar");

    EXPECT_EQ(*val, 1);
}

TEST(FieldIteratorTest, EmptyIteratorComparison)
{
    ObjectTypeInfo::PropertyIterator it1;
    ObjectTypeInfo::PropertyIterator it2;

    EXPECT_TRUE(it1 == it2);
}
