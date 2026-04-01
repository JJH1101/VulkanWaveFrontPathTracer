#pragma once
#include "VulkanTools.h"
#include "vulkan/vulkan.h"
#include <vector>

#define MEASURE_MODE 1
/**
 * @example
 * gpuTimer.reset()
 * gpuTimer.record()
 * "Record On CommandBuffer Things"
 * gpuTimer.record()
 * float deltaTime = gpuTimer.timerResult()
 */
class GPUTimer
{
private:
	VkDevice device = VK_NULL_HANDLE;
	VkQueryPool timeStampQueryPool = VK_NULL_HANDLE;
	uint32_t curQueryIndex = 0;
	uint32_t queryCount; // before after
	VkQueryResultFlagBits queryFlag = static_cast<VkQueryResultFlagBits>(VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT |VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
	float timestampPeriodDeviceLimit = 0.f;
	std::vector<float> timerResults;

public:
	~GPUTimer()
	{
		if (timeStampQueryPool != VK_NULL_HANDLE)
			vkDestroyQueryPool(device, timeStampQueryPool, nullptr);
	}
	GPUTimer() {}
	void init(vks::VulkanDevice& _device, uint32_t queryCount = 2)
	{
		this->device = _device.logicalDevice;
		this->timestampPeriodDeviceLimit = _device.properties.limits.timestampPeriod;
		this->queryCount = queryCount;
		this->timerResults.resize(queryCount / 2);

		VkQueryPoolCreateInfo queryPoolInfo{};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolInfo.queryCount = queryCount;
		VK_CHECK_RESULT(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &timeStampQueryPool));
	}
	void reset(VkCommandBuffer cmdBuffer)
	{
#if MEASURE_MODE
		curQueryIndex = 0;
		vkCmdResetQueryPool(cmdBuffer, timeStampQueryPool, 0, queryCount);
#endif
	}

	/**
	 * @note If NOT MEASURE_MODE, Do Nothing.
	 */
	void record(VkCommandBuffer cmdBuffer, VkPipelineStageFlagBits pipelineStageFlag)
	{
#if MEASURE_MODE
		vkCmdWriteTimestamp(cmdBuffer, pipelineStageFlag, timeStampQueryPool, curQueryIndex++);
#endif
	}

	/**
	 * @return -FLT_MAX if timer not ready
	 */
	const std::vector<float>& timerResult()
	{
#if MEASURE_MODE
		curQueryIndex = 0;
		std::vector<uint64_t> timeStampResults(queryCount * 2, 0);
		vkGetQueryPoolResults(device, timeStampQueryPool, 0, queryCount, sizeof(uint64_t) * queryCount * 2,
			timeStampResults.data(), sizeof(uint64_t) * 2, queryFlag);

		for (uint32_t i = 0; i < queryCount / 2; ++i) // (start, end, start, end, ,,,)
		{
			if (timeStampResults[i * 4 + 1] && timeStampResults[i * 4 + 3])
				timerResults[i] = float(timeStampResults[i * 4 + 2] - timeStampResults[i * 4 + 0]) * timestampPeriodDeviceLimit / (1'000'000.0f);
			else
				timerResults[i] = -FLT_MAX;
		}
#endif
		return timerResults;
	}
};
