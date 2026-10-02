#include "engine/common/timestep.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(TimestepTest, DefaultConstruction)
{
	Timestep ts;
	EXPECT_FLOAT_EQ(ts.GetSeconds(), 0.0f);
	EXPECT_FLOAT_EQ(ts.GetMilliseconds(), 0.0f);
}

TEST(TimestepTest, ValueConstruction)
{
	Timestep ts(0.016f);
	EXPECT_FLOAT_EQ(ts.GetSeconds(), 0.016f);
	EXPECT_FLOAT_EQ(ts.GetMilliseconds(), 16.0f);
}

TEST(TimestepTest, ImplicitFloatConversion)
{
	Timestep ts(0.5f);
	float value = ts;
	EXPECT_FLOAT_EQ(value, 0.5f);
}

TEST(TimestepTest, NegativeDelta)
{
	Timestep ts(-1.0f);
	EXPECT_FLOAT_EQ(ts.GetSeconds(), -1.0f);
}

TEST(TimestepTest, LargeDelta)
{
	Timestep ts(1000.0f);
	EXPECT_FLOAT_EQ(ts.GetSeconds(), 1000.0f);
	EXPECT_FLOAT_EQ(ts.GetMilliseconds(), 1000000.0f);
}
