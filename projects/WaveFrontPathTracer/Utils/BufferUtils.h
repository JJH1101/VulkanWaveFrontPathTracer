#pragma once

#include "VulkanDevice.h"
#include "GPUTimer.h"

namespace vks::util {
	
	void resizeBuffer(vks::VulkanDevice& device, VkQueue queue, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size);
	
	void resizeDiscardBuffer(vks::VulkanDevice& device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size);

	void clearBuffer(vks::VulkanDevice& device, VkQueue queue, vks::Buffer* buffer, uint32_t value = 0, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

	float clearBufferTimed(GPUTimer& timer, vks::VulkanDevice& device, VkQueue queue, vks::Buffer* buffer, uint32_t value = 0, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

	uint64_t getBufferDeviceAddress(VkDevice device, VkBuffer buffer);

}