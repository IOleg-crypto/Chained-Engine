#include "engine/core/events/events.h"
#include "engine/core/events/input_events.h"
#include "engine/core/events/window_events.h"
#include "engine/core/key_codes.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(EventSystemTest, KeyPressedEventCreation)
{
	KeyPressedEvent event(KeyCode::W, false);
	EXPECT_EQ(event.GetKeyCode(), KeyCode::W);
	EXPECT_FALSE(event.IsRepeat());
	EXPECT_EQ(event.GetEventType(), EventType::KeyPressed);
	EXPECT_TRUE(event.IsInCategory(EventCategoryKeyboard));
	EXPECT_TRUE(event.IsInCategory(EventCategoryInput));
}

TEST(EventSystemTest, EventDispatcherKeyPressed)
{
	KeyPressedEvent event(KeyCode::W, false);
	bool handlerCalled = false;
	int receivedKey = -1;

	EventDispatcher dispatcher(event);
	const bool dispatched = dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& e) {
		handlerCalled = true;
		receivedKey = (int)e.GetKeyCode();
		return true;
	});

	EXPECT_TRUE(dispatched);
	EXPECT_TRUE(handlerCalled);
	EXPECT_EQ(receivedKey, (int)KeyCode::W);
	EXPECT_TRUE(event.Handled);
}

TEST(EventSystemTest, EventDispatcherWrongType)
{
	KeyPressedEvent event(KeyCode::W, false);
	bool handlerCalled = false;

	EventDispatcher dispatcher(event);
	const bool dispatched = dispatcher.Dispatch<WindowResizeEvent>([&](WindowResizeEvent&) {
		handlerCalled = true;
		return true;
	});

	EXPECT_FALSE(dispatched);
	EXPECT_FALSE(handlerCalled);
	EXPECT_FALSE(event.Handled);
}

TEST(EventSystemTest, EventDispatcherMultipleHandlers)
{
	KeyPressedEvent event(KeyCode::Space, false);
	int handlerCount = 0;

	EventDispatcher dispatcher(event);

	dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& e) {
		handlerCount++;
		return false; // Don't mark as handled
	});

	dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& e) {
		handlerCount++;
		return true; // Mark as handled
	});

	EXPECT_EQ(handlerCount, 2);
	EXPECT_TRUE(event.Handled);
}

TEST(EventSystemTest, WindowResizeToStringIncludesDimensions)
{
	WindowResizeEvent event(1280, 720);

	EXPECT_EQ(event.GetWidth(), 1280u);
	EXPECT_EQ(event.GetHeight(), 720u);
	EXPECT_NE(event.ToString().find("1280"), std::string::npos);
	EXPECT_NE(event.ToString().find("720"), std::string::npos);
}
