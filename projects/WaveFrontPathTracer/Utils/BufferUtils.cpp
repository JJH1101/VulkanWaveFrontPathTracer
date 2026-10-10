#include "BufferUtils.h"

namespace vks::util {

	void memoryBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) {
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = srcAccess;
		barrier.dstAccessMask = dstAccess;
		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	}

	void copyBufferToHost(vks::VulkanDevice& device, VkQueue queue, vks::Buffer& src, vks::Buffer& dst) {
		assert(dst.size >= src.size);
		VkCommandBuffer commandBuffer = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		memoryBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		VkBufferCopy copyRegion{ 0, 0, src.size };
		vkCmdCopyBuffer(commandBuffer, src.buffer, dst.buffer, 1, &copyRegion);
		memoryBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
		device.flushCommandBuffer(commandBuffer, queue);
	}

	void resizeBuffer(vks::VulkanDevice& device, VkQueue queue, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size) {
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
		memoryBarrier(copyCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		vkCmdCopyBuffer(copyCmd, oldBuffer.buffer, buffer->buffer, 1, &copyRegion);
		memoryBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);

		device.flushCommandBuffer(copyCmd, queue, true);

		oldBuffer.destroy();
	}
	
	void resizeDiscardBuffer(vks::VulkanDevice& device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size) {
		if (buffer->size != size) {
			buffer->destroy();

			device.createBuffer(usageFlags, memoryPropertyFlags, buffer, size);
		}
	}

	void clearBuffer(vks::VulkanDevice& device, VkQueue queue, vks::Buffer* buffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		memoryBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

		vkCmdFillBuffer(commandBuffer, buffer->buffer, offset, size, value);

		device.flushCommandBuffer(commandBuffer, queue);
	}

	float clearBufferTimed(GPUTimer& timer, vks::VulkanDevice& device, VkQueue queue, vks::Buffer* buffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		timer.reset(commandBuffer);

		timer.record(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT);
		memoryBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

		vkCmdFillBuffer(commandBuffer, buffer->buffer, offset, size, value);

		timer.record(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT);

		device.flushCommandBuffer(commandBuffer, queue);

		auto timerResults = timer.timerResult();

		return timerResults[0];
	}

	uint64_t getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
		VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
		bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAddressInfo.buffer = buffer;

		return vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);
	}

}
