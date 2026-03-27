#include "Tracer.h"
#include "../Utils/BufferUtils.h"

PFN_vkCmdPushDescriptorSetKHR g_vkCmdPushDescriptorSetKHR = nullptr;
PFN_vkCmdWriteTimestamp g_vkCmdWriteTimestamp = nullptr;
PFN_vkCmdUpdateBuffer g_vkCmdUpdateBuffer = nullptr;
#define vkCmdPushDescriptorSetKHR g_vkCmdPushDescriptorSetKHR
#define vkCmdWriteTimestamp g_vkCmdWriteTimestamp
#define vkCmdUpdateBuffer g_vkCmdUpdateBuffer

#define VRDX_IMPLEMENTATION
#include "vulkan_radix_sort/vk_radix_sort.h"

Tracer::Tracer() {
    
}

Tracer::~Tracer() {
    vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayoutTrace, nullptr);
    vkDestroyDescriptorPool(device->logicalDevice, descriptorPoolTrace, nullptr);
}

float Tracer::computeMortonCodes(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos) {
    
    PushConstantsMortonCode pc{};
    pc.rayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getRayBuffer().buffer);
    pc.mortonCodeAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getMortonCodeBuffer().buffer);
    pc.rayIndexAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getIndexBuffer().buffer);
    pc.numberOfRays = rays.getSize();
    pc.sceneMin = sceneMinPos;
    pc.sceneSize = sceneMaxPos - sceneMinPos;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsMortonCode), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (rays.getSize() + workGroupSize - 1) / workGroupSize, 1, 1 };
    return computeMortonCodesPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float Tracer::radixSort(vks::Buffer& keyBuffer, vks::Buffer& valueBuffer, vks::Buffer& storageBuffer, int size) {
    VrdxSorterStorageRequirements requirements;
    vrdxGetSorterKeyValueStorageRequirements(sorter, size, &requirements);

    if (requirements.size > storageBuffer.size) {
        vks::util::resizeDiscardBuffer(
            *device,
            requirements.usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &storageBuffer,
            requirements.size
        );
    }

    VkCommandBuffer commandBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    timer->reset(commandBuffer);

    timer->record(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    vrdxCmdSortKeyValue(commandBuffer, sorter, size,
        keyBuffer.buffer, 0,
        valueBuffer.buffer, 0,
        storageBuffer.buffer, 0,
        nullptr, 0);

    //vkCmdPipelineBarrier(
    //	commandBuffer,
    //	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // Src
    //    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // Dst
    //	0, 0, nullptr, 0, nullptr, 0, nullptr
    //); // This barrier is needed or not?
    
    timer->record(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    device->flushCommandBuffer(commandBuffer, queue, true);

    auto timerResults = timer->timerResult();

    return timerResults[0];
}

float Tracer::mortonSort(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos, float& mortoncodesTime, float& sortTime) {
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &rays.getMortonCodeBuffer(),
        rays.getCapacity() * sizeof(uint32_t)
    );

    mortoncodesTime = computeMortonCodes(rays, sceneMinPos, sceneMaxPos);
    
    sortTime = radixSort(rays.getMortonCodeBuffer(), rays.getIndexBuffer(), rays.getSpineBuffer(), rays.getSize());

    return mortoncodesTime + sortTime;

}

void Tracer::init(vks::VulkanDevice& _device, GPUTimer& _timer, VkQueue _queue) {
    this->device = &_device;
    this->timer = &_timer;
    this->queue = _queue;

    vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(vkGetDeviceProcAddr(device->logicalDevice, "vkCmdWriteTimestamp"));
    vkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetDeviceProcAddr(device->logicalDevice, "vkCmdPushDescriptorSetKHR"));
    vkCmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(vkGetDeviceProcAddr(device->logicalDevice, "vkCmdUpdateBuffer"));

    // Create trace descriptor set
    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = &poolSize;
    descriptorPoolInfo.maxSets = 1;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPoolTrace));

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutInfo.bindingCount = 1;
    descriptorSetLayoutInfo.pBindings = &binding;
    vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutTrace);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPoolTrace;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayoutTrace;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &descriptorSetAllocateInfo, &descriptorSetTrace));

    // Create trace pipeline
    VkShaderModule shaderModule = vks::tools::loadShader((std::string(shaderPath) + "trace.comp.spv").c_str(), device->logicalDevice);
    VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsTrace) };
    ComputePass::PipelineContext pipelineContext;
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.descriptorSetLayouts = { descriptorSetLayoutTrace };
    pipelineContext.pushConstantRanges = { pushConstantRange };
    tracePass.createPipeline(*device, pipelineContext);

    // Create computeMortonCodes pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "computeMortonCodesAila32.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsMortonCode) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.descriptorSetLayouts = {};
    pipelineContext.pushConstantRanges = { pushConstantRange };
    computeMortonCodesPass.createPipeline(*device, pipelineContext);

    // Create radix sorter
    VrdxSorterCreateInfo sorterInfo = {};
    sorterInfo.physicalDevice = device->physicalDevice;
    sorterInfo.device = device->logicalDevice;
    vrdxCreateSorter(&sorterInfo, &sorter);
}

void Tracer::setAccererationStructure(VkAccelerationStructureKHR topLevelAS) {
    // Write trace descriptor set
    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS;

    VkWriteDescriptorSet accelerationStructureWrite{};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // The specialized acceleration structure descriptor has to be chained
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = descriptorSetTrace;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    vkUpdateDescriptorSets(device->logicalDevice, 1, &accelerationStructureWrite, 0, nullptr);
}

float Tracer::trace(RayBuffer& rays, bool sort) {
    int numRays = rays.getSize();

    // Set push constants
    PushConstantsTrace pc{};
    pc.rayBufferAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getRayBuffer().buffer);
    pc.resultBufferAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getResultBuffer().buffer);
    pc.numRays = numRays;
    pc.rayIndexBufferAddr = sort ? vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getIndexBuffer().buffer) : 0;
    pc.isClosestHit = rays.getClosestHit();

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsTrace), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };
    std::vector<VkDescriptorSet> descriptorSets = { descriptorSetTrace };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    return tracePass.launchTimed(*timer, queue, dispatchDesc, descriptorSets, {}, pushConstantDescs);
}

float Tracer::trace(RayBuffer& rays) {
    return trace(rays, false);
}

float Tracer::traceSort(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos) {
    float mortoncodesTime, sortTime, traceTime;
    return traceSort(rays, sceneMinPos, sceneMaxPos, mortoncodesTime, sortTime, traceTime);
}

float Tracer::traceSort(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos, float& mortoncodesTime, float& sortTime, float& traceTime) {
    
    mortonSort(rays, sceneMinPos, sceneMaxPos, mortoncodesTime, sortTime);
    traceTime = trace(rays, true);
    
    return mortoncodesTime + sortTime + traceTime;
}