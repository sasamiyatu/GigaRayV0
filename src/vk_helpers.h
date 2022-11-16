#pragma once
#include "Volk/volk.h"

namespace vkinit
{
	inline VkAccelerationStructureInstanceKHR acceleration_structure_instance(uint64_t acceleration_structure_ref)
	{
		VkAccelerationStructureInstanceKHR instance{};
		instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.f;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = acceleration_structure_ref;

		return instance;
	}
}