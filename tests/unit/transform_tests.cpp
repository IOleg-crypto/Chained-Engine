// transform_tests.cpp
// Unit tests for TransformSystem (ComputeLocalMatrix, ComputeInterpolatedMatrix, SRT accessors).

#include "engine/scene/systems/transform_system.h"
#include "gtest/gtest.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace Chained;

TEST(TransformSystemTest, DefaultTransformValues)
{
	TransformComponent tc;
	EXPECT_EQ(TransformSystem::GetTranslation(tc), glm::vec3(0.0f));
	EXPECT_EQ(TransformSystem::GetRotation(tc), glm::vec3(0.0f));
	EXPECT_EQ(TransformSystem::GetScale(tc), glm::vec3(1.0f));

	glm::quat q = TransformSystem::GetRotationQuat(tc);
	EXPECT_FLOAT_EQ(q.w, 1.0f);
	EXPECT_FLOAT_EQ(q.x, 0.0f);
	EXPECT_FLOAT_EQ(q.y, 0.0f);
	EXPECT_FLOAT_EQ(q.z, 0.0f);
}

TEST(TransformSystemTest, SetTranslationUpdatesTransformChanged)
{
	TransformComponent tc;
	tc.TransformChanged = false;

	TransformSystem::SetTranslation(tc, {10.0f, -5.0f, 2.5f});
	EXPECT_TRUE(tc.TransformChanged);
	EXPECT_EQ(TransformSystem::GetTranslation(tc), glm::vec3(10.0f, -5.0f, 2.5f));
}

TEST(TransformSystemTest, SetScaleUpdatesTransformChanged)
{
	TransformComponent tc;
	tc.TransformChanged = false;

	TransformSystem::SetScale(tc, {2.0f, 3.0f, 4.0f});
	EXPECT_TRUE(tc.TransformChanged);
	EXPECT_EQ(TransformSystem::GetScale(tc), glm::vec3(2.0f, 3.0f, 4.0f));
}

TEST(TransformSystemTest, SetRotationUpdatesQuatAndEuler)
{
	TransformComponent tc;
	tc.TransformChanged = false;

	glm::vec3 euler(glm::radians(90.0f), 0.0f, 0.0f);
	TransformSystem::SetRotation(tc, euler);

	EXPECT_TRUE(tc.TransformChanged);
	EXPECT_FLOAT_EQ(TransformSystem::GetRotation(tc).x, euler.x);

	glm::quat q = TransformSystem::GetRotationQuat(tc);
	// 90 deg around X: w = cos(45) = 0.7071, x = sin(45) = 0.7071
	EXPECT_NEAR(q.w, std::cos(glm::radians(45.0f)), 1e-4f);
	EXPECT_NEAR(q.x, std::sin(glm::radians(45.0f)), 1e-4f);
	EXPECT_NEAR(q.y, 0.0f, 1e-4f);
	EXPECT_NEAR(q.z, 0.0f, 1e-4f);
}

TEST(TransformSystemTest, SetRotationQuatUpdatesEuler)
{
	TransformComponent tc;
	tc.TransformChanged = false;

	glm::quat q = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	TransformSystem::SetRotationQuat(tc, q);

	EXPECT_TRUE(tc.TransformChanged);
	EXPECT_NEAR(TransformSystem::GetRotation(tc).y, glm::radians(45.0f), 1e-4f);
}

TEST(TransformSystemTest, ComputeLocalMatrixIdentity)
{
	TransformComponent tc;
	glm::mat4 mat = TransformSystem::ComputeLocalMatrix(tc);
	EXPECT_EQ(mat, glm::mat4(1.0f));
}

TEST(TransformSystemTest, ComputeLocalMatrixTranslationOnly)
{
	TransformComponent tc;
	TransformSystem::SetTranslation(tc, {3.0f, 7.0f, -2.0f});

	glm::mat4 mat = TransformSystem::ComputeLocalMatrix(tc);
	glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 transformed = mat * origin;

	EXPECT_FLOAT_EQ(transformed.x, 3.0f);
	EXPECT_FLOAT_EQ(transformed.y, 7.0f);
	EXPECT_FLOAT_EQ(transformed.z, -2.0f);
	EXPECT_FLOAT_EQ(transformed.w, 1.0f);
}

TEST(TransformSystemTest, ComputeLocalMatrixScaleOnly)
{
	TransformComponent tc;
	TransformSystem::SetScale(tc, {2.0f, 0.5f, 3.0f});

	glm::mat4 mat = TransformSystem::ComputeLocalMatrix(tc);
	glm::vec4 point(1.0f, 2.0f, 3.0f, 1.0f);
	glm::vec4 transformed = mat * point;

	EXPECT_FLOAT_EQ(transformed.x, 2.0f);
	EXPECT_FLOAT_EQ(transformed.y, 1.0f);
	EXPECT_FLOAT_EQ(transformed.z, 9.0f);
	EXPECT_FLOAT_EQ(transformed.w, 1.0f);
}

TEST(TransformSystemTest, ComputeLocalMatrixFullSRT)
{
	TransformComponent tc;
	TransformSystem::SetTranslation(tc, {10.0f, 20.0f, 30.0f});
	TransformSystem::SetRotation(tc, {0.0f, glm::radians(90.0f), 0.0f});
	TransformSystem::SetScale(tc, {2.0f, 2.0f, 2.0f});

	glm::mat4 mat = TransformSystem::ComputeLocalMatrix(tc);

	// Point (1, 0, 0) scaled to (2, 0, 0), rotated 90 deg around Y to (0, 0, -2), translated to (10, 20, 28)
	glm::vec4 point(1.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 transformed = mat * point;

	EXPECT_NEAR(transformed.x, 10.0f, 1e-4f);
	EXPECT_NEAR(transformed.y, 20.0f, 1e-4f);
	EXPECT_NEAR(transformed.z, 28.0f, 1e-4f);
}

TEST(TransformSystemTest, ComputeInterpolatedMatrixAlphaZeroReturnsPrev)
{
	TransformComponent tc;
	tc.PrevTranslation = {0.0f, 0.0f, 0.0f};
	tc.Translation = {10.0f, 10.0f, 10.0f};

	tc.PrevScale = {1.0f, 1.0f, 1.0f};
	tc.Scale = {2.0f, 2.0f, 2.0f};

	tc.PrevRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	tc.RotationQuat = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));

	glm::mat4 mat = TransformSystem::ComputeInterpolatedMatrix(tc, 0.0f);
	glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 transformed = mat * origin;

	EXPECT_NEAR(transformed.x, 0.0f, 1e-4f);
	EXPECT_NEAR(transformed.y, 0.0f, 1e-4f);
	EXPECT_NEAR(transformed.z, 0.0f, 1e-4f);
}

TEST(TransformSystemTest, ComputeInterpolatedMatrixAlphaOneReturnsCurrent)
{
	TransformComponent tc;
	tc.PrevTranslation = {0.0f, 0.0f, 0.0f};
	tc.Translation = {10.0f, 10.0f, 10.0f};

	tc.PrevScale = {1.0f, 1.0f, 1.0f};
	tc.Scale = {2.0f, 2.0f, 2.0f};

	tc.PrevRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	tc.RotationQuat = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));

	glm::mat4 mat = TransformSystem::ComputeInterpolatedMatrix(tc, 1.0f);
	glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 transformed = mat * origin;

	EXPECT_NEAR(transformed.x, 10.0f, 1e-4f);
	EXPECT_NEAR(transformed.y, 10.0f, 1e-4f);
	EXPECT_NEAR(transformed.z, 10.0f, 1e-4f);
}

TEST(TransformSystemTest, ComputeInterpolatedMatrixMidpoint)
{
	TransformComponent tc;
	tc.PrevTranslation = {0.0f, 0.0f, 0.0f};
	tc.Translation = {10.0f, 20.0f, 30.0f};

	tc.PrevScale = {1.0f, 1.0f, 1.0f};
	tc.Scale = {3.0f, 5.0f, 7.0f};

	tc.PrevRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	tc.RotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	glm::mat4 mat = TransformSystem::ComputeInterpolatedMatrix(tc, 0.5f);
	glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 transformed = mat * origin;

	EXPECT_NEAR(transformed.x, 5.0f, 1e-4f);
	EXPECT_NEAR(transformed.y, 10.0f, 1e-4f);
	EXPECT_NEAR(transformed.z, 15.0f, 1e-4f);
}
