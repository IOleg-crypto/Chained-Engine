// editor_camera_tests.cpp
// Consolidated scenario-based tests for EditorCameraController.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "editor/viewport/camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include "gtest/gtest.h"

using namespace Chained;

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Camera Projections and 2D/3D Toggling)
// ============================================================================
TEST(EditorCameraModule, Positive_ProjectionsAndModeToggling)
{
	// Arrange
	EditorCameraController camera;

	// Assert (Default initial state)
	EXPECT_FALSE(camera.Is2DMode());
	EXPECT_FLOAT_EQ(camera.GetNearClip(), 0.1f);
	EXPECT_FLOAT_EQ(camera.GetFarClip(), 10000.0f);
	EXPECT_FLOAT_EQ(camera.GetDistance(), 10.0f);
	EXPECT_EQ(camera.GetProjectionType(), ProjectionType::Perspective);

	// Act (Set viewport size)
	camera.SetViewportSize(1920, 1080);
	glm::mat4 proj = camera.GetProjection();

	// Assert (Valid non-zero projection matrix diagonals)
	EXPECT_NE(proj[0][0], 0.0f);
	EXPECT_NE(proj[1][1], 0.0f);
	EXPECT_NE(proj[2][2], 0.0f);

	// Act (Set pitch/yaw orientation)
	camera.SetPitch(0.2f);
	camera.SetYaw(0.5f);

	glm::vec3 forward = camera.GetForwardDirection();
	glm::vec3 right = camera.GetRightDirection();
	glm::vec3 up = camera.GetUpDirection();

	// Assert (Direction vectors have unit length and are orthogonal)
	EXPECT_NEAR(glm::length(forward), 1.0f, 0.001f);
	EXPECT_NEAR(glm::length(right), 1.0f, 0.001f);
	EXPECT_NEAR(glm::length(up), 1.0f, 0.001f);
	EXPECT_NEAR(glm::dot(forward, right), 0.0f, 0.001f);
	EXPECT_NEAR(glm::dot(forward, up), 0.0f, 0.001f);

	// Act (Switch to 2D Orthographic mode)
	camera.Set2DMode(true);

	// Assert
	EXPECT_TRUE(camera.Is2DMode());
	EXPECT_EQ(camera.GetProjectionType(), ProjectionType::Orthographic);

	// Act (Switch back to 3D mode — restores pitch and yaw)
	camera.Set2DMode(false);

	// Assert
	EXPECT_FALSE(camera.Is2DMode());
	EXPECT_EQ(camera.GetProjectionType(), ProjectionType::Perspective);
	EXPECT_NEAR(camera.GetPitch(), 0.2f, 0.001f);
	EXPECT_NEAR(camera.GetYaw(), 0.5f, 0.001f);
}
