#ifndef CH_JOLT_PHYSICS_WORLD_H
#define CH_JOLT_PHYSICS_WORLD_H

#include "iphysics_world.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Chained
{

	// Jolt Physics backend. Wraps JPH::PhysicsSystem and provides body management,
	// raycasting, gravity control, and ground detection via contact callbacks.
	class JoltPhysicsWorld : public IPhysicsWorld
	{
	public:
		JoltPhysicsWorld();
		virtual ~JoltPhysicsWorld() override;

		virtual PhysicsBodyHandle CreateBody(const PhysicsBodyDesc& desc) override;
		virtual std::vector<PhysicsBodyHandle> CreateBodies(const std::vector<PhysicsBodyDesc>& descs) override;
		virtual void DestroyBody(PhysicsBodyHandle handle) override;

		virtual void SetTransform(PhysicsBodyHandle handle, const glm::vec3& pos, const glm::quat& rot) override;
		virtual void GetTransform(PhysicsBodyHandle handle, glm::vec3& pos, glm::quat& rot) override;

		virtual void SetVelocity(PhysicsBodyHandle handle, const glm::vec3& velocity) override;
		virtual glm::vec3 GetVelocity(PhysicsBodyHandle handle) const override;

		virtual RaycastResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) override;

		virtual void Step(float fixedDt) override;

		virtual void SetGravity(float gravity) override;

		virtual bool IsBodyGrounded(PhysicsBodyHandle handle) const override;
		virtual bool IsBodyActive(PhysicsBodyHandle handle) const override;

		/// Clear all grounded state (call before/after world reset).
		virtual void ClearGroundedState() override;

		virtual bool HasCachedMeshShape(const std::string& key) const override;
		virtual bool IsShapeBaking(const std::string& key) const override;
		virtual bool HasPendingShapeBakes() const override;
		virtual void PrebuildShape(const PhysicsBodyDesc& desc) override;
		virtual void QueuePrebuildShape(const PhysicsBodyDesc& desc) override;

		/// Clear the cached mesh shapes.
		void ClearShapeCache();

		std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> GetMeshShapeCache() const
		{
			std::lock_guard<std::mutex> lock(m_CacheMutex);
			return m_MeshShapeCache;
		}

		std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> GetConvexHullCache() const
		{
			std::lock_guard<std::mutex> lock(m_CacheMutex);
			return m_ConvexHullCache;
		}

		void RestoreShapeCache(std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> meshCache,
							   std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> convexCache)
		{
			std::lock_guard<std::mutex> lock(m_CacheMutex);
			m_MeshShapeCache = std::move(meshCache);
			m_ConvexHullCache = std::move(convexCache);
		}

		/// Preserves the compiled BVH mesh shape cache from a previous world instance.
		void PreserveShapeCacheFrom(const JoltPhysicsWorld& otherWorld)
		{
			std::lock_guard<std::mutex> lock1(m_CacheMutex);
			std::lock_guard<std::mutex> lock2(otherWorld.m_CacheMutex);
			m_MeshShapeCache = otherWorld.m_MeshShapeCache;
			m_ConvexHullCache = otherWorld.m_ConvexHullCache;
		}

		/// Builds a Jolt shape from a PhysicsBodyDesc (used by CreateBody and CreateBodies).
		JPH::ShapeRefC BuildShape(const PhysicsBodyDesc& desc);

		/// Builds BodyCreationSettings from a desc + pre-built shape.
		JPH::BodyCreationSettings BuildBodySettings(const PhysicsBodyDesc& desc, JPH::ShapeRefC shape);

	private:
		/// Creates a unit-box fallback shape and logs a warning.
		JPH::ShapeRefC FallbackUnitBox(const std::string& reason);

	private:
		// ── Jolt subsystems ──────────────────────────────────────────────────────
		// m_Factory declared first → destroyed last (C++ reverse destruction order).
		// PhysicsSystem must be destroyed before the factory.
		std::unique_ptr<JPH::Factory> m_Factory;
		JPH::PhysicsSystem m_PhysicsSystem;
		std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;

		// ── Ground detection ──────────────────────────────────────────────────────
		// Set of Jolt body IDs (packed as uint32) that have at least one ground
		// contact — i.e. a contact whose normal Y component is positive (pointing
		// upward relative to the body being checked).
		// Protected by m_GroundedMutex: Jolt calls contact callbacks from worker threads.
		std::unordered_set<uint32_t> m_GroundedBodies;
		mutable std::mutex m_GroundedMutex;

		// ── Mesh shape cache ─────────────────────────────────────────────────────
		// Built MeshShapes are cached per triangle fingerprint (model path + scale)
		// so that multiple bodies using the same mesh share a single BVH build.
		mutable std::mutex m_CacheMutex;
		std::condition_variable m_BakeCondition;
		bool m_IsShuttingDown = false;
		std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> m_MeshShapeCache;
		std::unordered_set<std::string> m_InFlightMeshBakes;

		// ── Convex hull shape cache ─────────────────────────────────────────────
		// For dynamic meshes, a ConvexHull is built from deduped vertices. Cache it
		// per model path so identical dynamic bodies reuse the same hull.
		std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> m_ConvexHullCache;

		// Contact listener that populates m_GroundedBodies.
		class ContactListenerImpl : public JPH::ContactListener
		{
		public:
			JPH::ValidateResult OnContactValidate(const JPH::Body&, const JPH::Body&, JPH::RVec3Arg,
												  const JPH::CollideShapeResult&) override;
			void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
								const JPH::ContactManifold& inManifold, JPH::ContactSettings& inSettings) override;
			void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2,
									const JPH::ContactManifold& inManifold, JPH::ContactSettings& inSettings) override;
			void OnContactRemoved(const JPH::SubShapeIDPair& inPair) override;

			/// Set the tracker that receives ground-contact updates.
			void SetGroundedTracker(std::unordered_set<uint32_t>* tracker, std::mutex* mutex)
			{
				m_Tracker = tracker;
				m_Mutex = mutex;
			}

		private:
			std::unordered_set<uint32_t>* m_Tracker = nullptr;
			std::mutex* m_Mutex = nullptr;
		};

		ContactListenerImpl m_ContactListener;
	};

} // namespace Chained

#endif // CH_JOLT_PHYSICS_WORLD_H
