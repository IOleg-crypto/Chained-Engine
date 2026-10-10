#ifndef CH_SCRIPT_GLUE_REGISTRY_H
#define CH_SCRIPT_GLUE_REGISTRY_H

#include "engine/core/service_locator.h"
#include "engine/scene/entity.h"
#include <Coral/Assembly.hpp>
#include <vector>
#include <type_traits>
#include <string>
#include <utility>
#include <glm/glm.hpp>

namespace Chained
{
	// -------------------------------------------------------------------------
	// 1. ServiceBinder
	// -------------------------------------------------------------------------
	template <auto MemPtr> struct ServiceBinder;

	template <typename T, typename Ret, typename... Args, Ret (T::*MemPtr)(Args...)> struct ServiceBinder<MemPtr>
	{
		using AbiRet = std::conditional_t<std::is_same_v<Ret, bool>, int32_t, Ret>;
		static AbiRet Invoke(Args... args)
		{
			return static_cast<AbiRet>((ServiceLocator::Get<T>()->*MemPtr)(std::forward<Args>(args)...));
		}
	};

	template <typename T, typename Ret, typename... Args, Ret (T::*MemPtr)(Args...) const> struct ServiceBinder<MemPtr>
	{
		using AbiRet = std::conditional_t<std::is_same_v<Ret, bool>, int32_t, Ret>;
		static AbiRet Invoke(Args... args)
		{
			return static_cast<AbiRet>((ServiceLocator::Get<T>()->*MemPtr)(std::forward<Args>(args)...));
		}
	};

#define CH_BIND_SERVICE_METHOD(Assembly, CsClass, CsMethod, CppMemPtr)                                                 \
	(Assembly).AddInternalCall(CsClass, CsMethod, (void*)&::Chained::ServiceBinder<CppMemPtr>::Invoke)

	// -------------------------------------------------------------------------
	// 2. ComponentBinder
	// -------------------------------------------------------------------------
	Entity GetEntity(uint64_t entityID);

	namespace Detail
	{
		template <typename T> struct IsGlmVector : std::false_type
		{
		};

		template <int L, typename T, glm::qualifier Q> struct IsGlmVector<glm::vec<L, T, Q>> : std::true_type
		{
		};

		template <typename T> constexpr bool IsGlmVector_v = IsGlmVector<T>::value;

		template <auto MemPtr> struct ComponentBinder;

		template <typename Comp, typename FieldType, FieldType Comp::* Field> struct ComponentBinder<Field>
		{
			// GET: Primitive
			static FieldType GetPrimitive(uint64_t entityID)
			{
				auto e = GetEntity(entityID);
				return (e && e.HasComponent<Comp>()) ? e.GetComponent<Comp>().*Field : FieldType{};
			}

			// GET: Bool -> uint8_t
			static uint8_t GetBool(uint64_t entityID)
			{
				auto e = GetEntity(entityID);
				return (e && e.HasComponent<Comp>()) ? (e.GetComponent<Comp>().*Field ? 1 : 0) : 0;
			}

			// GET: Vector/Struct -> via out ptr
			static void GetVector(uint64_t entityID, FieldType* outVal)
			{
				if (!outVal)
				{
					return;
				}
				auto e = GetEntity(entityID);
				*outVal = (e && e.HasComponent<Comp>()) ? e.GetComponent<Comp>().*Field : FieldType{};
			}

			// SET: Primitive
			static void SetPrimitive(uint64_t entityID, FieldType value)
			{
				auto e = GetEntity(entityID);
				if (e && e.HasComponent<Comp>())
				{
					e.GetComponent<Comp>().*Field = value;
				}
			}

			// SET: Bool <- uint8_t
			static void SetBool(uint64_t entityID, uint8_t value)
			{
				auto e = GetEntity(entityID);
				if (e && e.HasComponent<Comp>())
				{
					e.GetComponent<Comp>().*Field = (value != 0);
				}
			}

			// SET: Vector/Struct <- via in ptr
			static void SetVector(uint64_t entityID, FieldType* inVal)
			{
				if (!inVal)
				{
					return;
				}
				auto e = GetEntity(entityID);
				if (e && e.HasComponent<Comp>())
				{
					e.GetComponent<Comp>().*Field = *inVal;
				}
			}

			static void* GetAddress()
			{
				if constexpr (std::is_same_v<FieldType, bool>)
				{
					return (void*)&GetBool;
				}
				else if constexpr (IsGlmVector_v<FieldType>)
				{
					return (void*)&GetVector;
				}
				else
				{
					return (void*)&GetPrimitive;
				}
			}

			static void* SetAddress()
			{
				if constexpr (std::is_same_v<FieldType, bool>)
				{
					return (void*)&SetBool;
				}
				else if constexpr (IsGlmVector_v<FieldType>)
				{
					return (void*)&SetVector;
				}
				else
				{
					return (void*)&SetPrimitive;
				}
			}
		};
	} // namespace Detail

#define CH_BIND_COMPONENT_GETTER(Assembly, CsClass, CsMethod, CompType, Field)                                         \
	(Assembly).AddInternalCall(CsClass, CsMethod, ::Chained::Detail::ComponentBinder<&CompType::Field>::GetAddress())

#define CH_BIND_COMPONENT_SETTER(Assembly, CsClass, CsMethod, CompType, Field)                                         \
	(Assembly).AddInternalCall(CsClass, CsMethod, ::Chained::Detail::ComponentBinder<&CompType::Field>::SetAddress())

} // namespace Chained

#endif // CH_SCRIPT_GLUE_REGISTRY_H
