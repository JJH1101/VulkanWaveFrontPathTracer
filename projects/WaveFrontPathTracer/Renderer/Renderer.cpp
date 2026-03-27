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
#include "../Utils/BufferUtils.h"
#include "../Environment/Environment.h"
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
    if (rayType == "shadow")
        return SHADOW_RAYS;
    else if (rayType == "path")
        return PATH_RAYS;
    else
        return PRIMARY_RAYS;
}

float Renderer::computeRayHits(RayBuffer& rays) {

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

    PushConstantsCountRayHits pc{};
    pc.rayResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getResultBuffer().buffer);
    pc.rayHitAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, counterDevice.buffer);
    pc.numberOfRays = rays.getSize();

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsCountRayHits), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (rays.getSize() + workGroupSize - 1) / workGroupSize, 1, 1 };
    float time = countRayHitsPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);

    device->copyBuffer(&counterDevice, &counterHost, queue);
    counterHost.map();
    numberOfHits = *static_cast<uint32_t*>(counterHost.mapped);
    counterHost.unmap();

    return time;
}

float Renderer::initDecreases(int numberOfPixels) {

    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &decreases,
        numberOfPixels * sizeof(glm::vec4)
    );

    return vks::util::clearBufferTimed(*timer, *device, queue, &decreases, std::bit_cast<uint32_t>(1.0f));
}

float Renderer::interpolateColors(int numberOfPixels, vks::Buffer& pixels, vks::Buffer& framePixels) {

    PushConstantsInterpolateColors pc{};
    pc.framePixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, framePixels.buffer);
    pc.pixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, pixels.buffer);
    pc.numberOfPixels = numberOfPixels;
    pc.frameIndex = frameIndex;
    pc.keyValue = keyValue;
    pc.whitePoint = whitePoint;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsInterpolateColors), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numberOfPixels + workGroupSize - 1) / workGroupSize, 1, 1 };
    return interpolateColorsPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float Renderer::primaryPass(Camera & camera, glm::ivec2 extent, vks::Buffer & pixels) {
    float traceTime = tracePrimaryRays(camera, extent);
    float reconstructTime = initDecreases(extent.x * extent.y);
    reconstructTime += reconstructSmooth(primaryRays, pixels);
    numberOfPrimaryRays += (extent.x * extent.y);
    primaryTraceTime += traceTime;
    return traceTime + reconstructTime;
}

float Renderer::shadowPass(RayBuffer & inRays, vks::Buffer & inPixels, vks::Buffer & outPixels, bool replace) {
    float traceTime = 0.0f;
    float reconstructTime = 0.0f;
    int batchDiff = RENDERER_MAX_BATCH_SIZE / numberOfShadowSamples;
    int batchIndex = 0;
    int batchBegin;
    int batchEnd;
    do {
        batchBegin = batchIndex;
        batchEnd = std::min(batchBegin + batchDiff, inRays.getSize());
        traceTime += traceShadowRays(inRays, batchBegin, batchEnd);
        reconstructTime += reconstructShadow(inRays, inPixels, outPixels, batchBegin, batchEnd, replace);
        batchIndex = batchEnd;
    } while (batchIndex != inRays.getSize());
    float rayHitsTime = computeRayHits(inRays);
    numberOfShadowRays += numberOfShadowSamples * numberOfHits;
    shadowTraceTime += traceTime;
    return traceTime + reconstructTime + rayHitsTime;
}

float Renderer::pathPass(vks::Buffer& pixels, RayBuffer& inRays, RayBuffer& outRays) {
    float traceTime = tracePathRays(inRays, outRays);
    float reconstructTime = reconstructSmooth(outRays, pixels);
    pathTraceTime += traceTime;
    numberOfPathRays += outRays.getSize();
    return traceTime + reconstructTime;
}

float Renderer::renderPrimary(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
    return primaryPass(camera, extent, pixels);
}

float Renderer::renderShadow(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
    vks::util::clearBuffer(*device, queue, &auxPixels);
    float time = primaryPass(camera, extent, auxPixels);
    time += shadowPass(primaryRays, auxPixels, pixels, false);
    return time;
}

float Renderer::renderPath(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
    vks::util::clearBuffer(*device, queue, &auxPixels);
    float time = primaryPass(camera, extent, auxPixels);
    time += shadowPass(primaryRays, auxPixels, pixels, false);
    pathQueue.init(*device, queue, primaryRays.getSize());
    for (bounce = 0; bounce < recursionDepth; ++bounce) {
        vks::util::clearBuffer(*device, queue, &auxPixels);
        if (bounce > 0)
            time += pathPass(
                auxPixels,
                pathQueue.getInputRays(),
                pathQueue.getOutputRays()
            );
        else
            time += pathPass(
                auxPixels,
                primaryRays,
                pathQueue.getOutputRays()
            );
        if (pathQueue.getOutputRays().getSize() == 0) break;
        time += shadowPass(pathQueue.getOutputRays(), auxPixels, pixels, false);
        pathQueue.swap();
    }
    return time;
}

float Renderer::reconstructSmooth(RayBuffer & rays, vks::Buffer & pixels) {

    int numRays = rays.getSize();

    PushConstantsReconstructSmooth pc{};
    pc.rayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getRayBuffer().buffer);
    pc.resultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getResultBuffer().buffer);
    pc.idxToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, rays.getSlotToIndexBuffer().buffer);
    pc.pixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, pixels.buffer);
    pc.decreaseAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, decreases.buffer);
    pc.geometryAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, scene.geometries.buffer);
    pc.light = scene.light;
    pc.numberOfRays = numRays;
    pc.backgroundColor = scene.backgroundColor;
    pc.numberOfSamples = numberOfPrimarySamples;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsReconstructSmooth), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    return reconstructSmoothPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float Renderer::reconstructShadow(RayBuffer & inRays, vks::Buffer & inPixels, vks::Buffer & outPixels, int batchBegin, int batchEnd, bool replace) {

    PushConstantsReconstructShadow pc{};
    pc.outputResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getResultBuffer().buffer);
    pc.indexToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, inRays.getSlotToIndexBuffer().buffer);
    pc.indexToSlotAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getIndexToSlotBuffer().buffer);
    pc.inPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, inPixels.buffer);
    pc.outPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, outPixels.buffer);
    pc.batchBegin = batchBegin;
    pc.batchSize = batchEnd - batchBegin;
    pc.numberOfSamples = numberOfShadowSamples;
    pc.replace = replace;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsReconstructShadow), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { ((batchEnd - batchBegin) + workGroupSize - 1) / workGroupSize, 1, 1 };
    return reconstructShadowPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);

}

float Renderer::tracePrimaryRays(Camera & camera, glm::ivec2& extent) {
    float time = raygen.primary(primaryRays, camera, extent, pass + numberOfPrimarySamples * (frameIndex - 1));

    time += tracer.trace(primaryRays);

    return time;
}

float Renderer::traceShadowRays(RayBuffer & inRays, int batchBegin, int batchEnd) {
    float time = 0.0f;
    time += raygen.shadow(shadowRays, inRays, batchBegin, batchEnd, numberOfShadowSamples, scene.light, shadowRadius);
    if (sortShadowRays) {
#if SORT_LOG
        float sortTime;
        float traceSortTime;
        float mortoncodesTime;
        float traceTime = tracer.trace(shadowRays);
        time += tracer.traceSort(shadowRays, scene.minPos, scene.maxPos, mortoncodesTime, sortTime, traceSortTime);
        shadowRayCounts[bounce+1] += numberOfShadowSamples * numberOfHits;
        shadowMortoncodesTimes[bounce+1] += mortoncodesTime;
        shadowSortTimes[bounce+1] += sortTime;
        shadowTraceSortTimes[bounce+1] += traceSortTime;
        shadowTraceTimes[bounce+1] += traceTime;
#else
        time += tracer.traceSort(shadowRays, scene.minPos, scene.maxPos);
#endif
    }
    else time += tracer.trace(shadowRays);
    return time;
}

float Renderer::tracePathRays(RayBuffer & inRays, RayBuffer & outRays) {
    float time = raygen.path(outRays, inRays, decreases, scene.geometries);
    if (sortPathRays) {
#if SORT_LOG
        float sortTime;
        float traceSortTime;
        float mortoncodesTime;
        float traceTime = tracer.trace(outRays);
        time += tracer.traceSort(outRays, scene.minPos, scene.maxPos, mortoncodesTime, sortTime, traceSortTime);
        pathRayCounts[bounce] += outRays.getSize();
        pathMortoncodesTimes[bounce] += mortoncodesTime;
        pathSortTimes[bounce] += sortTime;
        pathTraceSortTimes[bounce] += traceSortTime;
        pathTraceTimes[bounce] += traceTime;
#else
        time += tracer.traceSort(outRays, scene.minPos, scene.maxPos);
#endif
    }
    else time += tracer.trace(outRays);
    return time;
}


Renderer::Renderer() :
    rayType(PRIMARY_RAYS),
    keyValue(0.4f),
    whitePoint(1.0f),
    shadowRadius(1.0f),
    numberOfPrimarySamples(1),
    numberOfShadowSamples(4),
    recursionDepth(3),
    frameIndex(1) {
}

Renderer::~Renderer() {
    auxPixels.destroy();
    decreases.destroy();
    seeds.destroy();
    counterDevice.destroy();
    counterHost.destroy();
}

void Renderer::init(vks::VulkanDevice& _device, VkQueue _queue, GPUTimer& _timer) {

    this->device = &_device;
    this->timer = &_timer;
    this->queue = _queue;

    raygen.init(*device, *timer, queue);
    tracer.init(*device, *timer, queue);

    // Create interpolateColors pipeline
    VkShaderModule shaderModule = vks::tools::loadShader((std::string(shaderPath) + "interpolateColors.comp.spv").c_str(), device->logicalDevice);
    VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsInterpolateColors) };
    ComputePass::PipelineContext pipelineContext;
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    interpolateColorsPass.createPipeline(*device, pipelineContext);

    // Create countRayHits pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "countRayHits.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsCountRayHits) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    countRayHitsPass.createPipeline(*device, pipelineContext);

    // Create reconstructSmooth pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "reconstructSmooth.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsReconstructSmooth) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    reconstructSmoothPass.createPipeline(*device, pipelineContext);

    // Create reconstructShadow pipeline
    shaderModule = vks::tools::loadShader((std::string(shaderPath) + "reconstructShadow.comp.spv").c_str(), device->logicalDevice);
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsReconstructShadow) };
    pipelineContext.shaderEntry.module = shaderModule;
    pipelineContext.pushConstantRanges = { pushConstantRange };
    reconstructShadowPass.createPipeline(*device, pipelineContext);

    std::string _rayType;
    Environment::getInstance()->getStringValue("Renderer.rayType", _rayType);
    setRayType(stringToRayType(_rayType));

    float _keyValue;
    Environment::getInstance()->getFloatValue("Renderer.keyValue", _keyValue);
    setKeyValue(_keyValue);
    float _whitePoint;
    Environment::getInstance()->getFloatValue("Renderer.whitePoint", _whitePoint);
    setWhitePoint(_whitePoint);
    int _numberOfPrimarySamples;
    Environment::getInstance()->getIntValue("Renderer.numberOfPrimarySamples", _numberOfPrimarySamples);
    setNumberOfPrimarySamples(_numberOfPrimarySamples);
    int _numberOfShadowSamples;
    Environment::getInstance()->getIntValue("Renderer.numberOfShadowSamples", _numberOfShadowSamples);
    setNumberOfShadowSamples(_numberOfShadowSamples);
    float _shadowRadius;
    Environment::getInstance()->getFloatValue("Renderer.shadowRadius", _shadowRadius);
    setShadowRadius(_shadowRadius);
    int _recursionDepth;
    Environment::getInstance()->getIntValue("Renderer.recursionDepth", _recursionDepth);
    setRecursionDepth(_recursionDepth);

    Environment::getInstance()->getBoolValue("Renderer.sortShadowRays", sortShadowRays);
    Environment::getInstance()->getBoolValue("Renderer.sortPathRays", sortPathRays);
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

bool Renderer::getSortPathRays() {
    return sortPathRays;
}

void Renderer::setSortPathRays(bool sortPathRays) {
    this->sortPathRays = sortPathRays;
}

void Renderer::setScene(vks::Buffer& geometries, glm::vec3& sceneMinPos, glm::vec3& sceneMaxPos, glm::vec3& light, glm::vec3& backgroundColor, VkAccelerationStructureKHR topLevelAS) {
    setGeometries(geometries);
    setSceneBounds(sceneMinPos, sceneMaxPos);
    setLight(light);
    setBackgroundColor(backgroundColor);
    setAccelerationStructure(topLevelAS);
}

void Renderer::setSceneBounds(glm::vec3& sceneMinPos, glm::vec3& sceneMaxPos) {
    scene.minPos = sceneMinPos;
    scene.maxPos = sceneMaxPos;
}

void Renderer::setLight(glm::vec3& light) {
    scene.light = light;
}

void Renderer::setBackgroundColor(glm::vec3& backgroundColor) {
    scene.backgroundColor = backgroundColor;
}

void Renderer::setGeometries(vks::Buffer& geometries) {
    scene.geometries = geometries;
}

void Renderer::setAccelerationStructure(VkAccelerationStructureKHR topLevelAS) {
    tracer.setAccererationStructure(topLevelAS);
}

float Renderer::render(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels) {

    // Elapsed time.
    float time = 0.0f;

    // Resize pixel buffers.
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &pixels,
        extent.x * extent.y * sizeof(glm::vec4)
    );
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &framePixels,
        extent.x * extent.y * sizeof(glm::vec4)
    );
    vks::util::resizeDiscardBuffer(
        *device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
    pathTraceTime = 0.0f;

    // Clear number of rays.
    numberOfPrimaryRays = 0;
    numberOfShadowRays = 0;
    numberOfPathRays = 0;

#ifdef SORT_LOG
    // Clear sort counts.
    for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; ++i) {
        shadowRayCounts[i] = 0;
        shadowMortoncodesTimes[i] = 0.0f;
        shadowSortTimes[i] = 0.0f;
        shadowTraceSortTimes[i] = 0.0f;
        shadowTraceTimes[i] = 0.0f;

        pathRayCounts[i] = 0;
        pathMortoncodesTimes[i] = 0.0f;
        pathSortTimes[i] = 0.0f;
        pathTraceSortTimes[i] = 0.0f;
        pathTraceTimes[i] = 0.0f;
    }
    shadowRayCounts[RENDERER_MAX_RECURSION_DEPTH] = 0;
    shadowMortoncodesTimes[RENDERER_MAX_RECURSION_DEPTH] = 0.0f;
    shadowSortTimes[RENDERER_MAX_RECURSION_DEPTH] = 0.0f;
    shadowTraceSortTimes[RENDERER_MAX_RECURSION_DEPTH] = 0.0f;
    shadowTraceTimes[RENDERER_MAX_RECURSION_DEPTH] = 0.0f;

    bounce = -1;
#endif

    // Init seeds.
    if (rayType == PATH_RAYS || rayType == SHADOW_RAYS)
        time += raygen.initSeeds(extent.x * extent.y, frameIndex);

    // For-each primary ray.
    for (pass = 0; pass < numberOfPrimarySamples; ++pass) {
        if (rayType == PRIMARY_RAYS)
            time += renderPrimary(camera, extent, framePixels);
        else if (rayType == SHADOW_RAYS)
            time += renderShadow(camera, extent, framePixels);
        else if (rayType == PATH_RAYS)
            time += renderPath(camera, extent, framePixels);
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

unsigned long long Renderer::getNumberOfPathRays() {
    return numberOfPathRays;
}

unsigned long long Renderer::getNumberOfRays() {
    return numberOfPrimaryRays + numberOfShadowRays + numberOfPathRays;
}

float Renderer::getPrimaryTraceTime() {
    return primaryTraceTime;
}

float Renderer::getShadowTraceTime() {
    return shadowTraceTime;
}

float Renderer::getPathTraceTime() {
    return pathTraceTime;
}

float Renderer::getTraceTime() {
    return primaryTraceTime + shadowTraceTime + pathTraceTime;
}

float Renderer::getPrimaryTracePerformance() {
    return primaryTraceTime == 0.0f ? 0.0f : numberOfPrimaryRays * 1.0e-3f / primaryTraceTime;
}

float Renderer::getShadowTracePerformance() {
    return shadowTraceTime == 0.0f ? 0.0f : numberOfShadowRays * 1.0e-3f / shadowTraceTime;
}

float Renderer::getPathTracePerformance() {
    return pathTraceTime == 0.0f ? 0.0f : numberOfPathRays * 1.0e-3f / pathTraceTime;
}

float Renderer::getTracePerformance() {
    return getTraceTime() == 0.0f ? 0.0f : getNumberOfRays() * 1.0e-3f / getTraceTime();
}

void Renderer::getShadowSortLog(int* rayCounts, float* mortoncodesTims, float* sortTimes, float* traceSortTimes, float* traceTimes) {
    for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH + 1; i++) {
        rayCounts[i] = shadowRayCounts[i];
        mortoncodesTims[i] = shadowMortoncodesTimes[i];
        sortTimes[i] = shadowSortTimes[i];
        traceSortTimes[i] = shadowTraceSortTimes[i];
        traceTimes[i] = shadowTraceTimes[i];
    }
}

void Renderer::getPathSortLog(int* rayCounts, float* mortoncodesTims, float* sortTimes, float* traceSortTimes, float* traceTimes) {
    for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; i++) {
        rayCounts[i] = pathRayCounts[i];
        mortoncodesTims[i] = pathMortoncodesTimes[i];
        sortTimes[i] = pathSortTimes[i];
        traceSortTimes[i] = pathTraceSortTimes[i];
        traceTimes[i] = pathTraceTimes[i];
    }
}