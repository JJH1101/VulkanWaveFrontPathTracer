/**
 * \file	Renderer.cpp
 * \author	Daniel Meister
 * \date	2014/05/10
 * \brief	Renderer class source file.
 */

/**
 * Modified Source
 * \file	Renderer.cpp
 * \author	Junhyeok Jang
 * \date	2026/03/10
 * \brief	Modified for Vulkan
 */

#include "Renderer.h"
#include "../Utils/BufferUtils.hpp"
#include <bit>

PathQueue::PathQueue() : swapBuffers(false) {
}

PathQueue::~PathQueue() {
}

void PathQueue::swap() {
    swapBuffers = !swapBuffers;
}

void PathQueue::init(vks::VulkanDevice& device, VkQueue queue, int size) {
    rays[0].resize(device, queue, size);
    rays[1].resize(device, queue, size);
}

RayBuffer & PathQueue::getInputRays() {
    return swapBuffers ? rays[1] : rays[0];
}

RayBuffer & PathQueue::getOutputRays() {
    return swapBuffers ? rays[0] : rays[1];
}

Renderer::RayType Renderer::stringToRayType(const std::string & rayType) {
    if (rayType == "primary")
        return PRIMARY_RAYS;
    else if (rayType == "shadow")
        return SHADOW_RAYS;
    else if (rayType == "ao")
        return AO_RAYS;
    else if (rayType == "path")
        return PATH_RAYS;
    else if (rayType == "pseudocolor")
        return PSEUDOCOLOR_RAYS;
    else
        return THERMAL_RAYS;
}

float Renderer::computeRayHits(RayBuffer & rays) {

    //// Kernel.
    //HipModule * module = compiler.compile();
    //HipKernel kernel = module->getKernel("countRayHits");

    //// Reset hit counter
    //module->getGlobal("rayHits").clear();

    //// Set params.
    //kernel.setParams(rays.getSize(), rays.getResultBuffer());

    //// Launch.
    //float time = kernel.launchTimed(rays.getSize(), Vec2i(HITS_BLOCK_THREADS, 1));

    //// Ray hits
    //numberOfHits = *(int*)module->getGlobal("rayHits").getPtr();

    //return time;

    return 0.f; // Not implemented yet
}

float Renderer::initDecreases(int numberOfPixels) {

    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &decreases,
        numberOfPixels * sizeof(glm::vec3)
    );

    return vks::util::clearBufferTimed(*timer, *device, queue, &decreases, std::bit_cast<uint32_t>(1.0f));
}

float Renderer::interpolateColors(int numberOfPixels, vks::Buffer& pixels, vks::Buffer& framePixels) {

    pcInterpolateColors.framePixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, framePixels.buffer);
    pcInterpolateColors.pixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, pixels.buffer);
    pcInterpolateColors.numberOfPixels = numberOfPixels;
    pcInterpolateColors.frameIndex = frameIndex;
    pcInterpolateColors.keyValue = keyValue;
    pcInterpolateColors.whitePoint = whitePoint;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsInterpolateColors), &pcInterpolateColors };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numberOfPixels + workGroupSize - 1) / workGroupSize, 1, 1 };
    return interpolateColorsPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float Renderer::primaryPass(vks::Buffer geometries, glm::vec3 light, glm::vec3 backgroundColor, Camera & camera, glm::ivec2 extent, vks::Buffer & pixels) {
    float traceTime = tracePrimaryRays(camera, extent);
    float reconstructTime = initDecreases(extent.x * extent.y);
    reconstructTime += reconstructSmooth(geometries, light, backgroundColor, primaryRays, pixels);
    numberOfPrimaryRays += (extent.x * extent.y);
    primaryTraceTime += traceTime;
    return traceTime + reconstructTime;
}

//float Renderer::shadowPass(Scene & scene, RayBuffer & inRays, Buffer & inPixels, Buffer & outPixels, bool replace) {
//    float traceTime = 0.0f;
//    float reconstructTime = 0.0f;
//    int batchDiff = RENDERER_MAX_BATCH_SIZE / numberOfShadowSamples;
//    int batchIndex = 0;
//    int batchBegin;
//    int batchEnd;
//    do {
//        batchBegin = batchIndex;
//        batchEnd = qMin(batchBegin + batchDiff, inRays.getSize());
//        traceTime += traceShadowRays(scene, inRays, batchBegin, batchEnd);
//        reconstructTime += reconstructShadow(inRays, inPixels, outPixels, batchBegin, batchEnd, replace);
//        batchIndex = batchEnd;
//    } while (batchIndex != inRays.getSize());
//    float rayHitsTime = computeRayHits(inRays);
//    numberOfShadowRays += numberOfShadowSamples * numberOfHits;
//    shadowTraceTime += traceTime;
//    return traceTime + reconstructTime + rayHitsTime;
//}
//
//float Renderer::aoPass(Scene & scene, RayBuffer & inRays, Buffer & inPixels, Buffer & outPixels, bool replace) {
//    float traceTime = 0.0f;
//    float reconstructTime = 0.0f;
//    int batchDiff = RENDERER_MAX_BATCH_SIZE / numberOfAOSamples;
//    int batchIndex = 0;
//    int batchBegin;
//    int batchEnd;
//    do {
//        batchBegin = batchIndex;
//        batchEnd = qMin(batchBegin + batchDiff, inRays.getSize());
//        traceTime += traceAORays(scene, inRays, batchBegin, batchEnd);
//        reconstructTime += reconstructAO(inRays, inPixels, outPixels, batchBegin, batchEnd, replace);
//        batchIndex = batchEnd;
//    } while (batchIndex != inRays.getSize());
//    float rayHitsTime = computeRayHits(inRays);
//    numberOfAORays += numberOfAOSamples * numberOfHits;
//    aoTraceTime += traceTime;
//    return traceTime + reconstructTime + rayHitsTime;
//}
//
//float Renderer::pathPass(Scene & scene, Buffer & pixels, RayBuffer & inRays, RayBuffer & outRays) {
//    float traceTime = tracePathRays(scene, inRays, outRays);
//    float reconstructTime = reconstructSmooth(scene, outRays, pixels);
//    pathTraceTime += traceTime;
//    numberOfPathRays += outRays.getSize();
//    return traceTime + reconstructTime;
//}

float Renderer::renderPrimary(vks::Buffer geometries, glm::vec3 light, glm::vec3 backgroundColor, Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
    return primaryPass(geometries, light, backgroundColor, camera, extent, pixels);
}

//float Renderer::renderShadow(Scene & scene, Camera & camera, Buffer & pixels) {
//    auxPixels.clear();
//    float time = primaryPass(scene, camera, auxPixels);
//    time += shadowPass(scene, primaryRays, auxPixels, pixels, false);
//    return time;
//}
//
//float Renderer::renderAO(Scene & scene, Camera & camera, Buffer & pixels) {
//    auxPixels.clear();
//    float time = primaryPass(scene, camera, auxPixels);
//    time += aoPass(scene, primaryRays, auxPixels, pixels, false);
//    return time;
//}
//
//float Renderer::renderPath(Scene & scene, Camera & camera, Buffer & pixels) {
//    auxPixels.clear();
//    float time = primaryPass(scene, camera, auxPixels);
//    time += shadowPass(scene, primaryRays, auxPixels, pixels, false);
//    pathQueue.init(primaryRays.getSize());
//    for (bounce = 0; bounce < recursionDepth; ++bounce) {
//        auxPixels.clear();
//        if (bounce > 0)
//            time += pathPass(
//                scene,
//                auxPixels,
//                pathQueue.getInputRays(),
//                pathQueue.getOutputRays()
//            );
//        else
//            time += pathPass(
//                scene,
//                auxPixels,
//                primaryRays,
//                pathQueue.getOutputRays()
//            );
//        if (pathQueue.getOutputRays().getSize() == 0) break;
//        time += shadowPass(scene, pathQueue.getOutputRays(), auxPixels, pixels, false);
//        pathQueue.swap();
//    }
//    return time;
//}
//
//float Renderer::renderPseudocolor(Scene & scene, Camera & camera, Buffer & pixels) {
//    float time = tracePrimaryRays(camera);
//    time += reconstructPseudocolor(scene, pixels);
//    return time;
//}
//
//float Renderer::renderThermal(Camera & camera, Buffer & pixels) {
//    float time = tracePrimaryRays(camera);
//    time += reconstructThermal(pixels);
//    return time;
//}

float Renderer::reconstructSmooth(vks::Buffer & geometies, glm::vec3 light, glm::vec3 backgroundColor, RayBuffer & rays, vks::Buffer & pixels) {

    int numRays = rays.getSize();

    pcReconstructSmooth.rayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getRayBuffer().buffer);
    pcReconstructSmooth.resultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getResultBuffer().buffer);
    pcReconstructSmooth.idxToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getIndexToSlotBuffer().buffer);
    pcReconstructSmooth.pixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, pixels.buffer);
    pcReconstructSmooth.decreaseAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, decreases.buffer);
    pcReconstructSmooth.geometryAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, geometies.buffer);
    pcReconstructSmooth.light = light;
    pcReconstructSmooth.numberOfRays = numRays;
    pcReconstructSmooth.backgroundColor = backgroundColor;
    pcReconstructSmooth.numberOfSamples = numberOfPrimarySamples;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsReconstructSmooth), &pcReconstructSmooth };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    return reconstructSmoothPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

//float Renderer::reconstructPseudocolor(Scene & scene, Buffer & pixels) {
//
//    // Colorize scene.
//    tracer.getBVH()->colorizeScene(nodeSizeThreshold);
//
//    // Kernel.
//    HipModule * module = compiler.compile();
//    HipKernel kernel = module->getKernel("reconstructPseudocolor");
//
//    // Set params.
//    kernel.setParams(
//        primaryRays.getSize(),
//        numberOfPrimarySamples,
//        scene.getMatIndexBuffer(),
//        scene.getTriangleBuffer(),
//        scene.getNormalBuffer(),
//        scene.getPseudocolorBuffer(),
//        primaryRays.getRayBuffer(),
//        primaryRays.getResultBuffer(),
//        scene.getLight(),
//        primaryRays.getSlotToIndexBuffer(),
//        pixels
//    );
//
//    // Launch.
//    return kernel.launchTimed(primaryRays.getSize());
//
//}
//
//float Renderer::reconstructThermal(Buffer & pixels) {
//
//    // Kernel.
//    HipModule * module = compiler.compile();
//    HipKernel kernel = module->getKernel("reconstructThermal");
//
//    // Set params.
//    kernel.setParams(
//        primaryRays.getSize(),
//        numberOfPrimarySamples,
//        thermalThreshold,
//        primaryRays.getSlotToIndexBuffer(),
//        primaryRays.getStatBuffer(),
//        pixels
//    );
//
//    // Launch.
//    return kernel.launchTimed(primaryRays.getSize());
//
//}
//
//float Renderer::reconstructShadow(RayBuffer & inRays, Buffer & inPixels, Buffer & outPixels, int batchBegin, int batchEnd, bool replace) {
//
//    // Kernel.
//    HipModule * module = compiler.compile();
//    HipKernel kernel = module->getKernel("reconstructShadow");
//
//    // Set params.
//    kernel.setParams(
//        batchBegin,
//        batchEnd - batchBegin,
//        numberOfShadowSamples,
//        replace,
//        shadowRays.getResultBuffer(),
//        inRays.getSlotToIndexBuffer(),
//        shadowRays.getIndexToSlotBuffer(),
//        inPixels,
//        outPixels
//    );
//
//    // Launch.
//    return kernel.launchTimed(batchEnd - batchBegin);
//
//}
//
//float Renderer::reconstructAO(RayBuffer & inRays, Buffer & inPixels, Buffer & outPixels, int batchBegin, int batchEnd, bool replace) {
//
//    // Kernel.
//    HipModule * module = compiler.compile();
//    HipKernel kernel = module->getKernel("reconstructAO");
//
//    // Set params.
//    kernel.setParams(
//        batchBegin,
//        batchEnd - batchBegin,
//        numberOfAOSamples,
//        replace,
//        aoRays.getResultBuffer(),
//        inRays.getSlotToIndexBuffer(),
//        aoRays.getIndexToSlotBuffer(),
//        inPixels,
//        outPixels
//    );
//
//    // Launch.
//    return kernel.launchTimed(batchEnd - batchBegin);
//
//}

float Renderer::tracePrimaryRays(Camera & camera, glm::ivec2& extent) {
    float time = raygen.primary(primaryRays, camera, extent, pass + numberOfPrimarySamples * (frameIndex - 1));

    time += trace(primaryRays);

    return time;
}

//float Renderer::traceShadowRays(Scene & scene, RayBuffer & inRays, int batchBegin, int batchEnd) {
//    float time = 0.0f;
//    time += raygen.shadow(shadowRays, inRays, batchBegin, batchEnd, numberOfShadowSamples, scene.getLight(), shadowRadius);
//    if (sortShadowRays) time += tracer.traceSort(shadowRays);
//    else time += tracer.trace(shadowRays);
//    return time;
//}
//
//float Renderer::traceAORays(Scene & scene, RayBuffer & inRays, int batchBegin, int batchEnd) {
//    float time = raygen.ao(aoRays, inRays, scene, batchBegin, batchEnd, numberOfAOSamples, aoRadius);
//    if (sortAORays) time += tracer.traceSort(aoRays);
//    else time += tracer.trace(aoRays);
//    return time;
//}
//
//float Renderer::tracePathRays(Scene & scene, RayBuffer & inRays, RayBuffer & outRays) {
//    float time = raygen.path(outRays, inRays, decreases, scene);
//    if (sortPathRays) {
//#if SORT_LOG
//        float sortTime;
//        float traceSortTime;
//        float traceTime = tracer.trace(outRays);
//        time += tracer.traceSort(outRays, sortTime, traceSortTime);
//        avgRayCounts[bounce] += outRays.getSize() / getNumberOfPrimarySamples();
//        sortTimes[bounce] += sortTime;
//        traceSortTimes[bounce] += traceSortTime;
//        traceTimes[bounce] += tracer.trace(outRays);
//#else
//        time += tracer.traceSort(outRays);
//#endif
//    }
//    else time += tracer.trace(outRays);
//    return time;
//}

float Renderer::trace(RayBuffer& rays) {
    int numRays = rays.getSize();

    // Set push constants
    pcTrace.rayBufferAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getRayBuffer().buffer);
    pcTrace.resultBufferAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getResultBuffer().buffer);
    pcTrace.numRays = numRays;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsTrace), &pcTrace };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };
    std::vector<VkDescriptorSet> descriptorSets = { descriptorSetTrace };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    return tracePass.launchTimed(*timer, queue, dispatchDesc, descriptorSets, {}, pushConstantDescs);
}

Renderer::Renderer() :
    rayType(PRIMARY_RAYS),
    keyValue(1.0f),
    whitePoint(1.0f),
    aoRadius(1.0f),
    shadowRadius(1.0f),
    numberOfPrimarySamples(1),
    numberOfAOSamples(4),
    numberOfShadowSamples(4),
    recursionDepth(3),
    frameIndex(1),
    nodeSizeThreshold(5000),
    thermalThreshold(200) {
}

Renderer::~Renderer() {
    auxPixels.destroy();
    decreases.destroy();
    seeds.destroy();

    vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayoutTrace, nullptr);
    vkDestroyDescriptorPool(device->logicalDevice, descriptorPoolTrace, nullptr);
}

void Renderer::init(vks::VulkanDevice& _device, VkQueue _queue, GPUTimer& _timer, VkAccelerationStructureKHR topLevelAS) {

    this->device = &_device;
    this->timer = &_timer;
    this->queue = _queue;

    raygen.init(*device, *timer, queue);

    // Create interpolateColors pipeline
    VkShaderModule shaderModule = vks::tools::loadShader((std::string(shaderPath) + "interpolateColors.comp.spv").c_str(), device->logicalDevice);
    VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsInterpolateColors) };
    ComputePass::PipelineContext pipelineContext;
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    interpolateColorsPass.createPipeline(*device, pipelineContext);

    // Create reconstructSmooth pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "reconstructSmooth.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsReconstructSmooth) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    reconstructSmoothPass.createPipeline(*device, pipelineContext);

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

    // Create trace pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "trace.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsTrace) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.descriptorSetLayouts = { descriptorSetLayoutTrace };
    pipelineContext.pushConstantRanges = { pushConstantRange };
    tracePass.createPipeline(*device, pipelineContext);
}

Renderer::RayType Renderer::getRayType() {
    return rayType;
}

void Renderer::setKeyValue(float keyValue) {
    if (keyValue <= 0 || keyValue > RENDERER_MAX_KEY_VALUE) {
        std::cout << "WARN <Renderer> KeyValue must be in range (0," << RENDERER_MAX_KEY_VALUE << "].\n";
    }
    else {
        this->keyValue = keyValue;
        resetFrameIndex();
    }
}

float Renderer::getKeyValue() {
    return keyValue;
}

void Renderer::setWhitePoint(float whitePoint) {
    if (whitePoint <= 0 || whitePoint > RENDERER_MAX_WHITE_POINT) {
        std::cout << "WARN <Renderer> WhitePoint must be in range (0," << RENDERER_MAX_WHITE_POINT << "].\n";
    }
    else {
        this->whitePoint = whitePoint;
        resetFrameIndex();
    }
}

float Renderer::getWhitePoint() {
    return whitePoint;
}

void Renderer::setRayType(RayType rayType) {
    resetFrameIndex();
    this->rayType = rayType;
}

float Renderer::getAORadius() {
    return aoRadius;
}

void Renderer::setAORadius(float aoRadius) {
    if (aoRadius <= 0 || aoRadius > RENDERER_MAX_RADIUS) {
        std::cout << "WARN <Renderer> AO radius must be in range (0," << RENDERER_MAX_RADIUS << "].\n";
    }
    else {
        this->aoRadius = aoRadius;
        resetFrameIndex();
    }
}

float Renderer::getShadowRadius() {
    return shadowRadius;
}
void Renderer::setShadowRadius(float shadowRadius) {
    if (shadowRadius <= 0 || shadowRadius > RENDERER_MAX_RADIUS) {
        std::cout << "WARN <Renderer> Shadow radius must be in range (0," << RENDERER_MAX_RADIUS << "].\n";
    }
    else {
        this->shadowRadius = shadowRadius;
        resetFrameIndex();
    }
}

int Renderer::getNumberOfPrimarySamples(void) {
    return numberOfPrimarySamples;
}

void Renderer::setNumberOfPrimarySamples(int numberOfPrimarySamples) {
    if (numberOfPrimarySamples <= 0 || numberOfPrimarySamples > RENDERER_MAX_SAMPLES) {
        std::cout << "WARN <Renderer> Number of primary samples must be in range (0," << RENDERER_MAX_SAMPLES << "].\n";
    }
    else {
        this->numberOfPrimarySamples = numberOfPrimarySamples;
        resetFrameIndex();
    }
}

int Renderer::getNumberOfAOSamples() {
    return numberOfAOSamples;
}

void Renderer::setNumberOfAOSamples(int numberOfAOSamples) {
    if (numberOfAOSamples <= 0 || numberOfAOSamples > RENDERER_MAX_SAMPLES) {
        std::cout << "WARN <Renderer> Number of AO samples must be in range (0," << RENDERER_MAX_SAMPLES << "].\n";
    }
    else {
        this->numberOfAOSamples = numberOfAOSamples;
        resetFrameIndex();
    }
}

int Renderer::getNumberOfShadowSamples() {
    return numberOfShadowSamples;
}

void Renderer::setNumberOfShadowSamples(int numberOfShadowSamples) {
    if (numberOfShadowSamples <= 0 || numberOfShadowSamples > RENDERER_MAX_SAMPLES) {
        std::cout << "WARN <Renderer> Number of shadow samples must be in range (0," << RENDERER_MAX_SAMPLES << "].\n";
    }
    else {
        this->numberOfShadowSamples = numberOfShadowSamples;
        resetFrameIndex();
    }
}

int Renderer::getRecursionDepth() {
    return recursionDepth;
}

void Renderer::setRecursionDepth(int recursionDepth) {
    if (recursionDepth < 0 || recursionDepth > RENDERER_MAX_RECURSION_DEPTH) {
        std::cout << "WARN <Renderer> Recursion depth must be in range (0," << RENDERER_MAX_RECURSION_DEPTH << "].\n";
    }
    else {
        this->recursionDepth = recursionDepth;
        resetFrameIndex();
    }
}

int Renderer::getNodeSizeThreshold() {
    return nodeSizeThreshold;
}

void Renderer::setNodeSizeThreshold(int nodeSizeThreshold) {
    if (nodeSizeThreshold <= 0) {
        std::cout << "WARN <Renderer> Node size threshold must be positive.\n";
    }
    else {
        this->nodeSizeThreshold = nodeSizeThreshold;
        resetFrameIndex();
    }
}

int Renderer::getThermalThreshold() {
    return thermalThreshold;
}

void Renderer::setThermalThreshold(int thermalThreshold) {
    if (thermalThreshold <= 0) {
        std::cout << "WARN <Renderer> Thermal threshold must be positive.\n";
    }
    else {
        this->thermalThreshold = thermalThreshold;
        resetFrameIndex();
    }
}

bool Renderer::getRussianRoulette() {
    return raygen.getRussianRoulette();
}

void Renderer::setRussianRoulette(bool russianRoulette) {
    raygen.setRussianRoulette(russianRoulette);
}

bool Renderer::getSortShadowRays() {
    return sortShadowRays;
}

void Renderer::setSortShadowRays(bool sortShadowRays) {
    this->sortShadowRays = sortShadowRays;
}

bool Renderer::getSortAORays() {
    return sortAORays;
}

void Renderer::setSortAORays(bool sortAORays) {
    this->sortAORays = sortAORays;
}

bool Renderer::getSortPathRays() {
    return sortPathRays;
}

void Renderer::setSortPathRays(bool sortPathRays) {
    this->sortPathRays = sortPathRays;
}

float Renderer::getShadowRayLength(void) {
    return shadowRays.getRayLength();
}

void Renderer::setShadowRayLength(float shadowRayLength) {
    shadowRays.setRayLength(shadowRayLength);
}

float Renderer::getAORayLength(void) {
    return aoRays.getRayLength();
}

void Renderer::setAORayLength(float aoRayLength) {
    if (getAORadius() > aoRayLength) std::cout << "WARN <Renderer> AO Ray length must be less or equal to ao radius.\n";
    else aoRays.setRayLength(aoRayLength);
}

float Renderer::getPathRayLength(void) {
    assert(pathQueue.getInputRays().getRayLength() == pathQueue.getOutputRays().getRayLength());
    return pathQueue.getInputRays().getRayLength();
}

void Renderer::setPathRayLength(float pathRayLength) {
    pathQueue.getInputRays().setRayLength(pathRayLength);
    pathQueue.getOutputRays().setRayLength(pathRayLength);
}

int Renderer::getShadowMortonCodeBits() {
    return shadowRays.getMortonCodeBits();
}

void Renderer::setShadowMortonCodeBits(int shadowMortonCodeBits) {
    shadowRays.setMortonCodeBits(shadowMortonCodeBits);
}

int Renderer::getAOMortonCodeBits() {
    return aoRays.getMortonCodeBits();
}
void Renderer::setAOMortonCodeBits(int aoMortonCodeBits) {
    aoRays.setMortonCodeBits(aoMortonCodeBits);
}

int Renderer::getPathMortonCodeBits() {
    assert(pathQueue.getInputRays().getMortonCodeBits() == pathQueue.getOutputRays().getMortonCodeBits());
    return pathQueue.getInputRays().getMortonCodeBits();
}

void Renderer::setPathMortonCodeBits(int pathMortonCodeBits) {
    pathQueue.getInputRays().setMortonCodeBits(pathMortonCodeBits);
    pathQueue.getOutputRays().setMortonCodeBits(pathMortonCodeBits);
}

float Renderer::render(vks::Buffer geometries, glm::vec3 light, glm::vec3 backgroundColor, Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels) {

    // Elapsed time.
    float time = 0.0f;

    // Resize pixel buffers.
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &pixels,
        extent.x * extent.y * sizeof(glm::vec4)
    );
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &framePixels,
        extent.x * extent.y * sizeof(glm::vec4)
    );
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &auxPixels,
        extent.x * extent.y * sizeof(glm::vec4)
    );

    // Clear pixel buffer.
    vks::util::clearBuffer(*device, queue, &framePixels);
    if (frameIndex == 1)
        vks::util::clearBuffer(*device, queue, &pixels);

    // Clear trace times.
    primaryTraceTime = 0.0f;
    shadowTraceTime = 0.0f;
    aoTraceTime = 0.0f;
    pathTraceTime = 0.0f;

    // Clear number of rays.
    numberOfPrimaryRays = 0;
    numberOfShadowRays = 0;
    numberOfAORays = 0;
    numberOfPathRays = 0;

    // Clear sort counts.
    for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; ++i) {
        avgRayCounts[i] = 0;
        sortTimes[i] = 0.0f;
        traceSortTimes[i] = 0.0f;
        traceTimes[i] = 0.0f;
    }

    // Init seeds.
    if (rayType == PATH_RAYS || rayType == SHADOW_RAYS || rayType == AO_RAYS)
        time += raygen.initSeeds(extent.x * extent.y, frameIndex);

    // For-each primary ray.
    for (pass = 0; pass < numberOfPrimarySamples; ++pass) {
        if (rayType == PRIMARY_RAYS)
            time += renderPrimary(geometries, light, backgroundColor, camera, extent, framePixels);
        //else if (rayType == SHADOW_RAYS)
        //    time += renderShadow(scene, camera, framePixels);
        //else if (rayType == AO_RAYS)
        //    time += renderAO(scene, camera, framePixels);
        //else if (rayType == PATH_RAYS)
        //    time += renderPath(scene, camera, framePixels);
        //else if (rayType == PSEUDOCOLOR_RAYS)
        //    time += renderPseudocolor(scene, camera, framePixels);
        //else if (rayType == THERMAL_RAYS)
        //    time += renderThermal(camera, framePixels);
    }

    //// Log sort counts.
    //QString mode;
    //Environment::getInstance()->getStringValue("Application.mode", mode);
    //if (mode != "interactive") {
    //    QString output;
    //    Environment::getInstance()->getStringValue("Benchmark.output", output);
    //    QString logDir = "benchmark/" + output;
    //    if (!QDir(logDir).exists())
    //        QDir().mkpath(logDir);
    //    Logger sort(logDir + "/sort_" + output + ".log");
    //    for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; ++i)
    //        sort << avgRayCounts[i] << " " << sortTimes[i] << " " << traceSortTimes[i] << " " << traceTimes[i] << "\n";
    //}

    // Interpolate colors.
    time += interpolateColors(extent.x * extent.y, pixels, framePixels);

    // Inc. frame number.
    ++frameIndex;

    return time;

}

void Renderer::resetFrameIndex() {
    frameIndex = 1;
}

unsigned long long Renderer::getNumberOfPrimaryRays() {
    return numberOfPrimaryRays;
}

unsigned long long Renderer::getNumberOfShadowRays() {
    return numberOfShadowRays;
}

unsigned long long Renderer::getNumberOfAORays() {
    return numberOfAORays;
}

unsigned long long Renderer::getNumberOfPathRays() {
    return numberOfPathRays;
}

unsigned long long Renderer::getNumberOfRays() {
    return numberOfPrimaryRays + numberOfShadowRays + numberOfAORays + numberOfPathRays;
}

float Renderer::getPrimaryTraceTime() {
    return primaryTraceTime;
}

float Renderer::getShadowTraceTime() {
    return shadowTraceTime;
}

float Renderer::getAOTraceTime() {
    return aoTraceTime;
}

float Renderer::getPathTraceTime() {
    return pathTraceTime;
}

float Renderer::getTraceTime() {
    return primaryTraceTime + shadowTraceTime + aoTraceTime + pathTraceTime;
}

float Renderer::getPrimaryTracePerformance() {
    return primaryTraceTime == 0.0f ? 0.0f : numberOfPrimaryRays * 1.0e-6f / primaryTraceTime;
}

float Renderer::getShadowTracePerformance() {
    return shadowTraceTime == 0.0f ? 0.0f : numberOfShadowRays * 1.0e-6f / shadowTraceTime;
}

float Renderer::getAOTracePerformance() {
    return aoTraceTime == 0.0f ? 0.0f : numberOfAORays * 1.0e-6f / aoTraceTime;
}

float Renderer::getPathTracePerformance() {
    return pathTraceTime == 0.0f ? 0.0f : numberOfPathRays * 1.0e-6f / pathTraceTime;
}

float Renderer::getTracePerformance() {
    return getTraceTime() == 0.0f ? 0.0f : getNumberOfRays() * 1.0e-6f / getTraceTime();
}
