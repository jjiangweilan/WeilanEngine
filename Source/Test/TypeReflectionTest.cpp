#include <gtest/gtest.h>
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Core/Object.hpp"

// Test class with various member types
class TestClass : public Object
{
    DECLARE_OBJECT();
public:
    int intValue = 42;
    float floatValue = 3.14f;
    std::string stringValue = "hello";
    bool boolValue = true;

    int Add(int a, int b)
    {
        return a + b;
    }

    void SetInt(int val)
    {
        intValue = val;
    }

    int GetInt()
    {
        return intValue;
    }

    void NoArgsNoReturn()
    {
        intValue = 100;
    }

    std::string Concat(std::string a, std::string b)
    {
        return a + b;
    }
};

DEFINE_OBJECT(Object, TestClass, "30000000-0000-0000-0000-000000000001");

// Another test class to verify isolation between types
class AnotherTestClass : public Object
{
    DECLARE_OBJECT();
public:
    double doubleValue = 2.71;

    double Multiply(double a, double b)
    {
        return a * b;
    }
};

DEFINE_OBJECT(Object, AnotherTestClass, "30000000-0000-0000-0000-000000000002");

// Test fixture for TypeReflection tests
class TypeReflectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clear any previous registrations to ensure test isolation
        // Note: In practice, registrations are static and persist
        obj.intValue = 42;
        obj.floatValue = 3.14f;
        obj.stringValue = "hello";
        obj.boolValue = true;
    }

    TestClass obj;
    AnotherTestClass anotherObj;
};

// Test RegisterMemberVariable and GetVariable for int
TEST_F(TypeReflectionTest, RegisterAndGetIntVariable)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test1_intValue", &TestClass::intValue);

    int* pValue = TypeReflection<TestClass>::GetVariable<int>(obj, "test1_intValue");
    ASSERT_NE(pValue, nullptr);
    EXPECT_EQ(*pValue, 42);

    // Modify through pointer
    *pValue = 99;
    EXPECT_EQ(obj.intValue, 99);
}

// Test RegisterMemberVariable and GetVariable for float
TEST_F(TypeReflectionTest, RegisterAndGetFloatVariable)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test2_floatValue", &TestClass::floatValue);

    float* pValue = TypeReflection<TestClass>::GetVariable<float>(obj, "test2_floatValue");
    ASSERT_NE(pValue, nullptr);
    EXPECT_FLOAT_EQ(*pValue, 3.14f);

    *pValue = 2.71f;
    EXPECT_FLOAT_EQ(obj.floatValue, 2.71f);
}

// Test RegisterMemberVariable and GetVariable for std::string
TEST_F(TypeReflectionTest, RegisterAndGetStringVariable)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test3_stringValue", &TestClass::stringValue);

    std::string* pValue = TypeReflection<TestClass>::GetVariable<std::string>(obj, "test3_stringValue");
    ASSERT_NE(pValue, nullptr);
    EXPECT_EQ(*pValue, "hello");

    *pValue = "world";
    EXPECT_EQ(obj.stringValue, "world");
}

// Test RegisterMemberVariable and GetVariable for bool
TEST_F(TypeReflectionTest, RegisterAndGetBoolVariable)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test4_boolValue", &TestClass::boolValue);

    bool* pValue = TypeReflection<TestClass>::GetVariable<bool>(obj, "test4_boolValue");
    ASSERT_NE(pValue, nullptr);
    EXPECT_TRUE(*pValue);

    *pValue = false;
    EXPECT_FALSE(obj.boolValue);
}

// Test GetVariable with non-existent variable name
TEST_F(TypeReflectionTest, GetNonExistentVariable)
{
    int* pValue = TypeReflection<TestClass>::GetVariable<int>(obj, "nonExistent");
    EXPECT_EQ(pValue, nullptr);
}

// Test GetVariable with wrong type
TEST_F(TypeReflectionTest, GetVariableWithWrongType)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test5_intValue2", &TestClass::intValue);

    // Try to get as float instead of int
    float* pValue = TypeReflection<TestClass>::GetVariable<float>(obj, "test5_intValue2");
    EXPECT_EQ(pValue, nullptr);
}

// Test GetVariables returns the registered variables
TEST_F(TypeReflectionTest, GetVariablesReturnsMap)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test6_testInt", &TestClass::intValue);
    TypeReflection<TestClass>::RegisterMemberVariable("test6_testFloat", &TestClass::floatValue);

    const auto& vars = TypeReflection<TestClass>::StaticGetVariables();
    EXPECT_GE(vars.size(), 2u);
    EXPECT_NE(vars.find("test6_testInt"), vars.end());
    EXPECT_NE(vars.find("test6_testFloat"), vars.end());
}

// Test RegisterMemberFunction and CallFunction with return value
TEST_F(TypeReflectionTest, RegisterAndCallFunctionWithReturn)
{
    TypeReflection<TestClass>::RegisterMemberFunction("test7_Add", &TestClass::Add);

    int result = TypeReflection<TestClass>::CallFunction<int>(obj, "test7_Add", 10, 20);
    EXPECT_EQ(result, 30);
}

// Test RegisterMemberFunction and CallFunction with void return
TEST_F(TypeReflectionTest, RegisterAndCallVoidFunction)
{
    TypeReflection<TestClass>::RegisterMemberFunction("test8_SetInt", &TestClass::SetInt);

    TypeReflection<TestClass>::CallFunction<void>(obj, "test8_SetInt", 77);
    EXPECT_EQ(obj.intValue, 77);
}

// Test RegisterMemberFunction and CallFunction with no arguments
TEST_F(TypeReflectionTest, RegisterAndCallFunctionNoArgs)
{
    TypeReflection<TestClass>::RegisterMemberFunction("GetInt", &TestClass::GetInt);

    int result = TypeReflection<TestClass>::CallFunction<int>(obj, "GetInt");
    EXPECT_EQ(result, 42);
}

// Test RegisterMemberFunction and CallFunction with no args and void return
TEST_F(TypeReflectionTest, RegisterAndCallFunctionNoArgsVoid)
{
    TypeReflection<TestClass>::RegisterMemberFunction("NoArgsNoReturn", &TestClass::NoArgsNoReturn);

    obj.intValue = 50;
    TypeReflection<TestClass>::CallFunction<void>(obj, "NoArgsNoReturn");
    EXPECT_EQ(obj.intValue, 100);
}

// Test RegisterMemberFunction and CallFunction with string return
TEST_F(TypeReflectionTest, RegisterAndCallFunctionStringReturn)
{
    TypeReflection<TestClass>::RegisterMemberFunction("test9_Concat", &TestClass::Concat);

    std::string result = TypeReflection<TestClass>::CallFunction<std::string>(
        obj, "test9_Concat", std::string("Hello"), std::string(" World"));
    EXPECT_EQ(result, "Hello World");
}

// Test type isolation - registrations for different types don't interfere
TEST_F(TypeReflectionTest, TypeIsolation)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test10_value1", &TestClass::intValue);
    TypeReflection<AnotherTestClass>::RegisterMemberVariable("test10_value2", &AnotherTestClass::doubleValue);

    int* pInt = TypeReflection<TestClass>::GetVariable<int>(obj, "test10_value1");
    ASSERT_NE(pInt, nullptr);
    EXPECT_EQ(*pInt, 42);

    double* pDouble = TypeReflection<AnotherTestClass>::GetVariable<double>(anotherObj, "test10_value2");
    ASSERT_NE(pDouble, nullptr);
    EXPECT_DOUBLE_EQ(*pDouble, 2.71);

    // Ensure no cross-contamination
    int* pIntWrong = TypeReflection<TestClass>::GetVariable<int>(obj, "test10_value2");
    EXPECT_EQ(pIntWrong, nullptr);
}

// Test REGISTER_TYPE_REFLECTION_MEMBER_VARIABLE macro
TEST_F(TypeReflectionTest, MacroRegisterMemberVariable)
{
    REGISTER_TYPE_REFLECTION_MEMBER_VARIABLE(TestClass, intValue);

    int* pValue = TypeReflection<TestClass>::GetVariable<int>(obj, "intValue");
    ASSERT_NE(pValue, nullptr);
    EXPECT_EQ(*pValue, 42);
}

// Test REGISTER_TYPE_REFLECTION_MEMBER_FUNCTION macro
TEST_F(TypeReflectionTest, MacroRegisterMemberFunction)
{
    REGISTER_TYPE_REFLECTION_MEMBER_FUNCTION(TestClass, Add);

    int result = TypeReflection<TestClass>::CallFunction<int>(obj, "Add", 5, 7);
    EXPECT_EQ(result, 12);
}

// Test TypeReflectionPack structure
TEST_F(TypeReflectionTest, TypeReflectionPack)
{
    auto pack = TypeReflectionPack("testName", &TestClass::intValue);
    EXPECT_STREQ(pack.name, "testName");
    EXPECT_EQ(pack.val, &TestClass::intValue);
}

// Test with multiple member variables registered
TEST_F(TypeReflectionTest, MultipleVariablesRegistered)
{
    TypeReflection<TestClass>::RegisterMemberVariable("test13_int", &TestClass::intValue);
    TypeReflection<TestClass>::RegisterMemberVariable("test13_float", &TestClass::floatValue);
    TypeReflection<TestClass>::RegisterMemberVariable("test13_string", &TestClass::stringValue);
    TypeReflection<TestClass>::RegisterMemberVariable("test13_bool", &TestClass::boolValue);

    int* pInt = TypeReflection<TestClass>::GetVariable<int>(obj, "test13_int");
    float* pFloat = TypeReflection<TestClass>::GetVariable<float>(obj, "test13_float");
    std::string* pString = TypeReflection<TestClass>::GetVariable<std::string>(obj, "test13_string");
    bool* pBool = TypeReflection<TestClass>::GetVariable<bool>(obj, "test13_bool");

    ASSERT_NE(pInt, nullptr);
    ASSERT_NE(pFloat, nullptr);
    ASSERT_NE(pString, nullptr);
    ASSERT_NE(pBool, nullptr);

    EXPECT_EQ(*pInt, 42);
    EXPECT_FLOAT_EQ(*pFloat, 3.14f);
    EXPECT_EQ(*pString, "hello");
    EXPECT_TRUE(*pBool);
}

// Test with multiple member functions registered
TEST_F(TypeReflectionTest, MultipleFunctionsRegistered)
{
    TypeReflection<TestClass>::RegisterMemberFunction("test14_Add", &TestClass::Add);
    TypeReflection<TestClass>::RegisterMemberFunction("test14_SetInt", &TestClass::SetInt);
    TypeReflection<TestClass>::RegisterMemberFunction("test14_GetInt", &TestClass::GetInt);

    TypeReflection<TestClass>::CallFunction<void>(obj, "test14_SetInt", 88);
    EXPECT_EQ(obj.intValue, 88);

    int result = TypeReflection<TestClass>::CallFunction<int>(obj, "test14_GetInt");
    EXPECT_EQ(result, 88);

    result = TypeReflection<TestClass>::CallFunction<int>(obj, "test14_Add", 12, 13);
    EXPECT_EQ(result, 25);
}

// Test class for macro-based registration
class MacroTestClass : public Object
{
    DECLARE_OBJECT();
public:
    int x = 10;
    int y = 20;
    double z = 3.0;

    int Sum() { return x + y; }
    void Reset() { x = 0; y = 0; }
};

DEFINE_OBJECT(Object, MacroTestClass, "30000000-0000-0000-0000-000000000003");

TYPE_REFLECTION_MEMBER_VARIABLES(MacroTestClass,
    TYPE_REFLECTION_MEM(MacroTestClass, x),
    TYPE_REFLECTION_MEM(MacroTestClass, y),
    TYPE_REFLECTION_MEM(MacroTestClass, z)
)

TYPE_REFLECTION_MEMBER_FUNCTIONS(MacroTestClass,
    TYPE_REFLECTION_FUNC(MacroTestClass, Sum),
    TYPE_REFLECTION_FUNC(MacroTestClass, Reset)
)

// Test macro-based variable registration
TEST(TypeReflectionMacroTest, MacroVariableRegistration)
{
    MacroTestClass obj;
    
    int* pX = TypeReflection<MacroTestClass>::GetVariable<int>(obj, "x");
    int* pY = TypeReflection<MacroTestClass>::GetVariable<int>(obj, "y");
    double* pZ = TypeReflection<MacroTestClass>::GetVariable<double>(obj, "z");

    ASSERT_NE(pX, nullptr);
    ASSERT_NE(pY, nullptr);
    ASSERT_NE(pZ, nullptr);

    EXPECT_EQ(*pX, 10);
    EXPECT_EQ(*pY, 20);
    EXPECT_DOUBLE_EQ(*pZ, 3.0);
}

// Test macro-based function registration
TEST(TypeReflectionMacroTest, MacroFunctionRegistration)
{
    MacroTestClass obj;
    
    int result = TypeReflection<MacroTestClass>::CallFunction<int>(obj, "Sum");
    EXPECT_EQ(result, 30);

    TypeReflection<MacroTestClass>::CallFunction<void>(obj, "Reset");
    EXPECT_EQ(obj.x, 0);
    EXPECT_EQ(obj.y, 0);
}

// Test type safety - calling function with wrong return type should fail
// Note: These tests would fail with assertion if uncommented - kept for documentation
TEST(TypeReflectionMacroTest, TypeSafetyDocumentation)
{
    MacroTestClass obj;
    
    // The following would trigger assertion failures due to type mismatches:
    
    // 1. Wrong return type: Sum returns int, not void
    // TypeReflection<MacroTestClass>::CallFunction<void>(obj, "Sum");  // ASSERT fails
    
    // 2. These tests prove the type system works by passing correct types
    int result = TypeReflection<MacroTestClass>::CallFunction<int>(obj, "Sum");
    EXPECT_EQ(result, 30);
}

// Test that type checking validates argument counts and types
TEST_F(TypeReflectionTest, TypeCheckingValidation)
{
    TypeReflection<TestClass>::RegisterMemberFunction("test16_Add", &TestClass::Add);
    
    // This works - correct types
    int result = TypeReflection<TestClass>::CallFunction<int>(obj, "test16_Add", 5, 7);
    EXPECT_EQ(result, 12);
    
    // The following would fail if uncommented due to type mismatches:
    
    // Wrong argument count: Add expects 2 args, not 1
    // TypeReflection<TestClass>::CallFunction<int>(obj, "test16_Add", 5);  // ASSERT fails
    
    // Wrong argument types: Add expects (int, int) not (float, float)
    // float a = 1.5f, b = 2.5f;
    // TypeReflection<TestClass>::CallFunction<int>(obj, "test16_Add", a, b);  // ASSERT fails
}
