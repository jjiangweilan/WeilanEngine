#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>

namespace
{
void ExpectMatrixNear(const glm::mat4& actual, const glm::mat4& expected)
{
    constexpr float epsilon = 1e-5f;
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
            EXPECT_NEAR(actual[column][row], expected[column][row], epsilon);
    }
}
} // namespace

TEST(SceneTest, CopyChildPreservesLocalAndWorldTransformUnderScaledParent)
{
    Scene scene;
    GameObject* parent = scene.CreateGameObject();
    parent->SetLocalPosition({3.0f, -2.0f, 7.0f});
    parent->SetLocalRotation(glm::angleAxis(glm::radians(35.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 0.5f))));
    parent->SetLocalScale({2.0f, 3.0f, 0.5f});

    GameObject* source = scene.CreateGameObject();
    source->SetParent(parent, false);
    source->SetLocalPosition({-4.0f, 1.5f, 2.0f});
    source->SetLocalRotation(glm::angleAxis(glm::radians(-28.0f), glm::normalize(glm::vec3(0.5f, 1.0f, 2.0f))));
    source->SetLocalScale({0.75f, 1.25f, 2.5f});

    const glm::mat4 sourceWorld = source->GetWorldMatrix();
    GameObject* copy = scene.CopyGameObject(*source);

    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetParent(), parent);
    EXPECT_EQ(copy->GetLocalPosition(), source->GetLocalPosition());
    EXPECT_EQ(copy->GetLocalRotation(), source->GetLocalRotation());
    EXPECT_EQ(copy->GetLocalScale(), source->GetLocalScale());
    ExpectMatrixNear(copy->GetWorldMatrix(), sourceWorld);
}

TEST(SceneTest, CopyRootPreservesWorldTransform)
{
    Scene scene;
    GameObject* source = scene.CreateGameObject();
    source->SetLocalPosition({5.0f, 6.0f, -7.0f});
    source->SetLocalRotation(glm::angleAxis(glm::radians(42.0f), glm::normalize(glm::vec3(2.0f, 1.0f, 3.0f))));
    source->SetLocalScale({1.5f, 0.5f, 2.0f});

    const glm::mat4 sourceWorld = source->GetWorldMatrix();
    GameObject* copy = scene.CopyGameObject(*source);

    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetParent(), nullptr);
    ExpectMatrixNear(copy->GetWorldMatrix(), sourceWorld);
}
