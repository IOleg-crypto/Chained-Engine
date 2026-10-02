#include "engine/core/layer_stack.h"
#include "engine/core/layer.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(LayerStackTest, EmptyOnInit)
{
	LayerStack stack;
	EXPECT_EQ(stack.begin(), stack.end());
	EXPECT_EQ(stack.GetLayerCount(), 0u);
}

TEST(LayerStackTest, PushLayerAddsLayer)
{
	LayerStack stack;
	stack.PushLayer(std::make_unique<Layer>("TestLayer"));

	EXPECT_EQ(stack.GetLayerCount(), 1u);
	EXPECT_NE(stack.GetLayerAt(0), nullptr);
	EXPECT_EQ(stack.GetLayerAt(0)->GetName(), "TestLayer");
}

TEST(LayerStackTest, PushOverlayAddsToEnd)
{
	LayerStack stack;
	stack.PushLayer(std::make_unique<Layer>("Layer1"));
	stack.PushLayer(std::make_unique<Layer>("Layer2"));
	stack.PushOverlay(std::make_unique<Layer>("Overlay"));

	ASSERT_EQ(stack.GetLayerCount(), 3u);
	EXPECT_EQ(stack.GetLayerAt(0)->GetName(), "Layer1");
	EXPECT_EQ(stack.GetLayerAt(1)->GetName(), "Layer2");
	EXPECT_EQ(stack.GetLayerAt(2)->GetName(), "Overlay");
}

TEST(LayerStackTest, HasLayerFindsByName)
{
	LayerStack stack;
	stack.PushLayer(std::make_unique<Layer>("PlayerLayer"));

	EXPECT_TRUE(stack.HasLayer("PlayerLayer"));
	EXPECT_FALSE(stack.HasLayer("Nonexistent"));
}

TEST(LayerStackTest, LayerInsertIndex)
{
	LayerStack stack;
	stack.PushLayer(std::make_unique<Layer>("A"));
	stack.PushLayer(std::make_unique<Layer>("B"));
	stack.PushOverlay(std::make_unique<Layer>("O1"));
	stack.PushLayer(std::make_unique<Layer>("C"));

	ASSERT_EQ(stack.GetLayerCount(), 4u);
	EXPECT_EQ(stack.GetLayerAt(0)->GetName(), "A");
	EXPECT_EQ(stack.GetLayerAt(1)->GetName(), "B");
	EXPECT_EQ(stack.GetLayerAt(2)->GetName(), "C");
	EXPECT_EQ(stack.GetLayerAt(3)->GetName(), "O1");
}

TEST(LayerStackTest, ShutdownClearsLayers)
{
	LayerStack stack;
	stack.PushLayer(std::make_unique<Layer>("L1"));
	stack.PushLayer(std::make_unique<Layer>("L2"));

	stack.Shutdown();
	EXPECT_EQ(stack.GetLayerCount(), 0u);
}

TEST(LayerStackTest, GetLayerAtReturnsNullptrForOutOfBounds)
{
	LayerStack stack;
	EXPECT_EQ(stack.GetLayerAt(0), nullptr);
	EXPECT_EQ(stack.GetLayerAt(100), nullptr);
}

TEST(LayerStackTest, ConstGetLayersIsReadOnly)
{
	LayerStack stack;
	stack.PushLayer(std::make_unique<Layer>("ReadOnly"));
	const auto& constStack = stack;

	EXPECT_EQ(constStack.GetLayerCount(), 1u);
	EXPECT_EQ(constStack.GetLayerAt(0)->GetName(), "ReadOnly");
	EXPECT_FALSE(constStack.GetLayers().empty());
}
