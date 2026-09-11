#include "jolt_physics_world.h"

#include "engine/core/service_locator.h"
#include "engine/common/thread_pool.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/RegisterTypes.h>

#include <cmath>
#include <cstdarg>
#include <future>
#include <mutex>
#include "engine/core/log.h"

namespace Chained
{

	// ─────────────────────────────────────────────────────────────────────────────
	// Layer setup — two layers: NON_MOVING (static) and MOVING (dynamic/kinematic)
	// ─────────────────────────────────────────────────────────────────────────────
	namespace Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	} // namespace Layers

	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
		{
			switch (a)
			{
			case Layers::NON_MOVING:
				return b == Layers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				return false;
			}
		}
	};

	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr uint32_t NUM_LAYERS = 2;
	} // namespace BroadPhaseLayers

	class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			m_Map[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			m_Map[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}
		uint32_t GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}
		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
		{
			return m_Map[layer];
		}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
		{
			return (JPH::BroadPhaseLayer::Type)layer == 0 ? "NON_MOVING" : "MOVING";
		}
#endif
	private:
		JPH::BroadPhaseLayer m_Map[Layers::NUM_LAYERS];
	};

	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bpLayer) const override
		{
			switch (layer)
			{
			case Layers::NON_MOVING:
				return bpLayer == BroadPhaseLayers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				return false;
			}
		}
	};

	// ─────────────────────────────────────────────────────────────────────────────
	// File-scope singletons — survive JoltPhysicsWorld::ResetWorld() rebuilds
	// ─────────────────────────────────────────────────────────────────────────────
	static BPLayerInterfaceImpl s_BPLayerInterface;
	static ObjectVsBroadPhaseLayerFilterImpl s_ObjVsBPFilter;
	static ObjectLayerPairFilterImpl s_ObjVsObjFilter;

	static void JoltTraceImpl(const char* inFMT, ...)
	{
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		va_end(list);
		CH_CORE_INFO("[Jolt] {}", buffer);
	}

#ifdef JPH_ENABLE_ASSERTS
	static bool JoltAssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile,
									 JPH::uint inLine)
	{
		CH_CORE_ERROR("[Jolt Assert] {}:{} ({}) {}", inFile, inLine, inExpression, inMessage ? inMessage : "");
		return false;
	}
#endif

	// ─────────────────────────────────────────────────────────────────────────────
	// JoltPhysicsWorld
	// ─────────────────────────────────────────────────────────────────────────────
	JoltPhysicsWorld::JoltPhysicsWorld()
	{
		JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
		JPH::AssertFailed = JoltAssertFailedImpl;
#endif

		// Jolt factory + type registration (owned by this world instance)
		JPH::RegisterDefaultAllocator();
		m_Factory = std::make_unique<JPH::Factory>();
		JPH::Factory::sInstance = m_Factory.get();
		JPH::RegisterTypes();

		m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(32 * 1024 * 1024);
		unsigned int hwThreads = std::thread::hardware_concurrency();
		int numThreads = std::max(1, (int)hwThreads - 1);
		m_JobSystem =
			std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads);

		m_PhysicsSystem.Init(65536, 0, 65536, 65536, s_BPLayerInterface, s_ObjVsBPFilter, s_ObjVsObjFilter);

		m_PhysicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

		m_ContactListener.SetGroundedTracker(&m_GroundedBodies, &m_GroundedMutex);
		m_PhysicsSystem.SetContactListener(&m_ContactListener);

		CH_CORE_INFO("Jolt Physics World Initialized with ContactListener.");
	}

	JoltPhysicsWorld::~JoltPhysicsWorld()
	{
		// 1. Signal background tasks that this world is shutting down and wait for in-flight tasks to finish
		{
			std::unique_lock<std::mutex> lock(m_CacheMutex);
			m_IsShuttingDown = true;
			m_BakeCondition.wait(lock, [this]() { return m_InFlightMeshBakes.empty(); });
		}

		// 2. PhysicsSystem destroyed automatically (member variable).
		// Factory is destroyed last due to declaration order.
		JPH::Factory::sInstance = nullptr;
	}

	JPH::ShapeRefC JoltPhysicsWorld::FallbackUnitBox(const std::string& reason)
	{
		CH_CORE_WARN("Physics: {} — falling back to unit box.", reason);
		auto shape = JPH::BoxShapeSettings(JPH::Vec3(0.5f, 0.5f, 0.5f)).Create().Get();
		if (!shape)
		{
			CH_CORE_ERROR("Physics: unit-box fallback shape creation failed.");
		}
		return shape;
	}

	void JoltPhysicsWorld::ClearShapeCache()
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);
		m_MeshShapeCache.clear();
		m_ConvexHullCache.clear();
	}

	void JoltPhysicsWorld::PrebuildShape(const PhysicsBodyDesc& desc)
	{
		BuildShape(desc);
	}

	void JoltPhysicsWorld::QueuePrebuildShape(const PhysicsBodyDesc& desc)
	{
		if (desc.CacheKey.empty())
		{
			return;
		}

		{
			std::lock_guard lock(m_CacheMutex);
			if (m_IsShuttingDown)
			{
				return;
			}
			if (m_MeshShapeCache.count(desc.CacheKey) > 0 || m_ConvexHullCache.count(desc.CacheKey) > 0 ||
				m_ConvexHullCache.count(desc.CacheKey + "_convex") > 0 || m_InFlightMeshBakes.count(desc.CacheKey) > 0)
			{
				return;
			}
			m_InFlightMeshBakes.insert(desc.CacheKey);
		}

		if (auto* tp = ServiceLocator::TryGet<ThreadPool>())
		{
			tp->QueueTask([this, desc]() {
				// Check if world is still alive before building
				{
					std::lock_guard lock(m_CacheMutex);
					if (m_IsShuttingDown)
					{
						m_InFlightMeshBakes.erase(desc.CacheKey);
						m_BakeCondition.notify_all();
						return;
					}
				}

				PrebuildShape(desc);

				{
					std::lock_guard lock(m_CacheMutex);
					m_InFlightMeshBakes.erase(desc.CacheKey);
					m_BakeCondition.notify_all();
				}
			});
		}
		else
		{
			PrebuildShape(desc);
			std::lock_guard lock(m_CacheMutex);
			m_InFlightMeshBakes.erase(desc.CacheKey);
			m_BakeCondition.notify_all();
		}
	}

	JPH::ShapeRefC JoltPhysicsWorld::BuildShape(const PhysicsBodyDesc& desc)
	{
		JPH::ShapeRefC shape;

		switch (desc.Shape)
		{
		case ColliderType::Box: {
			// Use HasError() check — calling .Get() on a failed Result triggers
			// JPH_ASSERT(IsValid()), which was happening when AutoCalculate produced
			// a zero-sized box (model not yet Ready).
			auto result =
				JPH::BoxShapeSettings(JPH::Vec3(desc.Dimensions.x, desc.Dimensions.y, desc.Dimensions.z)).Create();
			shape = result.HasError()
						? FallbackUnitBox("Box shape creation failed: " + std::string(result.GetError().c_str()))
						: result.Get();
			break;
		}
		case ColliderType::Sphere: {
			auto result = JPH::SphereShapeSettings(desc.Dimensions.x).Create();
			shape = result.HasError()
						? FallbackUnitBox("Sphere shape creation failed: " + std::string(result.GetError().c_str()))
						: result.Get();
			break;
		}
		case ColliderType::Capsule: {
			auto result = JPH::CapsuleShapeSettings(desc.Dimensions.y, desc.Dimensions.x).Create();
			shape = result.HasError()
						? FallbackUnitBox("Capsule shape creation failed: " + std::string(result.GetError().c_str()))
						: result.Get();
			break;
		}
		case ColliderType::Mesh: {
			if (!desc.IsStatic)
			{
				// Convex hull path for dynamic meshes — check cache first
				std::string convexKey = desc.CacheKey.empty() ? "" : desc.CacheKey + "_convex";
				if (!convexKey.empty())
				{
					std::lock_guard<std::mutex> lock(m_CacheMutex);
					auto it = m_ConvexHullCache.find(convexKey);
					if (it != m_ConvexHullCache.end())
					{
						shape = it->second;
						if (desc.MeshScale != glm::vec3(1.0f))
						{
							JPH::ScaledShapeSettings scaledSettings(
								shape, JPH::Vec3(desc.MeshScale.x, desc.MeshScale.y, desc.MeshScale.z));
							auto scaledResult = scaledSettings.Create();
							if (!scaledResult.HasError())
							{
								shape = scaledResult.Get();
							}
						}
						CH_CORE_TRACE("Physics: Reused cached convex hull [key='{}']", convexKey);
						break;
					}
				}

				if (desc.Triangles.empty())
				{
					shape = FallbackUnitBox("Dynamic MeshShape requested but no triangles provided and not in cache");
					break;
				}

				constexpr float kGridCell = 0.05f;
				std::unordered_set<uint64_t> seen;
				JPH::Array<JPH::Vec3> points;

				auto tryAdd = [&](const glm::vec3& p) {
					auto quant = [](float v) { return (int64_t)std::floor(v / kGridCell); };
					uint64_t key = (uint64_t)(quant(p.x) & 0x1FFFFF) | ((uint64_t)(quant(p.y) & 0x1FFFFF) << 21) |
								   ((uint64_t)(quant(p.z) & 0x1FFFFF) << 42);
					if (seen.insert(key).second)
					{
						points.push_back(JPH::Vec3(p.x, p.y, p.z));
					}
				};

				for (const auto& t : desc.Triangles)
				{
					tryAdd(t.V0);
					tryAdd(t.V1);
					tryAdd(t.V2);
				}

				if (points.size() > 4096)
				{
					CH_CORE_WARN("Physics: Dynamic mesh has {} unique points after dedup ({} raw tris) — this looks "
								 "like static level geometry marked non-static. A convex hull will be a crude blob; "
								 "consider making this body Static or Kinematic instead.",
								 points.size(), desc.Triangles.size());
				}

				// Cap convex hull at 256 points — Jolt's hull builder struggles above this.
				// Downsample uniformly if needed.
				constexpr size_t kMaxConvexPoints = 256;
				if (points.size() > kMaxConvexPoints)
				{
					CH_CORE_WARN("Physics: Convex hull downsampled from {} to {} points.", points.size(),
								 kMaxConvexPoints);
					JPH::Array<JPH::Vec3> downsampled;
					downsampled.reserve(kMaxConvexPoints);
					size_t step = points.size() / kMaxConvexPoints;
					for (size_t i = 0; i < points.size() && downsampled.size() < kMaxConvexPoints; i += step)
					{
						downsampled.push_back(points[i]);
					}
					points = std::move(downsampled);
				}

				JPH::ConvexHullShapeSettings hullSettings(points);
				auto hullResult = hullSettings.Create();
				if (hullResult.HasError())
				{
					std::string err = hullResult.GetError().c_str();
					shape = FallbackUnitBox("ConvexHull build for dynamic mesh failed: " + err);
					break;
				}

				JPH::ShapeRefC hull = hullResult.Get();
				if (!convexKey.empty())
				{
					std::lock_guard<std::mutex> lock(m_CacheMutex);
					if (!m_IsShuttingDown)
					{
						m_ConvexHullCache[convexKey] = hull;
					}
				}

				shape = hull;
				if (desc.MeshScale != glm::vec3(1.0f))
				{
					JPH::ScaledShapeSettings scaledSettings(
						hull, JPH::Vec3(desc.MeshScale.x, desc.MeshScale.y, desc.MeshScale.z));
					auto scaledResult = scaledSettings.Create();
					if (!scaledResult.HasError())
					{
						shape = scaledResult.Get();
					}
					else
					{
						CH_CORE_WARN("Physics: ScaledShape build failed: {} — using unscaled hull.",
									 scaledResult.GetError().c_str());
					}
				}
				CH_CORE_INFO("Physics: Dynamic mesh body approximated with ConvexHull ({} points, {} raw tris).",
							 points.size(), desc.Triangles.size());
				break;
			}

			// Static mesh: BVH cache keyed by model path
			JPH::ShapeRefC baseShape;
			bool cached = false;
			if (!desc.CacheKey.empty())
			{
				std::lock_guard<std::mutex> lock(m_CacheMutex);
				auto it = m_MeshShapeCache.find(desc.CacheKey);
				if (it != m_MeshShapeCache.end())
				{
					baseShape = it->second;
					cached = true;
					CH_CORE_TRACE("Physics: Reused cached mesh BVH [key='{}']", desc.CacheKey);
				}
			}

			if (cached)
			{
				shape = baseShape;
				if (desc.MeshScale != glm::vec3(1.0f))
				{
					JPH::ScaledShapeSettings scaledSettings(
						baseShape, JPH::Vec3(desc.MeshScale.x, desc.MeshScale.y, desc.MeshScale.z));
					auto scaledResult = scaledSettings.Create();
					if (!scaledResult.HasError())
					{
						shape = scaledResult.Get();
					}
					else
					{
						CH_CORE_WARN(
							"Physics: ScaledShape build failed for cached shape: {} — using unscaled base shape.",
							scaledResult.GetError().c_str());
					}
				}
				break;
			}

			if (desc.Triangles.empty())
			{
				shape = FallbackUnitBox("Static MeshShape requested but no triangles provided and not in cache");
				break;
			}
			{
				const auto buildStart = std::chrono::steady_clock::now();
				JPH::TriangleList joltTris;
				joltTris.reserve(desc.Triangles.size());
				for (const auto& t : desc.Triangles)
				{
					JPH::Triangle tri(JPH::Float3(t.V0.x, t.V0.y, t.V0.z), JPH::Float3(t.V1.x, t.V1.y, t.V1.z),
									  JPH::Float3(t.V2.x, t.V2.y, t.V2.z));
					JPH::Vec3 v0 = JPH::Vec3::sLoadFloat3Unsafe(tri.mV[0]);
					JPH::Vec3 v1 = JPH::Vec3::sLoadFloat3Unsafe(tri.mV[1]);
					JPH::Vec3 v2 = JPH::Vec3::sLoadFloat3Unsafe(tri.mV[2]);
					JPH::Vec3 e1 = v1 - v0;
					JPH::Vec3 e2 = v2 - v0;
					if (e1.Cross(e2).LengthSq() < 1e-20f)
					{
						continue;
					}
					joltTris.push_back(tri);
				}

				if (joltTris.empty())
				{
					shape = FallbackUnitBox("All mesh triangles degenerate");
					break;
				}

				JPH::MeshShapeSettings s(std::move(joltTris));
				s.mBuildQuality = JPH::MeshShapeSettings::EBuildQuality::FavorBuildSpeed;
				auto result = s.Create();
				if (result.HasError())
				{
					std::string err = result.GetError().c_str();
					shape = FallbackUnitBox("MeshShape build failed: " + err);
					break;
				}

				baseShape = result.Get();
				if (!desc.CacheKey.empty())
				{
					std::lock_guard<std::mutex> lock(m_CacheMutex);
					if (!m_IsShuttingDown)
					{
						m_MeshShapeCache[desc.CacheKey] = baseShape;
					}
				}

				const auto buildEnd = std::chrono::steady_clock::now();
				const double buildMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();
				CH_CORE_INFO("Physics: Built mesh BVH ({} tris) in {:.2f} ms{} [key='{}']", joltTris.size(), buildMs,
							 desc.CacheKey.empty() ? " (uncached)" : " (freshly built, cached for reuse)",
							 desc.CacheKey);
			}

			shape = baseShape;
			if (desc.MeshScale != glm::vec3(1.0f))
			{
				JPH::ScaledShapeSettings scaledSettings(
					baseShape, JPH::Vec3(desc.MeshScale.x, desc.MeshScale.y, desc.MeshScale.z));
				auto scaledResult = scaledSettings.Create();
				if (!scaledResult.HasError())
				{
					shape = scaledResult.Get();
				}
				else
				{
					CH_CORE_WARN("Physics: ScaledShape build failed: {} — using unscaled mesh.",
								 scaledResult.GetError().c_str());
				}
			}
			break;
		}
		default: {
			shape = FallbackUnitBox("Unknown ColliderType");
			break;
		}
		}

		// Apply Offset if present
		if (shape && desc.Offset != glm::vec3(0.0f))
		{
			JPH::RotatedTranslatedShapeSettings offsetSettings(JPH::Vec3(desc.Offset.x, desc.Offset.y, desc.Offset.z),
															   JPH::Quat::sIdentity(), shape);
			auto offsetResult = offsetSettings.Create();
			if (!offsetResult.HasError())
			{
				shape = offsetResult.Get();
			}
		}

		return shape;
	}

	JPH::BodyCreationSettings JoltPhysicsWorld::BuildBodySettings(const PhysicsBodyDesc& desc, JPH::ShapeRefC shape)
	{
		JPH::EMotionType motionType;
		JPH::ObjectLayer objectLayer;

		if (desc.IsStatic)
		{
			motionType = JPH::EMotionType::Static;
			objectLayer = Layers::NON_MOVING;
		}
		else if (desc.IsKinematic)
		{
			motionType = JPH::EMotionType::Kinematic;
			objectLayer = Layers::MOVING;
		}
		else
		{
			motionType = JPH::EMotionType::Dynamic;
			objectLayer = Layers::MOVING;
		}

		JPH::BodyCreationSettings bodySettings(
			shape, JPH::RVec3(desc.Position.x, desc.Position.y, desc.Position.z),
			JPH::Quat(desc.Rotation.x, desc.Rotation.y, desc.Rotation.z, desc.Rotation.w), motionType, objectLayer);

		bodySettings.mFriction = desc.Friction;
		bodySettings.mRestitution = desc.Restitution;
		bodySettings.mLinearDamping = desc.LinearDamping;
		bodySettings.mAngularDamping = desc.AngularDamping;
		bodySettings.mAllowSleeping = true;

		if (desc.Shape == ColliderType::Mesh && !desc.IsStatic && !desc.Triangles.empty())
		{
			JPH::Vec3 bbMin(FLT_MAX, FLT_MAX, FLT_MAX), bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (const auto& tri : desc.Triangles)
			{
				const JPH::Vec3 verts[] = {
					JPH::Vec3(tri.V0.x, tri.V0.y, tri.V0.z),
					JPH::Vec3(tri.V1.x, tri.V1.y, tri.V1.z),
					JPH::Vec3(tri.V2.x, tri.V2.y, tri.V2.z),
				};
				for (const auto& v : verts)
				{
					bbMin = JPH::Vec3::sMin(bbMin, v);
					bbMax = JPH::Vec3::sMax(bbMax, v);
				}
			}
			JPH::Vec3 boxSize = bbMax - bbMin;
			if (boxSize.GetX() > 0.0f && boxSize.GetY() > 0.0f && boxSize.GetZ() > 0.0f)
			{
				float volume = boxSize.GetX() * boxSize.GetY() * boxSize.GetZ();
				float density = (volume > 0.0f) ? (desc.Mass / volume) : 1.0f;
				bodySettings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(boxSize, density);
				bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
			}
			else
			{
				bodySettings.mMassPropertiesOverride.mMass = desc.Mass;
				bodySettings.mMassPropertiesOverride.mInertia = JPH::Mat44::sScale(desc.Mass);
				bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
			}
		}
		else
		{
			bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
		}

		if (desc.IsKinematic)
		{
			bodySettings.mGravityFactor = 0.0f;
		}
		else
		{
			bodySettings.mGravityFactor = desc.UseGravity ? 1.0f : 0.0f;
		}

		if (desc.IsFixedRotation)
		{
			bodySettings.mAllowedDOFs =
				JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
		}

		bodySettings.mUserData = desc.UserData;

		return bodySettings;
	}

	PhysicsBodyHandle JoltPhysicsWorld::CreateBody(const PhysicsBodyDesc& desc)
	{
		auto shape = BuildShape(desc);
		if (!shape)
		{
			return kInvalidPhysicsBody;
		}

		auto settings = BuildBodySettings(desc, shape);

		JPH::BodyInterface& bi = m_PhysicsSystem.GetBodyInterface();
		JPH::Body* body = bi.CreateBody(settings);
		if (!body)
		{
			CH_CORE_ERROR("Physics: Body creation failed (user-data {}).", desc.UserData);
			return kInvalidPhysicsBody;
		}
		bi.AddBody(body->GetID(), JPH::EActivation::Activate);

		return (PhysicsBodyHandle)body->GetID().GetIndexAndSequenceNumber();
	}

	std::vector<PhysicsBodyHandle> JoltPhysicsWorld::CreateBodies(const std::vector<PhysicsBodyDesc>& descs)
	{
		if (descs.empty())
		{
			return {};
		}

		const auto buildStart = std::chrono::steady_clock::now();

		// Build all shapes
		std::vector<JPH::ShapeRefC> shapes(descs.size());
		for (size_t i = 0; i < descs.size(); ++i)
		{
			shapes[i] = BuildShape(descs[i]);
			if (!shapes[i])
			{
				shapes[i] = FallbackUnitBox("BuildShape returned null in batch");
			}
		}

		const auto shapesBuilt = std::chrono::steady_clock::now();
		const double shapesMs = std::chrono::duration<double, std::milli>(shapesBuilt - buildStart).count();
		if (shapesMs > 10.0)
		{
			CH_CORE_INFO("Physics: Built {} shapes in {:.1f} ms", descs.size(), shapesMs);
		}

		// Create bodies
		JPH::BodyInterface& bi = m_PhysicsSystem.GetBodyInterface();
		std::vector<JPH::BodyID> bodyIds;
		bodyIds.reserve(descs.size());

		for (size_t i = 0; i < descs.size(); ++i)
		{
			auto settings = BuildBodySettings(descs[i], shapes[i]);
			JPH::Body* body = bi.CreateBody(settings);
			if (!body)
			{
				CH_CORE_ERROR("Physics: Batch body creation failed (user-data {}).", descs[i].UserData);
				bodyIds.push_back(JPH::BodyID());
				continue;
			}
			bodyIds.push_back(body->GetID());
		}

		// Batch add to physics system
		std::vector<JPH::BodyID> validIds;
		validIds.reserve(bodyIds.size());
		for (auto& id : bodyIds)
		{
			if (!id.IsInvalid())
			{
				validIds.push_back(id);
			}
		}

		if (!validIds.empty())
		{
			auto addState = bi.AddBodiesPrepare(validIds.data(), (int)validIds.size());
			bi.AddBodiesFinalize(validIds.data(), (int)validIds.size(), addState, JPH::EActivation::Activate);
		}

		// Convert to handles
		std::vector<PhysicsBodyHandle> handles;
		handles.reserve(bodyIds.size());
		for (auto& id : bodyIds)
		{
			handles.push_back(id.IsInvalid() ? kInvalidPhysicsBody : (PhysicsBodyHandle)id.GetIndexAndSequenceNumber());
		}

		return handles;
	}

	void JoltPhysicsWorld::DestroyBody(PhysicsBodyHandle handle)
	{
		JPH::BodyInterface& bi = m_PhysicsSystem.GetBodyInterface();
		JPH::BodyID id((JPH::uint32)handle);
		bi.RemoveBody(id);
		bi.DestroyBody(id);
	}

	bool JoltPhysicsWorld::HasCachedMeshShape(const std::string& key) const
	{
		if (key.empty())
		{
			return false;
		}
		std::lock_guard lock(m_CacheMutex);
		return m_MeshShapeCache.count(key) > 0 || m_ConvexHullCache.count(key) > 0 ||
			   m_ConvexHullCache.count(key + "_convex") > 0;
	}

	bool JoltPhysicsWorld::IsShapeBaking(const std::string& key) const
	{
		if (key.empty())
		{
			return false;
		}
		std::lock_guard lock(m_CacheMutex);
		return m_InFlightMeshBakes.count(key) > 0;
	}

	bool JoltPhysicsWorld::HasPendingShapeBakes() const
	{
		std::lock_guard lock(m_CacheMutex);
		return !m_InFlightMeshBakes.empty();
	}

	void JoltPhysicsWorld::SetTransform(PhysicsBodyHandle handle, const glm::vec3& pos, const glm::quat& rot)
	{
		m_PhysicsSystem.GetBodyInterface().SetPositionAndRotation((JPH::BodyID)handle, JPH::RVec3(pos.x, pos.y, pos.z),
																  JPH::Quat(rot.x, rot.y, rot.z, rot.w),
																  JPH::EActivation::Activate);
	}

	void JoltPhysicsWorld::GetTransform(PhysicsBodyHandle handle, glm::vec3& pos, glm::quat& rot)
	{
		JPH::RVec3 jPos;
		JPH::Quat jRot;
		m_PhysicsSystem.GetBodyInterface().GetPositionAndRotation((JPH::BodyID)handle, jPos, jRot);
		pos = {jPos.GetX(), jPos.GetY(), jPos.GetZ()};
		rot = {jRot.GetW(), jRot.GetX(), jRot.GetY(), jRot.GetZ()};
	}

	void JoltPhysicsWorld::SetVelocity(PhysicsBodyHandle handle, const glm::vec3& velocity)
	{
		m_PhysicsSystem.GetBodyInterface().SetLinearVelocity((JPH::BodyID)handle,
															 JPH::Vec3(velocity.x, velocity.y, velocity.z));
		if (glm::length2(velocity) > 0.0001f)
		{
			m_PhysicsSystem.GetBodyInterface().ActivateBody((JPH::BodyID)handle);
		}
	}

	glm::vec3 JoltPhysicsWorld::GetVelocity(PhysicsBodyHandle handle) const
	{
		JPH::Vec3 v = m_PhysicsSystem.GetBodyInterface().GetLinearVelocity((JPH::BodyID)handle);
		return {v.GetX(), v.GetY(), v.GetZ()};
	}

	RaycastResult JoltPhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
	{
		JPH::RRayCast ray{JPH::RVec3(origin.x, origin.y, origin.z),
						  JPH::Vec3(direction.x, direction.y, direction.z) * maxDistance};

		JPH::RayCastResult result;
		if (m_PhysicsSystem.GetNarrowPhaseQuery().CastRay(ray, result))
		{
			JPH::RVec3 hitPos = ray.GetPointOnRay(result.mFraction);
			RaycastResult out;
			out.Hit = true;
			out.Distance = result.mFraction * maxDistance;
			out.Position = {hitPos.GetX(), hitPos.GetY(), hitPos.GetZ()};
			out.Entity = (entt::entity)m_PhysicsSystem.GetBodyInterface().GetUserData(result.mBodyID);
			return out;
		}
		return RaycastResult{false};
	}

	void JoltPhysicsWorld::Step(float fixedDt)
	{
		m_PhysicsSystem.Update(fixedDt, 1, m_TempAllocator.get(), m_JobSystem.get());
	}

	void JoltPhysicsWorld::SetGravity(float gravity)
	{
		m_PhysicsSystem.SetGravity(JPH::Vec3(0.0f, -gravity, 0.0f));
	}

	// ─────────────────────────────────────────────────────────────────────────────
	// Ground detection via contact callbacks (O(1) lookup)
	// ─────────────────────────────────────────────────────────────────────────────
	bool JoltPhysicsWorld::IsBodyGrounded(PhysicsBodyHandle handle) const
	{
		if (handle == 0)
		{
			return false;
		}
		std::lock_guard lock(m_GroundedMutex);
		return m_GroundedBodies.count(static_cast<uint32_t>(handle)) > 0;
	}

	bool JoltPhysicsWorld::IsBodyActive(PhysicsBodyHandle handle) const
	{
		if (handle == 0)
		{
			return false;
		}
		return m_PhysicsSystem.GetBodyInterface().IsActive(JPH::BodyID(static_cast<uint32_t>(handle)));
	}

	void JoltPhysicsWorld::ClearGroundedState()
	{
		std::lock_guard lock(m_GroundedMutex);
		std::unordered_set<uint32_t> nextGrounded;
		auto& bi = m_PhysicsSystem.GetBodyInterface();
		for (uint32_t handle : m_GroundedBodies)
		{
			if (!bi.IsActive(JPH::BodyID(handle)))
			{
				nextGrounded.insert(handle);
			}
		}
		m_GroundedBodies = std::move(nextGrounded);
	}

	// ─────────────────────────────────────────────────────────────────────────────
	// ContactListenerImpl
	// ─────────────────────────────────────────────────────────────────────────────
	JPH::ValidateResult JoltPhysicsWorld::ContactListenerImpl::OnContactValidate(const JPH::Body&, const JPH::Body&,
																				 JPH::RVec3Arg,
																				 const JPH::CollideShapeResult&)
	{
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	static void RegisterGroundContact(std::mutex& mutex, std::unordered_set<uint32_t>& tracker, const JPH::Body& body1,
									  const JPH::Body& body2, const JPH::ContactManifold& manifold)
	{
		JPH::Vec3 normal = manifold.mWorldSpaceNormal;
		uint32_t id1 = body1.GetID().GetIndexAndSequenceNumber();
		uint32_t id2 = body2.GetID().GetIndexAndSequenceNumber();

		std::lock_guard lock(mutex);
		// In Jolt, mWorldSpaceNormal points from body1 to body2.
		// If normal Y > 0.5, body2 is ABOVE body1. Thus body2 is grounded on body1.
		if (normal.GetY() > 0.5f)
		{
			tracker.insert(id2);
		}
		// If normal Y < -0.5, body1 is ABOVE body2. Thus body1 is grounded on body2.
		else if (normal.GetY() < -0.5f)
		{
			tracker.insert(id1);
		}
	}

	void JoltPhysicsWorld::ContactListenerImpl::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
															   const JPH::ContactManifold& manifold,
															   JPH::ContactSettings&)
	{
		RegisterGroundContact(*m_Mutex, *m_Tracker, body1, body2, manifold);
	}

	void JoltPhysicsWorld::ContactListenerImpl::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
																   const JPH::ContactManifold& manifold,
																   JPH::ContactSettings&)
	{
		RegisterGroundContact(*m_Mutex, *m_Tracker, body1, body2, manifold);
	}

	void JoltPhysicsWorld::ContactListenerImpl::OnContactRemoved(const JPH::SubShapeIDPair&)
	{
		// Grounded set is rebuilt each frame by ClearGroundedState() + step contacts.
	}

} // namespace Chained