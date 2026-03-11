/*
 * Taekgeun You, 2025
 * 
 * Modified by Junhyeok Jang, 2025
 */

#include "ComputePass.h"

void ComputePass::createPipeline(vks::VulkanDevice& _device, const PipelineContext pipelineContext)
{
	this->device = &_device;

	// create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(pipelineContext.descriptorSetLayouts.data(), static_cast<uint32_t>(pipelineContext.descriptorSetLayouts.size()));

	if (pipelineContext.pushConstantRanges.size() > 0)
	{
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pipelineContext.pushConstantRanges.size());
		pipelineLayoutCreateInfo.pPushConstantRanges = pipelineContext.pushConstantRanges.data();
	}

	VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkPipelineShaderStageCreateInfo shaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStage.module = pipelineContext.shaderEntry.module;
	shaderStage.pName = pipelineContext.shaderEntry.entryPoint.c_str();
	shaderStage.pSpecializationInfo = pipelineContext.shaderEntry.specializationInfo;

	VkComputePipelineCreateInfo computePipelineCI= vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);
	computePipelineCI.stage = shaderStage;
	VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));
}

void ComputePass::record(VkCommandBuffer commandBuffer,
	DispatchDesc dispatchDesc,
	const std::vector<VkDescriptorSet>& descriptorSets,
	const std::vector<uint32_t>& dynamicOffsets,
	const std::vector<PushConstantDesc>& pushConstantDescs)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	if (descriptorSets.size() > 0)
	{
		if (dynamicOffsets.size() > 0)
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_COMPUTE,
				pipelineLayout,
				0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
				static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
		else
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_COMPUTE,
				pipelineLayout,
				0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
				0, nullptr);
	}
	if (pushConstantDescs.size() > 0)
	{
		for (auto& pushConstantDesc : pushConstantDescs)
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, pushConstantDesc.offset, pushConstantDesc.size, pushConstantDesc.data);
	}

	vkCmdDispatch(commandBuffer, dispatchDesc.groupCountX, dispatchDesc.groupCountY, dispatchDesc.groupCountZ);
}

void ComputePass::launch(VkQueue queue,
	DispatchDesc dispatchDesc, 
	const std::vector<VkDescriptorSet>& descriptorSets, 
	const std::vector<uint32_t>& dynamicOffsets, 
	const std::vector<PushConstantDesc>& pushConstantDescs)
{
	VkCommandBuffer commandBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	record(commandBuffer, dispatchDesc, descriptorSets, dynamicOffsets, pushConstantDescs);

	device->flushCommandBuffer(commandBuffer, queue, true);
}

float ComputePass::launchTimed(GPUTimer& timer,
	VkQueue queue,
	DispatchDesc dispatchDesc,
	const std::vector<VkDescriptorSet>& descriptorSets,
	const std::vector<uint32_t>& dynamicOffsets,
	const std::vector<PushConstantDesc>& pushConstantDescs)
{
	float time = 0.f;

	VkCommandBuffer commandBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	timer.reset(commandBuffer);

	timer.record(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	record(commandBuffer, dispatchDesc, descriptorSets, dynamicOffsets, pushConstantDescs);
	//vkCmdPipelineBarrier(
	//	commandBuffer,
	//	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // Src
	//	VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // Dst
	//	0, 0, nullptr, 0, nullptr, 0, nullptr
	//);
	// This barrier is needed or not?
	timer.record(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	device->flushCommandBuffer(commandBuffer, queue, true);

	auto timerResults = timer.timerResult();

	return timerResults[0];
}