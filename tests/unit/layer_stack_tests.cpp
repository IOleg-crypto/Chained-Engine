// layer_stack_tests.cpp
// Consolidated scenario-based tests for LayerStack lifecycle.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/core/layer_stack.h"
#include "engine/core/layer.h"
#include "gtest/gtest.h"

using namespace Chained;

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Push Layers and Overlays, Order)
// ============================================================================
TEST(LayerStackModule, Positive_PushLayersAndOverlaysOrder)
{
	// Arrange
	LayerStack stack;
	EXPECT_EQ(stack.GetLayerCount(), 0u);

	// Act (Push regular layers and overlay)
	stack.PushLayer(std::make_unique<Layer>("LayerA"));
	stack.PushLayer(std::make_unique<Layer>("LayerB"));
	stack.PushOverlay(std::make_unique<Layer>("Overlay1"));
	stack.PushLayer(std::make_unique<Layer>("LayerC"));

	// Assert: Layers are inserted before overlays, overlays stay at the end
	ASSERT_EQ(stack.GetLayerCount(), 4u);
	EXPECT_EQ(stack.GetLayerAt(0)->GetName(), "LayerA");
	EXPECT_EQ(stack.GetLayerAt(1)->GetName(), "LayerB");
	EXPECT_EQ(stack.GetLayerAt(2)->GetName(), "LayerC");
	EXPECT_EQ(stack.GetLayerAt(3)->GetName(), "Overlay1");

	// Act & Assert (Lookup by name)
	EXPECT_TRUE(stack.HasLayer("LayerA"));
	EXPECT_TRUE(stack.HasLayer("Overlay1"));
	EXPECT_FALSE(stack.HasLayer("NonExistent"));

	// Act (Shutdown clears stack)
	stack.Shutdown();

	// Assert
	EXPECT_EQ(stack.GetLayerCount(), 0u);
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Out-of-Bounds Queries Safety)
// ============================================================================
TEST(LayerStackModule, Negative_OutOfBoundsQueriesReturnNull)
{
	// Arrange
	LayerStack stack;

	// Act & Assert
	EXPECT_EQ(stack.GetLayerAt(0), nullptr);
	EXPECT_EQ(stack.GetLayerAt(999), nullptr);
}
