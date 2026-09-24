#include <gtest/gtest.h>

#include "editor/undo/command_history.h"
#include "editor/undo/command.h"
#include "editor/undo/entity_commands.h"
#include "editor/undo/component_commands.h"
#include "engine/scene/scene.h"
#include "engine/scene/components.h"

using namespace Chained;

namespace
{
	class MockCommand : public IEditorCommand
	{
	public:
		MockCommand(int* value, int increment)
			: m_Value(value),
			  m_Increment(increment)
		{
		}

		void Execute() override
		{
			*m_Value += m_Increment;
		}

		void Undo() override
		{
			*m_Value -= m_Increment;
		}

		std::string GetName() const override
		{
			return "MockCommand";
		}

	private:
		int* m_Value;
		int m_Increment;
	};
} // namespace

TEST(CommandHistoryTest, BasicUndoRedo)
{
	CommandHistory history(10);
	int counter = 0;

	history.PushCommand(std::make_unique<MockCommand>(&counter, 5));
	EXPECT_EQ(counter, 5);

	history.PushCommand(std::make_unique<MockCommand>(&counter, 10));
	EXPECT_EQ(counter, 15);

	history.Undo();
	EXPECT_EQ(counter, 5);

	history.Undo();
	EXPECT_EQ(counter, 0);

	// Extra undo does nothing
	history.Undo();
	EXPECT_EQ(counter, 0);

	history.Redo();
	EXPECT_EQ(counter, 5);

	history.Redo();
	EXPECT_EQ(counter, 15);

	// Extra redo does nothing
	history.Redo();
	EXPECT_EQ(counter, 15);
}

TEST(CommandHistoryTest, PushClearsRedoStack)
{
	CommandHistory history(10);
	int counter = 0;

	history.PushCommand(std::make_unique<MockCommand>(&counter, 5));
	history.PushCommand(std::make_unique<MockCommand>(&counter, 10));
	EXPECT_EQ(counter, 15);

	history.Undo();
	EXPECT_EQ(counter, 5);

	// Pushing a new command should invalidate the previous redo stack
	history.PushCommand(std::make_unique<MockCommand>(&counter, 20));
	EXPECT_EQ(counter, 25);

	history.Redo(); // Should do nothing
	EXPECT_EQ(counter, 25);

	history.Undo();
	EXPECT_EQ(counter, 5);
}

TEST(CommandHistoryTest, MaxHistoryCap)
{
	constexpr size_t maxHistory = 3;
	CommandHistory history(maxHistory);
	int counter = 0;

	for (int i = 0; i < 5; ++i)
	{
		history.PushCommand(std::make_unique<MockCommand>(&counter, 1));
	}
	EXPECT_EQ(counter, 5);

	// Since maxHistory is 3, we should only be able to undo 3 times
	history.Undo();
	EXPECT_EQ(counter, 4);
	history.Undo();
	EXPECT_EQ(counter, 3);
	history.Undo();
	EXPECT_EQ(counter, 2);

	// 4th undo should do nothing because oldest commands were popped
	history.Undo();
	EXPECT_EQ(counter, 2);
}

TEST(EntityCommandsTest, CreateEntityCommand)
{
	auto scene = std::make_shared<Scene>();
	CommandHistory history(10);

	history.PushCommand(std::make_unique<CreateEntityCommand>(scene.get(), "NewEntity"));

	Entity found = scene->FindEntityByTag("NewEntity");
	EXPECT_TRUE(found.IsValid());

	history.Undo();
	found = scene->FindEntityByTag("NewEntity");
	EXPECT_FALSE(found.IsValid());

	history.Redo();
	found = scene->FindEntityByTag("NewEntity");
	EXPECT_TRUE(found.IsValid());
}

TEST(EntityCommandsTest, DestroyEntityCommandRestoresUUIDAndComponents)
{
	auto scene = std::make_shared<Scene>();
	CommandHistory history(10);

	Entity entity = scene->CreateEntity("TargetEntity");
	uint64_t originalUUID = entity.GetUUID();
	auto& tc = entity.GetComponent<TransformComponent>();
	tc.Translation = {1.0f, 2.0f, 3.0f};

	history.PushCommand(std::make_unique<DestroyEntityCommand>(entity));

	// Entity destroyed
	Entity notFound = scene->GetEntityByUUID(originalUUID);
	EXPECT_FALSE(notFound.IsValid());

	// Undo restores entity with identical UUID and components
	history.Undo();
	Entity restored = scene->GetEntityByUUID(originalUUID);
	ASSERT_TRUE(restored.IsValid());
	EXPECT_EQ(restored.GetComponent<TagComponent>().Tag, "TargetEntity");
	EXPECT_FLOAT_EQ(restored.GetComponent<TransformComponent>().Translation.x, 1.0f);
	EXPECT_FLOAT_EQ(restored.GetComponent<TransformComponent>().Translation.y, 2.0f);
	EXPECT_FLOAT_EQ(restored.GetComponent<TransformComponent>().Translation.z, 3.0f);
}

TEST(EntityCommandsTest, DuplicateEntityCommand)
{
	auto scene = std::make_shared<Scene>();
	CommandHistory history(10);

	Entity original = scene->CreateEntity("Original");
	original.GetComponent<TransformComponent>().Translation = {5.0f, 0.0f, 0.0f};

	history.PushCommand(std::make_unique<DuplicateEntityCommand>(original));

	Entity duplicate = scene->FindEntityByTag("Original_copy");
	EXPECT_TRUE(duplicate.IsValid());
	EXPECT_FLOAT_EQ(duplicate.GetComponent<TransformComponent>().Translation.x, 5.0f);

	history.Undo();
	duplicate = scene->FindEntityByTag("Original_copy");
	EXPECT_FALSE(duplicate.IsValid());
	EXPECT_TRUE(scene->FindEntityByTag("Original").IsValid());

	history.Redo();
	duplicate = scene->FindEntityByTag("Original_copy");
	EXPECT_TRUE(duplicate.IsValid());
}

TEST(EntityCommandsTest, ParentEntityCommand)
{
	auto scene = std::make_shared<Scene>();
	CommandHistory history(10);

	Entity parent = scene->CreateEntity("Parent");
	Entity child = scene->CreateEntity("Child");

	history.PushCommand(std::make_unique<ParentEntityCommand>(child, parent, scene.get()));

	ASSERT_TRUE(child.HasComponent<HierarchyComponent>());
	ASSERT_TRUE(parent.HasComponent<HierarchyComponent>());
	EXPECT_EQ(child.GetComponent<HierarchyComponent>().Parent, (entt::entity)parent);
	ASSERT_EQ(parent.GetComponent<HierarchyComponent>().Children.size(), 1u);
	EXPECT_EQ(parent.GetComponent<HierarchyComponent>().Children[0], (entt::entity)child);

	// Undo parenting
	history.Undo();
	EXPECT_TRUE(child.GetComponent<HierarchyComponent>().Parent == entt::null);
	EXPECT_TRUE(parent.GetComponent<HierarchyComponent>().Children.empty());

	// Redo parenting
	history.Redo();
	EXPECT_EQ(child.GetComponent<HierarchyComponent>().Parent, (entt::entity)parent);
	ASSERT_EQ(parent.GetComponent<HierarchyComponent>().Children.size(), 1u);
}

TEST(ComponentCommandsTest, AddAndRemoveComponent)
{
	auto scene = std::make_shared<Scene>();
	CommandHistory history(10);

	Entity entity = scene->CreateEntity("CompEntity");
	EXPECT_FALSE(entity.HasComponent<LightComponent>());

	history.PushCommand(std::make_unique<AddComponentCommand<LightComponent>>(entity));
	EXPECT_TRUE(entity.HasComponent<LightComponent>());

	history.Undo();
	EXPECT_FALSE(entity.HasComponent<LightComponent>());

	history.Redo();
	EXPECT_TRUE(entity.HasComponent<LightComponent>());

	// Now test removal command
	history.PushCommand(std::make_unique<RemoveComponentCommand<LightComponent>>(entity));
	EXPECT_FALSE(entity.HasComponent<LightComponent>());

	history.Undo();
	EXPECT_TRUE(entity.HasComponent<LightComponent>());
}
