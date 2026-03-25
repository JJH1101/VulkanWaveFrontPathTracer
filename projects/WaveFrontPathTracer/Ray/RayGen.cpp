/**
  * Original Source
  * \file	RayGen.cpp
  * \author	Daniel Meister
  * \date	2014/05/10
  * \brief	RayGen class source file.
  */

/**
  * Modified Source
  * \file	RayGen.cpp
  * \author	Junhyeok Jang
  * \date	2026/03/10
  * \brief	Modified for Vulkan
  */

#include "RayGen.h"
#include "../Utils/BufferUtils.h"
#include "../Environment/Environment.h"

float RayGen::initSeeds(int numberOfPixels, int frameIndex) {

    // Resize seeds.
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &seeds,
        numberOfPixels * sizeof(uint32_t)
    );

    // Set push constants
    PushConstantsInitSeeds pc{};
    pc.seedAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, seeds.buffer);
    pc.numberOfPixels = numberOfPixels;
    pc.frameIndex = frameIndex;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsInitSeeds), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numberOfPixels + workGroupSize - 1) / workGroupSize, 1, 1 };
    return initSeedsPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

void RayGen::init(vks::VulkanDevice& _device, GPUTimer& _timer, VkQueue _queue) {

    this->device = &_device;
    this->timer = &_timer;
    this->queue = _queue;

    // Create initSeeds pipeline
    VkShaderModule shaderModule = vks::tools::loadShader((std::string(shaderPath) + "initSeeds.comp.spv").c_str(), device->logicalDevice);
    VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsInitSeeds) };
    ComputePass::PipelineContext pipelineContext;
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    initSeedsPass.createPipeline(*device, pipelineContext);

    // Create primary pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "raygenPrimary.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsPrimary) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    primaryPass.createPipeline(*device, pipelineContext);

    // Create shadow pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "raygenShadow.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsShadow) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    shadowPass.createPipeline(*device, pipelineContext);

    // Create path pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "raygenPath.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsPath) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    pathPass.createPipeline(*device, pipelineContext);

    Environment::getInstance()->getBoolValue("Renderer.russianRoulette", russianRoulette);
}

float RayGen::primary(RayBuffer & orays, Camera & camera, glm::ivec2 & extent, int sampleIndex) {

    // Closest hit.
    pixelTable.setSize(extent, *device, queue);
    orays.resize(*device, queue, extent.x * extent.y);
    orays.setClosestHit(true);
    orays.getSlotToIndexBuffer() = pixelTable.getIndexToPixel();
    orays.getIndexToSlotBuffer() = pixelTable.getPixelToIndex();

    // Set push constants.
    PushConstantsPrimary pc{};
    pc.indexToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getSlotToIndexBuffer().buffer);
    pc.rayBufferAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getRayBuffer().buffer);
    pc.screenToWorld = glm::inverse(camera.matrices.perspective * camera.matrices.view);
    pc.origin = camera.position;
    pc.sampleIndex = sampleIndex;
    pc.size = extent;
    pc.maxDist = camera.getFarClip();

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsPrimary), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { ((extent.x * extent.y) + workGroupSize - 1) / workGroupSize, 1, 1 };
    return primaryPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float RayGen::shadow(RayBuffer & orays, RayBuffer & irays, int batchBegin, int batchEnd, int numberOfSamples, const glm::vec3 & light, float lightRadius) {

    // First hit.
    orays.resize(*device, queue, (batchEnd - batchBegin) * numberOfSamples);
    orays.setClosestHit(false);

    // Set push constants
    PushConstantsShadow pc{};
    pc.seedAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, seeds.buffer);
    pc.inputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getRayBuffer().buffer);
    pc.outputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getRayBuffer().buffer);
    pc.inputResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getResultBuffer().buffer);
    pc.outputSlotToIndexAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getSlotToIndexBuffer().buffer);
    pc.outputIndexToSlotAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getIndexToSlotBuffer().buffer);
    pc.light = light;
    pc.lightRadius = lightRadius;
    pc.batchBegin = batchBegin;
    pc.batchSize = batchEnd - batchBegin;
    pc.numberOfSamples = numberOfSamples;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsShadow), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { ((batchEnd - batchBegin) + workGroupSize - 1) / workGroupSize, 1, 1 };
    return shadowPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float RayGen::path(RayBuffer& orays, RayBuffer& irays, vks::Buffer& decreases, vks::Buffer& geometries) {

    // Closest hit.
    orays.resize(*device, queue, irays.getSize());
    orays.setClosestHit(true);

    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &counterDevice,
        sizeof(uint32_t)
    );
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &counterHost,
        sizeof(uint32_t)
    );

    vks::util::clearBuffer(*device, queue, &counterDevice);

    PushConstantsPath pc{};
    pc.seedAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, seeds.buffer);
    pc.geometryAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, geometries.buffer);
    pc.inputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getRayBuffer().buffer);
    pc.outputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getRayBuffer().buffer);
    pc.inputIndexToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getSlotToIndexBuffer().buffer);
    pc.outputIndexToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getSlotToIndexBuffer().buffer);
    pc.inputResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getResultBuffer().buffer);
    pc.decreaseAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, decreases.buffer);
    pc.numberOfOutputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, counterDevice.buffer);
    pc.numberOfInputRays = irays.getSize();
    pc.russianRoulette = russianRoulette;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsPath), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (irays.getSize() + workGroupSize - 1) / workGroupSize, 1, 1 };
    float time = pathPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);

    device->copyBuffer(&counterDevice, &counterHost, queue);
    counterHost.map();
    uint32_t numberOfRays = *static_cast<uint32_t*>(counterHost.mapped);
    counterHost.unmap();

    orays.resize(*device, queue, numberOfRays);

    return time;
}

bool RayGen::getRussianRoulette() {
    return russianRoulette;
}

void RayGen::setRussianRoulette(bool russianRoulette) {
    this->russianRoulette = russianRoulette;
}
