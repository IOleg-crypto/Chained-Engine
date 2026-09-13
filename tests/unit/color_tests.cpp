#include "engine/common/color.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(ColorTest, DefaultConstructor)
{
	Color c;
	EXPECT_EQ(c.r, 0);
	EXPECT_EQ(c.g, 0);
	EXPECT_EQ(c.b, 0);
	EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, ParameterizedConstructor)
{
	Color c(100, 150, 200, 128);
	EXPECT_EQ(c.r, 100);
	EXPECT_EQ(c.g, 150);
	EXPECT_EQ(c.b, 200);
	EXPECT_EQ(c.a, 128);
}

TEST(ColorTest, DefaultAlphaIs255)
{
	Color c(255, 0, 0);
	EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, NamedColors)
{
	auto white = Color::White();
	EXPECT_EQ(white.r, 255);
	EXPECT_EQ(white.g, 255);
	EXPECT_EQ(white.b, 255);
	EXPECT_EQ(white.a, 255);

	auto black = Color::Black();
	EXPECT_EQ(black.r, 0);
	EXPECT_EQ(black.g, 0);
	EXPECT_EQ(black.b, 0);
	EXPECT_EQ(black.a, 255);

	auto red = Color::Red();
	EXPECT_EQ(red.r, 255);
	EXPECT_EQ(red.g, 0);
	EXPECT_EQ(red.b, 0);

	auto green = Color::Green();
	EXPECT_EQ(green.r, 0);
	EXPECT_EQ(green.g, 255);
	EXPECT_EQ(green.b, 0);

	auto blue = Color::Blue();
	EXPECT_EQ(blue.r, 0);
	EXPECT_EQ(blue.g, 0);
	EXPECT_EQ(blue.b, 255);
}

TEST(ColorTest, MaxValues)
{
	Color c(255, 255, 255, 255);
	EXPECT_EQ(c.r, 255);
	EXPECT_EQ(c.g, 255);
	EXPECT_EQ(c.b, 255);
	EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, NegativeValues)
{
	Color c(-1, -1, -1, -1);
	EXPECT_EQ(c.r, 255); // Underflow to 255
	EXPECT_EQ(c.g, 255);
	EXPECT_EQ(c.b, 255);
	EXPECT_EQ(c.a, 255);
}
