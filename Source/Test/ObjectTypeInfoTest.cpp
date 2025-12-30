#include <gtest/gtest.h>
#include "Engine/Core/Object.hpp"
#include "Engine/Library/TypeReflection.hpp"

// Test class hierarchy
class TestObjectBase : public Object
{
    DECLARE_OBJECT();
public:
    int baseValue = 10;
    
    int GetBaseValue() { return baseValue; }
    void SetBaseValue(int val) { baseValue = val; }
};

class TestObjectDerived : public TestObjectBase
{
    DECLARE_OBJECT();
public:
    float derivedValue = 3.14f;
    std::string textValue = "test";
    
    float GetDerivedValue() { return derivedValue; }
    void SetDerivedValue(float val) { derivedValue = val; }
};

class TestObjectGrandchild : public TestObjectDerived
{
    DECLARE_OBJECT();
public:
    bool boolValue = true;
    
    bool GetBoolValue() { return boolValue; }
};

// Define the objects with proper inheritance chain
DEFINE_OBJECT(Object, TestObjectBase, "10000000-0000-0000-0000-000000000001");
DEFINE_OBJECT(TestObjectBase, TestObjectDerived, "10000000-0000-0000-0000-000000000002");
DEFINE_OBJECT(TestObjectDerived, TestObjectGrandchild, "10000000-0000-0000-0000-000000000003");

// Register type reflection for TestObjectBase
TYPE_REFLECTION_MEMBER_VARIABLES(TestObjectBase,
    TYPE_REFLECTION_MEM(TestObjectBase, baseValue)
)

TYPE_REFLECTION_MEMBER_FUNCTIONS(TestObjectBase,
    TYPE_REFLECTION_FUNC(TestObjectBase, GetBaseValue),
    TYPE_REFLECTION_FUNC(TestObjectBase, SetBaseValue)
)

// Register type reflection for TestObjectDerived
TYPE_REFLECTION_MEMBER_VARIABLES(TestObjectDerived,
    TYPE_REFLECTION_MEM(TestObjectDerived, derivedValue),
    TYPE_REFLECTION_MEM(TestObjectDerived, textValue)
)

TYPE_REFLECTION_MEMBER_FUNCTIONS(TestObjectDerived,
    TYPE_REFLECTION_FUNC(TestObjectDerived, GetDerivedValue),
    TYPE_REFLECTION_FUNC(TestObjectDerived, SetDerivedValue)
)

// Register type reflection for TestObjectGrandchild
TYPE_REFLECTION_MEMBER_VARIABLES(TestObjectGrandchild,
    TYPE_REFLECTION_MEM(TestObjectGrandchild, boolValue)
)

TYPE_REFLECTION_MEMBER_FUNCTIONS(TestObjectGrandchild,
    TYPE_REFLECTION_FUNC(TestObjectGrandchild, GetBoolValue)
)

class ObjectTypeInfoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        baseObj = std::make_unique<TestObjectBase>();
        derivedObj = std::make_unique<TestObjectDerived>();
        grandchildObj = std::make_unique<TestObjectGrandchild>();
    }

    std::unique_ptr<TestObjectBase> baseObj;
    std::unique_ptr<TestObjectDerived> derivedObj;
    std::unique_ptr<TestObjectGrandchild> grandchildObj;
};

// Test ObjectTypeInfo basic properties
TEST_F(ObjectTypeInfoTest, BasicTypeInfo)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    
    ASSERT_NE(typeInfo, nullptr);
    EXPECT_EQ(typeInfo->GetTypeName(), "TestObjectBase");
    EXPECT_EQ(typeInfo->GetTypeID(), UUID("10000000-0000-0000-0000-000000000001"));
}

// Test parent type info chain
TEST_F(ObjectTypeInfoTest, ParentTypeInfoChain)
{
    const ObjectTypeInfo* baseTypeInfo = baseObj->GetTypeInfo();
    const ObjectTypeInfo* derivedTypeInfo = derivedObj->GetTypeInfo();
    const ObjectTypeInfo* grandchildTypeInfo = grandchildObj->GetTypeInfo();
    
    ASSERT_NE(baseTypeInfo, nullptr);
    ASSERT_NE(derivedTypeInfo, nullptr);
    ASSERT_NE(grandchildTypeInfo, nullptr);
    
    // Check base object parent (should be Object)
    const ObjectTypeInfo* baseParent = baseTypeInfo->GetParentTypeInfo();
    ASSERT_NE(baseParent, nullptr);
    EXPECT_EQ(baseParent->GetTypeName(), "Object");
    
    // Check derived object parent (should be TestObjectBase)
    const ObjectTypeInfo* derivedParent = derivedTypeInfo->GetParentTypeInfo();
    ASSERT_NE(derivedParent, nullptr);
    EXPECT_EQ(derivedParent->GetTypeName(), "TestObjectBase");
    EXPECT_EQ(derivedParent->GetTypeID(), baseTypeInfo->GetTypeID());
    
    // Check grandchild object parent (should be TestObjectDerived)
    const ObjectTypeInfo* grandchildParent = grandchildTypeInfo->GetParentTypeInfo();
    ASSERT_NE(grandchildParent, nullptr);
    EXPECT_EQ(grandchildParent->GetTypeName(), "TestObjectDerived");
    EXPECT_EQ(grandchildParent->GetTypeID(), derivedTypeInfo->GetTypeID());
}

// Test TypeReflection availability through ObjectTypeInfo
TEST_F(ObjectTypeInfoTest, TypeReflectionAvailability)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ASSERT_NE(typeInfo, nullptr);
    
    const ITypeReflection* reflection = typeInfo->GetTypeReflection();
    ASSERT_NE(reflection, nullptr);
}

// Test TypeReflection GetVariables
TEST_F(ObjectTypeInfoTest, TypeReflectionGetVariables)
{
    const ObjectTypeInfo* baseTypeInfo = baseObj->GetTypeInfo();
    const ITypeReflection* baseReflection = baseTypeInfo->GetTypeReflection();
    
    const auto& baseVars = baseReflection->GetVariables();
    EXPECT_NE(baseVars.find("baseValue"), baseVars.end());
    
    const ObjectTypeInfo* derivedTypeInfo = derivedObj->GetTypeInfo();
    const ITypeReflection* derivedReflection = derivedTypeInfo->GetTypeReflection();
    
    const auto& derivedVars = derivedReflection->GetVariables();
    EXPECT_NE(derivedVars.find("derivedValue"), derivedVars.end());
    EXPECT_NE(derivedVars.find("textValue"), derivedVars.end());
}

// Test TypeReflection GetVariable through interface
TEST_F(ObjectTypeInfoTest, TypeReflectionGetVariable)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    void* valuePtr = reflection->GetVariable(*baseObj, "baseValue");
    ASSERT_NE(valuePtr, nullptr);
    
    int* intPtr = static_cast<int*>(valuePtr);
    EXPECT_EQ(*intPtr, 10);
    
    // Modify through pointer
    *intPtr = 42;
    EXPECT_EQ(baseObj->baseValue, 42);
}

// Test TypeReflection GetVariable on derived object
TEST_F(ObjectTypeInfoTest, TypeReflectionGetVariableDerived)
{
    const ObjectTypeInfo* typeInfo = derivedObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    void* floatPtr = reflection->GetVariable(*derivedObj, "derivedValue");
    ASSERT_NE(floatPtr, nullptr);
    EXPECT_FLOAT_EQ(*static_cast<float*>(floatPtr), 3.14f);
    
    void* strPtr = reflection->GetVariable(*derivedObj, "textValue");
    ASSERT_NE(strPtr, nullptr);
    EXPECT_EQ(*static_cast<std::string*>(strPtr), "test");
}

// Test TypeReflection CallFunction through interface
TEST_F(ObjectTypeInfoTest, TypeReflectionCallFunction)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    // Call SetBaseValue(55)
    int arg = 55;
    void* argPtrs[] = {&arg};
    reflection->CallFunction(*baseObj, "SetBaseValue", nullptr, argPtrs, 1);
    
    EXPECT_EQ(baseObj->baseValue, 55);
    
    // Call GetBaseValue()
    int result = 0;
    reflection->CallFunction(*baseObj, "GetBaseValue", &result, nullptr, 0);
    EXPECT_EQ(result, 55);
}

// Test TypeReflection CallFunction on derived object
TEST_F(ObjectTypeInfoTest, TypeReflectionCallFunctionDerived)
{
    const ObjectTypeInfo* typeInfo = derivedObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    // Call SetDerivedValue(2.71f)
    float arg = 2.71f;
    void* argPtrs[] = {&arg};
    reflection->CallFunction(*derivedObj, "SetDerivedValue", nullptr, argPtrs, 1);
    
    EXPECT_FLOAT_EQ(derivedObj->derivedValue, 2.71f);
    
    // Call GetDerivedValue()
    float result = 0.0f;
    reflection->CallFunction(*derivedObj, "GetDerivedValue", &result, nullptr, 0);
    EXPECT_FLOAT_EQ(result, 2.71f);
}

// Test TypeReflection Copy through interface
TEST_F(ObjectTypeInfoTest, TypeReflectionCopy)
{
    baseObj->baseValue = 123;
    
    auto destObj = std::make_unique<TestObjectBase>();
    destObj->baseValue = 0;
    
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    reflection->Copy(baseObj.get(), destObj.get());
    
    EXPECT_EQ(destObj->baseValue, 123);
}

// Test TypeReflection Copy on derived objects
TEST_F(ObjectTypeInfoTest, TypeReflectionCopyDerived)
{
    derivedObj->derivedValue = 9.99f;
    derivedObj->textValue = "modified";
    
    auto destObj = std::make_unique<TestObjectDerived>();
    destObj->derivedValue = 0.0f;
    destObj->textValue = "";
    
    const ObjectTypeInfo* typeInfo = derivedObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    reflection->Copy(derivedObj.get(), destObj.get());
    
    EXPECT_FLOAT_EQ(destObj->derivedValue, 9.99f);
    EXPECT_EQ(destObj->textValue, "modified");
}

// Test parent TypeReflection access
TEST_F(ObjectTypeInfoTest, ParentTypeReflection)
{
    const ObjectTypeInfo* derivedTypeInfo = derivedObj->GetTypeInfo();
    const ObjectTypeInfo* baseTypeInfo = derivedTypeInfo->GetParentTypeInfo();
    
    ASSERT_NE(baseTypeInfo, nullptr);
    
    const ITypeReflection* baseReflection = baseTypeInfo->GetTypeReflection();
    ASSERT_NE(baseReflection, nullptr);
    
    // Access base class variables through parent type reflection
    const auto& baseVars = baseReflection->GetVariables();
    EXPECT_NE(baseVars.find("baseValue"), baseVars.end());
}

// Test complete inheritance chain TypeReflection
TEST_F(ObjectTypeInfoTest, CompleteInheritanceChainReflection)
{
    const ObjectTypeInfo* grandchildTypeInfo = grandchildObj->GetTypeInfo();
    ASSERT_NE(grandchildTypeInfo, nullptr);
    EXPECT_EQ(grandchildTypeInfo->GetTypeName(), "TestObjectGrandchild");
    
    // Get parent (TestObjectDerived)
    const ObjectTypeInfo* parentTypeInfo = grandchildTypeInfo->GetParentTypeInfo();
    ASSERT_NE(parentTypeInfo, nullptr);
    EXPECT_EQ(parentTypeInfo->GetTypeName(), "TestObjectDerived");
    
    // Get grandparent (TestObjectBase)
    const ObjectTypeInfo* grandparentTypeInfo = parentTypeInfo->GetParentTypeInfo();
    ASSERT_NE(grandparentTypeInfo, nullptr);
    EXPECT_EQ(grandparentTypeInfo->GetTypeName(), "TestObjectBase");
    
    // Get great-grandparent (Object)
    const ObjectTypeInfo* rootTypeInfo = grandparentTypeInfo->GetParentTypeInfo();
    ASSERT_NE(rootTypeInfo, nullptr);
    EXPECT_EQ(rootTypeInfo->GetTypeName(), "Object");
    
    // Verify all have type reflections
    ASSERT_NE(grandchildTypeInfo->GetTypeReflection(), nullptr);
    ASSERT_NE(parentTypeInfo->GetTypeReflection(), nullptr);
    ASSERT_NE(grandparentTypeInfo->GetTypeReflection(), nullptr);
    ASSERT_NE(rootTypeInfo->GetTypeReflection(), nullptr);
}

// Test CreateInstance through ObjectTypeInfo
TEST_F(ObjectTypeInfoTest, CreateInstance)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ASSERT_NE(typeInfo, nullptr);
    
    std::unique_ptr<Object> newObj = typeInfo->CreateInstance();
    ASSERT_NE(newObj, nullptr);
    
    // Verify it's the correct type
    TestObjectBase* testObj = dynamic_cast<TestObjectBase*>(newObj.get());
    ASSERT_NE(testObj, nullptr);
    EXPECT_EQ(testObj->baseValue, 10); // Default value
}

// Test ObjectRegistry::GetObjectTypeInfo
TEST_F(ObjectTypeInfoTest, GetObjectTypeInfoFromRegistry)
{
    const UUID& typeID = TestObjectBase::StaticGetObjectTypeID();
    const ObjectTypeInfo* typeInfo = ObjectRegistry::GetObjectTypeInfo(typeID);
    
    ASSERT_NE(typeInfo, nullptr);
    EXPECT_EQ(typeInfo->GetTypeID(), typeID);
    EXPECT_EQ(typeInfo->GetTypeName(), "TestObjectBase");
}

// Test accessing parent reflection and performing operations
TEST_F(ObjectTypeInfoTest, AccessParentReflectionAndOperate)
{
    // Set up derived object with base value
    derivedObj->baseValue = 77;
    derivedObj->derivedValue = 8.88f;
    
    // Get derived type info
    const ObjectTypeInfo* derivedTypeInfo = derivedObj->GetTypeInfo();
    
    // Get parent type info
    const ObjectTypeInfo* parentTypeInfo = derivedTypeInfo->GetParentTypeInfo();
    ASSERT_NE(parentTypeInfo, nullptr);
    
    // Get parent type reflection
    ITypeReflection* parentReflection = const_cast<ITypeReflection*>(parentTypeInfo->GetTypeReflection());
    ASSERT_NE(parentReflection, nullptr);
    
    // Access base class variable through parent reflection
    // Note: This casts derivedObj to Object* since the interface expects Object&
    void* baseValuePtr = parentReflection->GetVariable(*static_cast<Object*>(derivedObj.get()), "baseValue");
    ASSERT_NE(baseValuePtr, nullptr);
    EXPECT_EQ(*static_cast<int*>(baseValuePtr), 77);
    
    // Modify through parent reflection
    *static_cast<int*>(baseValuePtr) = 999;
    EXPECT_EQ(derivedObj->baseValue, 999);
}

// Test null handling for GetVariable
TEST_F(ObjectTypeInfoTest, GetVariableNullHandling)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    void* result = reflection->GetVariable(*baseObj, "nonExistentVariable");
    EXPECT_EQ(result, nullptr);
}

// Test CallFunction with non-existent function
TEST_F(ObjectTypeInfoTest, CallFunctionNonExistent)
{
    const ObjectTypeInfo* typeInfo = baseObj->GetTypeInfo();
    ITypeReflection* reflection = const_cast<ITypeReflection*>(typeInfo->GetTypeReflection());
    
    int result = 0;
    // Should not crash, just do nothing
    reflection->CallFunction(*baseObj, "nonExistentFunction", &result, nullptr, 0);
    
    EXPECT_EQ(result, 0); // Should remain unchanged
}
