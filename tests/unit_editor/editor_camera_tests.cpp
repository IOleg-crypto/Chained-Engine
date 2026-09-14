#include <gtest/gtest.h>

#include "editor/viewport/camera.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace Chained;

TEST(EditorCameraTest, DefaultInitialization)
{
	EditorCameraController camera;

	EXPECT_FALSE(camera.Is2DMode());
	EXPECT_FLOAT_EQ(camera.GetNearClip(), 0.1f);
	EXPECT_FLOAT_EQ(camera.GetFarClip(), 10000.0f);
	EXPECT_FLOAT_EQ(camera.GetDistance(), 10.0f);
	EXPECT_EQ(camera.GetProjectionType(), ProjectionType::Perspective);
}

TEST(EditorCameraTest, ViewportSizeAndProjection)
{
	EditorCameraController camera;
	camera.SetViewportSize(1920, 1080);

	glm::mat4 proj = camera.GetProjection();
	// Non-zero diagonal elements in valid projection matrix
	EXPECT_NE(proj[0][0], 0.0f);
	EXPECT_NE(proj[1][1], 0.0f);
	EXPECT_NE(proj[2][2], 0.0f);
}

TEST(EditorCameraTest, DirectionVectorsOrthonormality)
{
	EditorCameraController camera;
	camera.SetPitch(0.2f);
	camera.SetYaw(0.5f);

	glm::vec3 forward = camera.GetForwardDirection();
	glm::vec3 right = camera.GetRightDirection();
	glm::vec3 up = camera.GetUpDirection();

	// Vectors should have unit length
	EXPECT_NEAR(glm::length(forward), 1.0f, 0.001f);
	EXPECT_NEAR(glm::length(right), 1.0f, 0.001f);
	EXPECT_NEAR(glm::length(up), 1.0f, 0.001f);

	// Vectors should be mutually orthogonal
	EXPECT_NEAR(glm::dot(forward, right), 0.0f, 0.001f);
	EXPECT_NEAR(glm::dot(forward, up), 0.0f, 0.001f);
	EXPECT_NEAR(glm::dot(right, up), 0.0f, 0.001f);
}

TEST(EditorCameraTest, FocalPointAndPositionCalculation)
{
	EditorCameraController camera;
	camera.SetFocalPoint({0.0f, 5.0f, 0.0f});
	camera.SetDistance(15.0f);
	camera.SetPitch(0.0f);
	camera.SetYaw(0.0f);

	glm::vec3 pos = camera.CalculatePosition();
	glm::vec3 forward = camera.GetForwardDirection();

	// Position should be FocalPoint - Forward * Distance
	glm::vec3 expectedPos = glm::vec3(0.0f, 5.0f, 0.0f) - forward * 15.0f;
	EXPECT_NEAR(pos.x, expectedPos.x, 0.001f);
	EXPECT_NEAR(pos.y, expectedPos.y, 0.001f);
	EXPECT_NEAR(pos.z, expectedPos.z, 0.001f);
}

TEST(EditorCameraTest, Toggle2DModePreserves3DState)
{
	EditorCameraController camera;
	camera.SetPitch(0.35f);
	camera.SetYaw(1.2f);
	camera.SetProjectionType(ProjectionType::Perspective);

	// Switch to 2D
	camera.Set2DMode(true);
	EXPECT_TRUE(camera.Is2DMode());
	EXPECT_EQ(camera.GetProjectionType(), ProjectionType::Orthographic);
	EXPECT_FLOAT_EQ(camera.GetPitch(), 0.0f);
	EXPECT_FLOAT_EQ(camera.GetYaw(), 0.0f);

	// Switch back to 3D
	camera.Set2DMode(false);
	EXPECT_FALSE(camera.Is2DMode());
	EXPECT_EQ(camera.GetProjectionType(), ProjectionType::Perspective);
	EXPECT_NEAR(camera.GetPitch(), 0.35f, 0.001f);
	EXPECT_NEAR(camera.GetYaw(), 1.2f, 0.001f);
}

TEST(EditorCameraTest, ToCamera3DConversion)
{
	EditorCameraController camera;
	camera.SetViewportSize(1280, 720);
	camera.SetFocalPoint({2.0f, 3.0f, 4.0f});

	Camera3D c3d = camera.ToCamera3D();
	EXPECT_NEAR(c3d.Target.x, 2.0f, 0.001f);
	EXPECT_NEAR(c3d.Target.y, 3.0f, 0.001f);
	EXPECT_NEAR(c3d.Target.z, 4.0f, 0.001f);
	EXPECT_EQ(c3d.Projection, ProjectionType::Perspective);
}
