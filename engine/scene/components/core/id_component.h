#ifndef CH_ID_COMPONENT_H
#define CH_ID_COMPONENT_H

#include "engine/common/uuid.h"
#include "engine/reflection/reflection_rfl.h"

namespace Chained
{
	struct IDComponent
	{
		UUID ID;

		static const char* GetStaticName()
		{
			return "IDComponent";
		}

		struct UI
		{
			UIMeta ID = {.ReadOnly = true, .Transient = true, .Tooltip = "Unique entity identifier (UUID)"};
		};
	};
	CH_MARK_RFL(IDComponent);
} // namespace Chained

// IDComponent uses manual registration in RegisterEngineComponents()
// due to circular include dependency (entity.h -> id_component.h -> component_registry.h)

#endif // CH_ID_COMPONENT_H
