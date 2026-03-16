#pragma once

#include "VulkanDevice.h"
#include "GPUTimer.h"

namespace vks::util {
	
	void inline resizeBuffer(vks::VulkanDevice& device, VkQueue queue, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size) {
		if (buffer->size == size) return;

		usageFlags |= (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		if (buffer->buffer == VK_NULL_HANDLE || size == 0) {
			buffer->destroy();
			if (size > 0) {
				device.createBuffer(usageFlags, memoryPropertyFlags, buffer, size);
			}
			return;
		}

		vks::Buffer oldBuffer = *buffer;

		device.createBuffer(usageFlags, memoryPropertyFlags, buffer, size);

		VkCommandBuffer copyCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = std::min(oldBuffer.size, size);

		vkCmdCopyBuffer(copyCmd, oldBuffer.buffer, buffer->buffer, 1, &copyRegion);

		device.flushCommandBuffer(copyCmd, queue, true);

		oldBuffer.destroy();
	}
	
	void inline resizeDiscardBuffer(vks::VulkanDevice& device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size) {
		if (buffer->size != size) {
			buffer->destroy();

			device.createBuffer(usageFlags, memoryPropertyFlags, buffer, size);
		}
	}

	void inline clearBuffer(vks::VulkanDevice& device, VkQueue queue, vks::Buffer* buffer, uint32_t value = 0, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) {
		VkCommandBuffer commandBuffer = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		vkCmdFillBuffer(commandBuffer, buffer->buffer, offset, size, value);

		device.flushCommandBuffer(commandBuffer, queue);
	}

	float inline clearBufferTimed(GPUTimer& timer, vks::VulkanDevice& device, VkQueue queue, vks::Buffer* buffer, uint32_t value = 0, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) {
		VkCommandBuffer commandBuffer = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		timer.reset(commandBuffer);

		timer.record(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdFillBuffer(commandBuffer, buffer->buffer, offset, size, value);

		timer.record(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT);

		device.flushCommandBuffer(commandBuffer, queue);

		auto timerResults = timer.timerResult();

		return timerResults[0];
	}

	uint64_t inline getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
		VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
		bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAddressInfo.buffer = buffer;

		return vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);
	}

}