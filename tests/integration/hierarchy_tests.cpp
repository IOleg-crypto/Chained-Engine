#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(HierarchyTest, BasicParenting)
{
	Scene scene;
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");

	parent.AddComponent<HierarchyComponent>();
	child.AddComponent<HierarchyComponent>();

	auto& phc = parent.GetComponent<HierarchyComponent>();
	auto& chc = child.GetComponent<HierarchyComponent>();

	chc.Parent = (entt::entity)parent;
	phc.Children.push_back((entt::entity)child);

	EXPECT_EQ(chc.Parent, (entt::entity)parent);
	EXPECT_EQ(phc.Children.size(), 1);
	EXPECT_EQ(phc.Children[0], (entt::entity)child);
}

TEST(HierarchyTest, DestroyParent)
{
	Scene scene;
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");

	parent.AddComponent<HierarchyComponent>().Children.push_back((entt::entity)child);
	child.AddComponent<HierarchyComponent>().Parent = (entt::entity)parent;

	entt::entity childHandle = (entt::entity)child;

	scene.DestroyEntity(parent);

	// In this engine, DestroyEntity is recursive
	EXPECT_FALSE(scene.GetRegistry().valid(childHandle));
}

TEST(HierarchyTest, ClearParent)
{
	Scene scene;
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");

	parent.AddComponent<HierarchyComponent>();
	child.AddComponent<HierarchyComponent>();

	auto& phc = parent.GetComponent<HierarchyComponent>();
	auto& chc = child.GetComponent<HierarchyComponent>();

	chc.Parent = (entt::entity)parent;
	phc.Children.push_back((entt::entity)child);

	// Clear parenting
	chc.Parent = entt::null;
	auto it = std::find(phc.Children.begin(), phc.Children.end(), (entt::entity)child);
	if (it != phc.Children.end())
	{
		phc.Children.erase(it);
	}

	EXPECT_TRUE(chc.Parent == entt::null);
	EXPECT_EQ(phc.Children.size(), 0);
}

TEST(HierarchyTest, DeepHierarchyDestruction)
{
	Scene scene;
	Entity root = scene.CreateEntity("Root");
	Entity child1 = scene.CreateEntity("Child 1");
	Entity child2 = scene.CreateEntity("Child 2");

	// Root -> Child1 -> Child2
	root.AddComponent<HierarchyComponent>().Children.push_back((entt::entity)child1);
	child1.AddComponent<HierarchyComponent>().Parent = (entt::entity)root;
	child1.GetComponent<HierarchyComponent>().Children.push_back((entt::entity)child2);
	child2.AddComponent<HierarchyComponent>().Parent = (entt::entity)child1;

	entt::entity c1 = (entt::entity)child1;
	entt::entity c2 = (entt::entity)child2;

	scene.DestroyEntity(root);

	EXPECT_FALSE(scene.GetRegistry().valid(c1));
	EXPECT_FALSE(scene.GetRegistry().valid(c2));
}

// ============================================================
//  Destructive / Negative Tests
// ============================================================

// Destroy child BEFORE parent — parent's child list becomes stale.
// Engine must handle it without crashing.
TEST(HierarchyDestructiveTest, DestroyChildBeforeParent)
{
	Scene scene;
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");

	parent.AddComponent<HierarchyComponent>().Children.push_back((entt::entity)child);
	child.AddComponent<HierarchyComponent>().Parent = (entt::entity)parent;

	entt::entity parentHandle = (entt::entity)parent;
	entt::entity childHandle = (entt::entity)child;

	EXPECT_NO_THROW(scene.DestroyEntity(child));
	EXPECT_FALSE(scene.GetRegistry().valid(childHandle));
	EXPECT_TRUE(scene.GetRegistry().valid(parentHandle));

	// Parent still alive but its children list has a stale handle — must not crash
	EXPECT_NO_THROW(scene.DestroyEntity(parent));
	EXPECT_FALSE(scene.GetRegistry().valid(parentHandle));
}

// 1 parent + 500 children destroyed at once — no stack overflow, no O(n^2).
TEST(HierarchyDestructiveTest, WideHierarchyMassDestruction)
{
	Scene scene;
	Entity root = scene.CreateEntity("Root");
	root.AddComponent<HierarchyComponent>();

	constexpr int kCount = 500;
	for (int i = 0; i < kCount; ++i)
	{
		Entity child = scene.CreateEntity("C_" + std::to_string(i));
		child.AddComponent<HierarchyComponent>().Parent = (entt::entity)root;
		root.GetComponent<HierarchyComponent>().Children.push_back((entt::entity)child);
	}

	EXPECT_EQ((int)root.GetComponent<HierarchyComponent>().Children.size(), kCount);
	EXPECT_NO_THROW(scene.DestroyEntity(root));

	auto view = scene.GetRegistry().view<TagComponent>();
	EXPECT_EQ(std::distance(view.begin(), view.end()), 0);
}

// Re-parent the same entity 100 times alternately between two parents.
TEST(HierarchyDestructiveTest, RapidReparenting)
{
	Scene scene;
	Entity p1 = scene.CreateEntity("Parent1");
	Entity p2 = scene.CreateEntity("Parent2");
	Entity child = scene.CreateEntity("Child");

	p1.AddComponent<HierarchyComponent>();
	p2.AddComponent<HierarchyComponent>();
	auto& chc = child.AddComponent<HierarchyComponent>();

	for (int i = 0; i < 100; ++i)
	{
		Entity& newP = (i % 2 == 0) ? p1 : p2;
		Entity& oldP = (i % 2 == 0) ? p2 : p1;

		auto& oldKids = oldP.GetComponent<HierarchyComponent>().Children;
		oldKids.erase(std::remove(oldKids.begin(), oldKids.end(), (entt::entity)child), oldKids.end());

		newP.GetComponent<HierarchyComponent>().Children.push_back((entt::entity)child);
		chc.Parent = (entt::entity)newP;
	}

	// 100 swaps (starting at 0 for p1, 1 for p2... 99 for p2)
	// Means the last assigned parent is p2!
	EXPECT_EQ(chc.Parent, (entt::entity)p2);
	EXPECT_NO_THROW(scene.DestroyEntity(p1));
	EXPECT_NO_THROW(scene.DestroyEntity(p2));
}
