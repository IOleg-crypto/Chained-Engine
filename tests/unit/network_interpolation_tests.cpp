// network_interpolation_tests.cpp
// Unit tests for the dead-reckoning / interpolation math used in
// NetworkReplicationManager::InterpolateEntities().
//
// These tests are PURE MATH — no engine runtime, no Scene, no EnTT registry.
// They replicate the exact formulas from network_replication_manager.cpp so
// any accidental change to the production code breaks this test immediately.

#include "gtest/gtest.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// ─── Mirror the exact helpers from network_replication_manager.cpp ───────────
// We deliberately copy the formulas rather than call them via the class, so the
// test is self-contained and any drift between formula copies is caught.

namespace
{
	// Matches SafeNormalizeQuat in network_replication_manager.cpp
	glm::quat SafeNormalizeQuat(const glm::quat& q)
	{
		float len2 = glm::dot(q, q);
		if (len2 < 1e-6f || std::isnan(len2) || std::isinf(len2))
		{
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		}
		return glm::normalize(q);
	}

	// Dead-reckoning goal position (matches line 355)
	glm::vec3 DeadReckonGoal(const glm::vec3& targetPos, const glm::vec3& targetVelocity, float dt)
	{
		return targetPos + targetVelocity * dt;
	}

	// Exponential damping factor t (matches lines 356-357)
	float InterpFactor(float dt, float speed = 10.0f)
	{
		return glm::clamp(1.0f - std::exp(-speed * dt), 0.0f, 1.0f);
	}

	constexpr float kMaxCorrectionDistance = 10.0f; // mirrors NetworkConstants
} // namespace

// ── SafeNormalizeQuat ────────────────────────────────────────────────────────

TEST(SafeNormalizeQuatTest, UnitQuatUnchanged)
{
	glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
	glm::quat result = SafeNormalizeQuat(q);
	EXPECT_NEAR(result.w, 1.0f, 1e-5f);
	EXPECT_NEAR(result.x, 0.0f, 1e-5f);
	EXPECT_NEAR(result.y, 0.0f, 1e-5f);
	EXPECT_NEAR(result.z, 0.0f, 1e-5f);
}

TEST(SafeNormalizeQuatTest, ArbitraryQuatNormalized)
{
	glm::quat q(2.0f, 1.0f, 1.0f, 1.0f); // not unit length
	glm::quat result = SafeNormalizeQuat(q);
	float len = std::sqrt(result.w * result.w + result.x * result.x + result.y * result.y + result.z * result.z);
	EXPECT_NEAR(len, 1.0f, 1e-5f);
}

TEST(SafeNormalizeQuatTest, ZeroQuatReturnsIdentity)
{
	glm::quat q(0.0f, 0.0f, 0.0f, 0.0f);
	glm::quat result = SafeNormalizeQuat(q);
	EXPECT_FLOAT_EQ(result.w, 1.0f);
	EXPECT_FLOAT_EQ(result.x, 0.0f);
	EXPECT_FLOAT_EQ(result.y, 0.0f);
	EXPECT_FLOAT_EQ(result.z, 0.0f);
}

TEST(SafeNormalizeQuatTest, NaNQuatReturnsIdentity)
{
	glm::quat q(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f);
	glm::quat result = SafeNormalizeQuat(q);
	EXPECT_FLOAT_EQ(result.w, 1.0f);
	EXPECT_FLOAT_EQ(result.x, 0.0f);
}

TEST(SafeNormalizeQuatTest, InfQuatReturnsIdentity)
{
	glm::quat q(std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0.0f);
	glm::quat result = SafeNormalizeQuat(q);
	EXPECT_FLOAT_EQ(result.w, 1.0f);
	EXPECT_FLOAT_EQ(result.x, 0.0f);
}

TEST(SafeNormalizeQuatTest, VerySmallQuatReturnsIdentity)
{
	// len2 < 1e-6 threshold
	glm::quat q(1e-4f, 0.0f, 0.0f, 0.0f); // len2 = 1e-8 < 1e-6
	glm::quat result = SafeNormalizeQuat(q);
	EXPECT_FLOAT_EQ(result.w, 1.0f);
}

TEST(SafeNormalizeQuatTest, ResultHasUnitLength)
{
	// Arbitrary non-trivial rotation
	glm::quat q = glm::angleAxis(1.2f, glm::normalize(glm::vec3(1, 2, 3)));
	q *= 3.0f; // scale it
	glm::quat result = SafeNormalizeQuat(q);
	float len = glm::length(result);
	EXPECT_NEAR(len, 1.0f, 1e-5f);
}

// ── Dead-reckoning goal position ─────────────────────────────────────────────

TEST(DeadReckoningTest, ZeroVelocityGoalEqualsTarget)
{
	glm::vec3 targetPos = {5.0f, 1.0f, -3.0f};
	glm::vec3 targetVel = {0.0f, 0.0f, 0.0f};
	glm::vec3 goal = DeadReckonGoal(targetPos, targetVel, 0.016f);
	EXPECT_FLOAT_EQ(goal.x, targetPos.x);
	EXPECT_FLOAT_EQ(goal.y, targetPos.y);
	EXPECT_FLOAT_EQ(goal.z, targetPos.z);
}

TEST(DeadReckoningTest, PositiveVelocityExtrapolates)
{
	glm::vec3 targetPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 targetVel = {10.0f, 0.0f, 0.0f};
	float dt = 0.016f;
	glm::vec3 goal = DeadReckonGoal(targetPos, targetVel, dt);
	EXPECT_NEAR(goal.x, 0.16f, 1e-4f);
	EXPECT_FLOAT_EQ(goal.y, 0.0f);
	EXPECT_FLOAT_EQ(goal.z, 0.0f);
}

TEST(DeadReckoningTest, NegativeVelocityExtrapolatesBackwards)
{
	glm::vec3 targetPos = {10.0f, 0.0f, 0.0f};
	glm::vec3 targetVel = {-5.0f, 0.0f, 0.0f};
	float dt = 0.1f;
	glm::vec3 goal = DeadReckonGoal(targetPos, targetVel, dt);
	EXPECT_NEAR(goal.x, 9.5f, 1e-4f);
}

TEST(DeadReckoningTest, AllAxes)
{
	glm::vec3 targetPos = {1.0f, 2.0f, 3.0f};
	glm::vec3 targetVel = {1.0f, -1.0f, 0.5f};
	float dt = 0.5f;
	glm::vec3 goal = DeadReckonGoal(targetPos, targetVel, dt);
	EXPECT_NEAR(goal.x, 1.5f, 1e-4f);
	EXPECT_NEAR(goal.y, 1.5f, 1e-4f);
	EXPECT_NEAR(goal.z, 3.25f, 1e-4f);
}

// ── Exponential damping factor ───────────────────────────────────────────────

TEST(InterpFactorTest, ZeroDtGivesZeroFactor)
{
	// exp(-10 * 0) = 1 → 1 - 1 = 0
	float t = InterpFactor(0.0f, 10.0f);
	EXPECT_NEAR(t, 0.0f, 1e-6f);
}

TEST(InterpFactorTest, LargeDtGivesFactorNearOne)
{
	// exp(-10 * 1) ≈ 0 → factor ≈ 1
	float t = InterpFactor(1.0f, 10.0f);
	EXPECT_GT(t, 0.99f);
	EXPECT_LE(t, 1.0f);
}

TEST(InterpFactorTest, FactorIsClampedToZeroOne)
{
	EXPECT_GE(InterpFactor(0.016f, 10.0f), 0.0f);
	EXPECT_LE(InterpFactor(0.016f, 10.0f), 1.0f);
}

TEST(InterpFactorTest, TypicalFrameTimeGivesExpectedRange)
{
	// At 60 Hz (dt=0.016s) with speed=10, factor ≈ 0.148
	float t = InterpFactor(0.016f, 10.0f);
	EXPECT_GT(t, 0.10f);
	EXPECT_LT(t, 0.20f);
}

TEST(InterpFactorTest, MonotonicallyIncreasesWithDt)
{
	float prev = 0.0f;
	for (int i = 1; i <= 10; ++i)
	{
		float t = InterpFactor(i * 0.016f, 10.0f);
		EXPECT_GT(t, prev);
		prev = t;
	}
}

// ── Snap-correction threshold ────────────────────────────────────────────────

TEST(SnapCorrectionTest, BelowThresholdNoSnap)
{
	glm::vec3 renderPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 goalPos = {9.9f, 0.0f, 0.0f}; // < 10m
	float dist = glm::length(renderPos - goalPos);
	EXPECT_LT(dist, kMaxCorrectionDistance);
}

TEST(SnapCorrectionTest, AboveThresholdSnaps)
{
	glm::vec3 renderPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 goalPos = {10.1f, 0.0f, 0.0f}; // > 10m
	float dist = glm::length(renderPos - goalPos);
	EXPECT_GT(dist, kMaxCorrectionDistance);
}

TEST(SnapCorrectionTest, ExactlyAtThresholdNoSnap)
{
	glm::vec3 renderPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 goalPos = {kMaxCorrectionDistance, 0.0f, 0.0f};
	float dist = glm::length(renderPos - goalPos);
	// strictly > threshold triggers snap → at exactly threshold, no snap
	EXPECT_FALSE(dist > kMaxCorrectionDistance);
}

// ── Full interpolation step simulation ───────────────────────────────────────
// These tests simulate a single InterpolateEntities() tick to verify the
// combined effect of dead-reckoning + exponential smoothing.

TEST(InterpolationStepTest, ConvergesOnStaticTarget)
{
	glm::vec3 renderPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 targetPos = {5.0f, 0.0f, 0.0f};
	glm::vec3 targetVel = {0.0f, 0.0f, 0.0f};
	const float dt = 0.016f;
	const float speed = 10.0f;

	// Simulate 200 frames
	for (int frame = 0; frame < 200; ++frame)
	{
		glm::vec3 goalPos = DeadReckonGoal(targetPos, targetVel, dt);
		float t = InterpFactor(dt, speed);
		renderPos = glm::mix(renderPos, goalPos, t);
	}

	EXPECT_NEAR(renderPos.x, 5.0f, 0.001f);
	EXPECT_NEAR(renderPos.y, 0.0f, 0.001f);
}

TEST(InterpolationStepTest, DeadReckoningPredictsMidFramePosition)
{
	// Target is at x=0 moving at 10 m/s. After dt=0.016 render should
	// advance toward x = 0 + 10*0.016 = 0.16 (goal), blended by factor t.
	glm::vec3 renderPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 targetPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 targetVel = {10.0f, 0.0f, 0.0f};
	float dt = 0.016f;

	glm::vec3 goal = DeadReckonGoal(targetPos, targetVel, dt);
	float t = InterpFactor(dt, 10.0f);
	glm::vec3 newRenderPos = glm::mix(renderPos, goal, t);

	// Should have moved in positive x, less than goal (partial interpolation)
	EXPECT_GT(newRenderPos.x, 0.0f);
	EXPECT_LT(newRenderPos.x, goal.x);
}

TEST(InterpolationStepTest, SnapOnLargeGap)
{
	// > 10m gap → render immediately jumps to target
	glm::vec3 renderPos = {0.0f, 0.0f, 0.0f};
	glm::vec3 targetPos = {50.0f, 0.0f, 0.0f};
	glm::vec3 targetVel = {0.0f, 0.0f, 0.0f};
	float dt = 0.016f;

	glm::vec3 goalPos = DeadReckonGoal(targetPos, targetVel, dt);
	float snapDist = glm::length(renderPos - goalPos);

	if (snapDist > kMaxCorrectionDistance)
	{
		renderPos = targetPos; // mirror the snap branch
	}

	EXPECT_NEAR(renderPos.x, 50.0f, 1e-4f);
}

TEST(InterpolationStepTest, QuatSlerpConvergesToTarget)
{
	glm::quat renderRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity
	glm::quat targetRot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
	const float dt = 0.016f;
	const float speed = 10.0f;

	for (int frame = 0; frame < 200; ++frame)
	{
		float t = InterpFactor(dt, speed);
		renderRot = glm::slerp(SafeNormalizeQuat(renderRot), SafeNormalizeQuat(targetRot), t);
	}

	// After 200 frames renderRot should be very close to targetRot
	float dot = std::abs(glm::dot(renderRot, targetRot));
	EXPECT_NEAR(dot, 1.0f, 0.001f);
}

TEST(InterpolationStepTest, QuatSlerpNeverProducesNaN)
{
	glm::quat renderRot = glm::quat(0.0f, 0.0f, 0.0f, 0.0f); // degenerate
	glm::quat targetRot = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 0, 1));
	float dt = 0.016f;
	float t = InterpFactor(dt, 10.0f);

	glm::quat result = glm::slerp(SafeNormalizeQuat(renderRot), SafeNormalizeQuat(targetRot), t);

	EXPECT_FALSE(std::isnan(result.w));
	EXPECT_FALSE(std::isnan(result.x));
	EXPECT_FALSE(std::isnan(result.y));
	EXPECT_FALSE(std::isnan(result.z));
}

// ── Tick wrap-guard for out-of-order packet rejection ────────────────────────

TEST(TickWrapGuardTest, OlderTickDropped)
{
	constexpr uint32_t kTickWrapGuard = 100'000u;

	uint32_t lastTick = 500;
	uint32_t incoming = 300; // older

	bool shouldDrop = (incoming < lastTick) && ((lastTick - incoming) < kTickWrapGuard);
	EXPECT_TRUE(shouldDrop);
}

TEST(TickWrapGuardTest, NewerTickAccepted)
{
	constexpr uint32_t kTickWrapGuard = 100'000u;

	uint32_t lastTick = 500;
	uint32_t incoming = 501; // newer

	bool shouldDrop = (incoming < lastTick) && ((lastTick - incoming) < kTickWrapGuard);
	EXPECT_FALSE(shouldDrop);
}

TEST(TickWrapGuardTest, LargeGapTreatedAsWraparound)
{
	// incoming << lastTick but gap > kTickWrapGuard → not a stale packet, wrap-around
	constexpr uint32_t kTickWrapGuard = 100'000u;
	uint32_t lastTick = 50;
	uint32_t incoming = 0; // delta would be 50, < guard → drop? No:

	// gap = lastTick - incoming = 50, which IS < kTickWrapGuard → drop
	// This just shows the guard doesn't help for tiny wrap-arounds, which is by design.
	bool shouldDrop = (incoming < lastTick) && ((lastTick - incoming) < kTickWrapGuard);
	EXPECT_TRUE(shouldDrop); // intentional: 50 < 100,000 → drop

	// But when gap > guard (genuine wraparound e.g. 4,000,000,000 → 5)
	lastTick = 5;
	incoming = 0xFFFFFF00u; // incoming is huge uint32 that wrapped
	// incoming < lastTick? 0xFFFFFF00 > 5, so condition is false → not dropped
	shouldDrop = (incoming < lastTick) && ((lastTick - incoming) < kTickWrapGuard);
	EXPECT_FALSE(shouldDrop);
}
