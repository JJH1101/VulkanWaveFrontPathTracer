/*
 * Taekgeun You, 2025
 * 
 * Modified by Junhyeok Jang, 2025
 */
#pragma once

#include "VulkanDevice.h"
#include "../Utils/gpuTimer.h"

class ComputePass
{
private:
	vks::VulkanDevice* device = nullptr;
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

public:

	struct ShaderEntry {
		std::string filePath = "";
		std::string entryPoint = "main";
		const VkSpecializationInfo* specializationInfo = nullptr;
		VkShaderStageFlagBits stage = VK_SHADER_STAGE_COMPUTE_BIT;
	};

	struct PipelineContext {
		ShaderEntry shaderEntry;
		std::vector<VkPushConstantRange> pushConstantRanges;
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	};

	struct DispatchDesc {
		uint32_t groupCountX = 1;
		uint32_t groupCountY = 1;
		uint32_t groupCountZ = 1;
	};

	struct PushConstantDesc {
		uint32_t offset;
		uint32_t size;
		void* data;
	};

	void createPipeline(vks::VulkanDevice& device, const PipelineContext pipelineContext);
	void record(VkCommandBuffer commandBuffer, DispatchDesc dispatchDesc, const std::vector<VkDescriptorSet>& descriptorSets = {}, const std::vector<uint32_t>& dynamicOffsets = {}, const std::vector<PushConstantDesc>& pushConstantDescs = {});
	void launch(VkQueue queue, DispatchDesc dispatchDesc, const std::vector<VkDescriptorSet>& descriptorSets = {}, const std::vector<uint32_t>& dynamicOffsets = {}, const std::vector<PushConstantDesc>& pushConstantDescs = {});
	float launchTimed(GPUTimer& timer, VkQueue queue, DispatchDesc dispatchDesc, const std::vector<VkDescriptorSet>& descriptorSets = {}, const std::vector<uint32_t>& dynamicOffsets = {}, const std::vector<PushConstantDesc>& pushConstantDescs = {});

	ComputePass(void) = default;
	~ComputePass(void) {
		if (device) {
			vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
			vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
		}
	}
};