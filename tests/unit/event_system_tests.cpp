// event_system_tests.cpp
// Consolidated scenario-based tests for EventDispatcher, EventQueue, Input Events, and Categories.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/core/events/events.h"
#include "engine/core/events/window_events.h"
#include "engine/core/events/input_events.h"
#include "engine/core/key_codes.h"
#include "gtest/gtest.h"

using namespace Chained;

// ============================================================================
// 1. EVENT DISPATCHER AND CATEGORIES (Happy Path: Dispatching and Category Filters)
// ============================================================================
TEST(EventSystemModule, Positive_DispatcherAndCategories)
{
	// Arrange
	KeyPressedEvent keyEvent(KeyCode::W, false);

	// Assert (Category and initial flags)
	EXPECT_TRUE(keyEvent.IsInCategory(EventCategoryKeyboard));
	EXPECT_TRUE(keyEvent.IsInCategory(EventCategoryInput));
	EXPECT_FALSE(keyEvent.IsInCategory(EventCategoryEditor));
	EXPECT_EQ(keyEvent.GetKeyCode(), KeyCode::W);
	EXPECT_FALSE(keyEvent.IsRepeat());

	// Act (Dispatch to KeyPressedEvent handler)
	EventDispatcher dispatcher(keyEvent);
	bool handlerCalled = false;
	bool dispatched = dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& e) {
		handlerCalled = true;
		EXPECT_EQ(e.GetKeyCode(), KeyCode::W);
		return true; // Handled
	});

	// Assert
	EXPECT_TRUE(dispatched);
	EXPECT_TRUE(handlerCalled);
	EXPECT_TRUE(keyEvent.Handled);

	// Act & Assert (WindowResizeEvent string representation)
	WindowResizeEvent resizeEvent(1920, 1080);
	EXPECT_EQ(resizeEvent.GetWidth(), 1920u);
	EXPECT_EQ(resizeEvent.GetHeight(), 1080u);
	EXPECT_NE(resizeEvent.ToString().find("1920"), std::string::npos);
}

// ============================================================================
// 2. EVENT QUEUE AND NEGATIVE CASES (Queue FIFO, Type Mismatches)
// ============================================================================
TEST(EventSystemModule, Negative_TypeMismatchAndQueueProcessing)
{
	// Arrange
	KeyPressedEvent keyEvent(KeyCode::Space, false);
	EventDispatcher dispatcher(keyEvent);
	bool wrongHandlerCalled = false;

	// Act (Dispatch to mismatched event type)
	bool dispatched = dispatcher.Dispatch<WindowResizeEvent>([&](WindowResizeEvent&) {
		wrongHandlerCalled = true;
		return true;
	});

	// Assert
	EXPECT_FALSE(dispatched);
	EXPECT_FALSE(wrongHandlerCalled);
	EXPECT_FALSE(keyEvent.Handled);

	// Arrange & Act (EventQueue FIFO processing)
	EventQueue queue;
	EXPECT_TRUE(queue.IsEmpty());

	queue.Push(std::make_unique<WindowCloseEvent>());
	queue.Enqueue<KeyPressedEvent>(KeyCode::A, false);

	EXPECT_FALSE(queue.IsEmpty());

	int count = 0;
	queue.Process([&](Event& e) {
		++count;
		if (count == 1)
		{
			EXPECT_EQ(e.GetEventType(), EventType::WindowClose);
		}
		else if (count == 2)
		{
			EXPECT_EQ(e.GetEventType(), EventType::KeyPressed);
		}
	});

	// Assert
	EXPECT_EQ(count, 2);
	EXPECT_TRUE(queue.IsEmpty());
}
