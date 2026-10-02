// command_history_tests.cpp
// Consolidated scenario-based tests for Editor CommandHistory and Undo/Redo operations.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "editor/undo/command_history.h"
#include "editor/undo/command.h"
#include "editor/undo/entity_commands.h"
#include "editor/undo/component_commands.h"
#include "engine/scene/scene.h"
#include "engine/scene/components.h"
#include "gtest/gtest.h"

using namespace Chained;

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Compound Undo/Redo Workflow)
// ============================================================================
TEST(CommandHistoryModule, Positive_UndoRedoCompoundWorkflow)
{
	// Arrange
	auto scene = std::make_shared<Scene>();
	scene->GetRegistry().ctx().emplace<Scene*>(scene.get());
	CommandHistory history(20);

	// Act & Assert (Step 1: Create Entity Command)
	history.PushCommand(std::make_unique<CreateEntityCommand>(scene.get(), "PlayerEntity"));
	Entity player = scene->FindEntityByTag("PlayerEntity");
	ASSERT_TRUE(player.IsValid());

	history.Undo();
	EXPECT_FALSE(scene->FindEntityByTag("PlayerEntity").IsValid());

	history.Redo();
	player = scene->FindEntityByTag("PlayerEntity");
	ASSERT_TRUE(player.IsValid());

	// Act & Assert (Step 2: Add Component Command)
	history.PushCommand(std::make_unique<AddComponentCommand<LightComponent>>(player));
	EXPECT_TRUE(player.HasComponent<LightComponent>());

	history.Undo();
	EXPECT_FALSE(player.HasComponent<LightComponent>());

	history.Redo();
	EXPECT_TRUE(player.HasComponent<LightComponent>());

	// Act & Assert (Step 3: Parenting Command)
	Entity weapon = scene->CreateEntity("WeaponEntity");
	weapon.AddComponent<HierarchyComponent>();
	player.AddOrReplaceComponent<HierarchyComponent>();

	history.PushCommand(std::make_unique<ParentEntityCommand>(weapon, player, scene.get()));
	EXPECT_EQ(weapon.GetComponent<HierarchyComponent>().Parent, (entt::entity)player);

	history.Undo();
	EXPECT_TRUE(weapon.GetComponent<HierarchyComponent>().Parent == entt::null);

	history.Redo();
	EXPECT_EQ(weapon.GetComponent<HierarchyComponent>().Parent, (entt::entity)player);

	// Act & Assert (Step 4: Destroy Entity Command with Full State Restoration)
	uint64_t weaponUUID = weapon.GetUUID();
	history.PushCommand(std::make_unique<DestroyEntityCommand>(weapon));
	EXPECT_FALSE(scene->FindEntityByTag("WeaponEntity").IsValid());

	history.Undo(); // Restores exact UUID and components
	Entity restoredWeapon = scene->GetEntityByUUID(UUID(weaponUUID));
	ASSERT_TRUE(restoredWeapon.IsValid());
	EXPECT_EQ(restoredWeapon.GetName(), "WeaponEntity");

	history.Redo(); // Destroys again
	EXPECT_FALSE(scene->FindEntityByTag("WeaponEntity").IsValid());
}

// ============================================================================
// 2. NEGATIVE SCENARIO (History Boundaries, Capacity, Redo Invalidation)
// ============================================================================
TEST(CommandHistoryModule, Negative_HistoryBoundaryAndInvalidation)
{
	// Arrange
	constexpr size_t kMaxHistory = 3;
	CommandHistory history(kMaxHistory);

	class CounterCommand : public IEditorCommand
	{
	public:
		int* target;
		int delta;
		CounterCommand(int* t, int d)
			: target(t),
			  delta(d)
		{
		}
		void Execute() override
		{
			*target += delta;
		}
		void Undo() override
		{
			*target -= delta;
		}
		std::string GetName() const override
		{
			return "Counter";
		}
	};

	int value = 0;

	// Act & Assert (Step 1: Undo / Redo on empty history does not crash)
	EXPECT_NO_THROW(history.Undo());
	EXPECT_NO_THROW(history.Redo());
	EXPECT_EQ(value, 0);

	// Act (Step 2: Overflow capacity — push 5 commands with max size 3)
	for (int i = 0; i < 5; ++i)
	{
		history.PushCommand(std::make_unique<CounterCommand>(&value, 1));
	}

	// Assert
	EXPECT_EQ(value, 5);

	// Act & Assert (Can only undo 3 times back to 2)
	history.Undo(); // 4
	history.Undo(); // 3
	history.Undo(); // 2
	EXPECT_EQ(value, 2);

	// 4th Undo does nothing as oldest commands were evicted
	history.Undo();
	EXPECT_EQ(value, 2);

	// Act (Step 3: New command invalidates redo stack)
	history.PushCommand(std::make_unique<CounterCommand>(&value, 10));
	EXPECT_EQ(value, 12);

	// Assert (Redo does nothing)
	history.Redo();
	EXPECT_EQ(value, 12);

	// Act (Undo rolls back new command)
	history.Undo();
	EXPECT_EQ(value, 2);
}
