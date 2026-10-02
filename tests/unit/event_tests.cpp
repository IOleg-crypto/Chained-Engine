#include <gtest/gtest.h>
#include "engine/core/events/events.h"
#include "engine/core/events/window_events.h"
#include "engine/core/events/input_events.h"

using namespace Chained;

TEST(EventTest, Dispatcher)
{
	WindowResizeEvent e(1280, 720);
	EventDispatcher dispatcher(e);

	bool dispatched = dispatcher.Dispatch<WindowResizeEvent>([](WindowResizeEvent& event) {
		EXPECT_EQ(event.GetWidth(), 1280);
		EXPECT_EQ(event.GetHeight(), 720);
		return true;
	});

	EXPECT_TRUE(dispatched);
	EXPECT_TRUE(e.Handled);
}

TEST(EventTest, Queue)
{
	EventQueue queue;
	queue.Push(std::make_unique<WindowCloseEvent>());
	queue.Enqueue<KeyPressedEvent>(KeyCode::A, false); // 'A' key

	int processedCount = 0;
	queue.Process([&processedCount](Event& e) {
		processedCount++;
		if (processedCount == 1)
		{
			EXPECT_EQ(e.GetEventType(), EventType::WindowClose);
		}
		else if (processedCount == 2)
		{
			EXPECT_EQ(e.GetEventType(), EventType::KeyPressed);
		}
	});

	EXPECT_EQ(processedCount, 2);
	EXPECT_TRUE(queue.IsEmpty());
}

TEST(EventTest, Category)
{
	KeyPressedEvent e(KeyCode::A);
	EXPECT_TRUE(e.IsInCategory(EventCategoryKeyboard));
	EXPECT_TRUE(e.IsInCategory(EventCategoryInput));
	EXPECT_FALSE(e.IsInCategory(EventCategoryEditor));
}
