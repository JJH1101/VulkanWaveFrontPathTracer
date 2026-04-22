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
#include <array>

PathQueue::PathQueue() : swapBuffers(false) {
}

PathQueue::~PathQueue() {
}

void PathQueue::swap() {
    swapBuffers = !swapBuffers;
}

void PathQueue::init(vks::VulkanDevice& device, int size) {
    rays[0].resize(device, size);
    rays[1].resize(device, size);
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
    pc.numberOfSamples = samplesPerPixel;
    pc.frameIndex = frameIndex;
    pc.keyValue = keyValue;
    pc.whitePoint = whitePoint;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsInterpolateColors), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numberOfPixels + workGroupSize - 1) / workGroupSize, 1, 1 };
    return interpolateColorsPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

float Renderer::renderPrimary(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
    float time = raygenPrimary(camera, extent, pass + samplesPerPixel * (frameIndex - 1));
    float traceTime = tracer.trace(primaryRays);
    time += traceTime;
	time += initDecreases(extent.x * extent.y);
    time += reconstructSmooth(primaryRays, pixels, false);

    numberOfPrimaryRays += (extent.x * extent.y);
	primaryTraceTime += traceTime;

	return time;
}

float Renderer::renderShadow(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
    float time = raygenPrimary(camera, extent, pass + samplesPerPixel * (frameIndex - 1));
    float traceTime = tracer.trace(primaryRays);
    time += traceTime;
    time += initDecreases(extent.x * extent.y);
    time += reconstructSmooth(primaryRays, auxPixels);

    numberOfPrimaryRays += (extent.x * extent.y);
    primaryTraceTime += traceTime;
    
    time += computeRayHits(primaryRays);

	traceTime = tracer.trace(shadowRays);
	time += traceTime;
	time += reconstructShadow(auxPixels, pixels, false);

    numberOfShadowRays += numberOfHits;
	shadowTraceTime += traceTime;

    return time;
}

float Renderer::renderPath(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels) {
	float time = initDecreases(extent.x * extent.y);
    time += raygenPrimary(camera, extent, pass + samplesPerPixel * (frameIndex - 1));
    
    for (bounce = 0; bounce <= recursionDepth; ++bounce) {
		RayBuffer& inRays = bounce == 0 ? primaryRays : pathQueue.getInputRays();
		
        float traceTime = 0.0f;

        // Path rays.
        if (bounce > 0) {
			traceTime = traceRays(inRays, &pathBounceLogs[bounce - 1], sortPathRays, reorderPathRays);
            numberOfPathRays += inRays.getSize();
            pathTraceTime += traceTime;
        }
        // Bounce = 0 -> primary rays.
        else {
			traceTime = traceRays(inRays);
            numberOfPrimaryRays += (extent.x * extent.y);
            primaryTraceTime += traceTime;
        }
        time += traceTime;

        // Not last bounce.
        if (bounce != recursionDepth) {
            time += reconstructSmooth(inRays, pathQueue.getOutputRays(), auxPixels);
        }
		// Last bounce -> no need to generate path rays for next bounce.
        else {
            time += reconstructSmooth(inRays, auxPixels);
        }

        time += computeRayHits(inRays);

		traceTime = traceRays(shadowRays, &shadowBounceLogs[bounce], sortShadowRays, reorderShadowRays);
        numberOfShadowRays += numberOfHits;
        shadowTraceTime += traceTime;
        time += traceTime;

        time += reconstructShadow(auxPixels, pixels, false);

		pathQueue.swap();
    }

    return time;
}

float Renderer::traceRays(RayBuffer& rays, BounceLog* bounceLog, bool sortRays, bool reorderRays) {
	float traceTime = 0.0f;
    std::array<float, 4> times{};
    
    if (sortRays) {
        traceTime = tracer.traceSort(rays, sceneMinPos, sceneMaxPos, reorderRays, times);
    }
    else {
        traceTime = tracer.trace(rays);
    }
    
    if (mode == "benchmark" && bounceLog != nullptr && printBounceLogs) {
        bounceLog->rayCount += rays.getSize();
        if (sortRays) {
            bounceLog->mortonCodesTime += times[0];
            bounceLog->sortTime += times[1];
            bounceLog->reorderTime += times[2];
            bounceLog->traceTime += times[3];
        }
        else {
            bounceLog->traceTime += traceTime;
        }
    }

	return traceTime;
}

float Renderer::reconstructSmooth(RayBuffer& irays, vks::Buffer& pixels, bool genShadow) {
    uint32_t numRays = irays.getSize();
    if (genShadow) {
        shadowRays.resize(*device, numRays);
        shadowRays.setClosestHit(false);
    }

    PushConstantsReconstructSmooth pc{};
    pc.inputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getRayBuffer().buffer);
    pc.inputResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getResultBuffer().buffer);
    pc.inputIdxToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getIndexToPixelBuffer().buffer);
    pc.pixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, pixels.buffer);
    pc.decreaseAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, decreases.buffer);
    pc.seedAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, seeds.buffer);
    pc.geometryNodeAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, geometryNodes.buffer);
    pc.shadowRayAddr = genShadow ? vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getRayBuffer().buffer) : 0;
    pc.shadowIdxToPixelAddr = genShadow ? vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getIndexToPixelBuffer().buffer) : 0;
    pc.pathRayAddr = 0;
    pc.pathIdxToPixelAddr = 0;
    pc.rayCounterAddr = 0;
    pc.light = light;
    pc.lightRadius = lightRadius;
    pc.numberOfRays = numRays;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsReconstructSmooth), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };
    std::vector<VkDescriptorSet> descriptorSets = { descriptorSet };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    return reconstructSmoothPass.launchTimed(*timer, queue, dispatchDesc, descriptorSets, {}, pushConstantDescs);
}

float Renderer::reconstructSmooth(RayBuffer & irays, RayBuffer & orays, vks::Buffer & pixels) {

    uint32_t numRays = irays.getSize();
	orays.resize(*device, numRays);
    orays.setClosestHit(true);
	shadowRays.resize(*device, numRays);
	shadowRays.setClosestHit(false);

    vks::util::clearBuffer(*device, queue, &counterDevice);

    PushConstantsReconstructSmooth pc{};
	pc.inputRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getRayBuffer().buffer);
	pc.inputResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getResultBuffer().buffer);
	pc.inputIdxToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, irays.getIndexToPixelBuffer().buffer);
	pc.pixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, pixels.buffer);
	pc.decreaseAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, decreases.buffer);
	pc.seedAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, seeds.buffer);
	pc.geometryNodeAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, geometryNodes.buffer);
    pc.shadowRayAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getRayBuffer().buffer);
	pc.shadowIdxToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getIndexToPixelBuffer().buffer);
	pc.pathRayAddr =  vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getRayBuffer().buffer);
	pc.pathIdxToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, orays.getIndexToPixelBuffer().buffer);
	pc.rayCounterAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, counterDevice.buffer);
	pc.light = light;
	pc.lightRadius = lightRadius;
	pc.russianRoulette = russianRoulette ? 1 : 0;
	pc.numberOfRays = numRays;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsReconstructSmooth), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };
    std::vector<VkDescriptorSet> descriptorSets = { descriptorSet };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    float time = reconstructSmoothPass.launchTimed(*timer, queue, dispatchDesc, descriptorSets, {}, pushConstantDescs);

    device->copyBuffer(&counterDevice, &counterHost, queue);
    counterHost.map();
    uint32_t rayCount = *static_cast<uint32_t*>(counterHost.mapped);
    counterHost.unmap();

    orays.resize(*device, rayCount);
	
    return time;
}

float Renderer::reconstructShadow(vks::Buffer & inPixels, vks::Buffer & outPixels,  bool replace) {
	uint32_t numRays = shadowRays.getSize();

    PushConstantsReconstructShadow pc{};
	pc.outputResultAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getResultBuffer().buffer);
	pc.indexToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, shadowRays.getIndexToPixelBuffer().buffer);
	pc.inPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, inPixels.buffer);
	pc.outPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, outPixels.buffer);
	pc.numberOfRays = numRays;
	pc.replace = replace ? 1 : 0;

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsReconstructShadow), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { (numRays + workGroupSize - 1) / workGroupSize, 1, 1 };
    return reconstructShadowPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);

}

float Renderer::initSeeds(int numberOfPixels, int frameIndex) {

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

float Renderer::raygenPrimary(Camera& camera, glm::ivec2& extent, int sampleIndex) {

    // Closest hit.
    pixelTable.setSize(extent, *device, queue);
    primaryRays.resize(*device, extent.x * extent.y, true);
    primaryRays.setClosestHit(true);
    primaryRays.getIndexToPixelBuffer() = pixelTable.getIndexToPixel();

    // Set push constants.
    PushConstantsRaygenPrimary pc{};
    pc.indexToPixelAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, primaryRays.getIndexToPixelBuffer().buffer);
    pc.rayBufferAddr = vks::util::getBufferDeviceAddress(device->logicalDevice, primaryRays.getRayBuffer().buffer);
    pc.screenToWorld = glm::inverse(camera.matrices.perspective * camera.matrices.view);
    pc.origin = camera.position;
    pc.sampleIndex = sampleIndex;
    pc.size = extent;
    pc.maxDist = camera.getFarClip();

    ComputePass::PushConstantDesc pushConstantDesc = { 0, sizeof(PushConstantsRaygenPrimary), &pc };
    std::vector<ComputePass::PushConstantDesc> pushConstantDescs = { pushConstantDesc };

    // Launch
    ComputePass::DispatchDesc dispatchDesc = { ((extent.x * extent.y) + workGroupSize - 1) / workGroupSize, 1, 1 };
    return raygenPrimaryPass.launchTimed(*timer, queue, dispatchDesc, {}, {}, pushConstantDescs);
}

Renderer::Renderer() :
    rayType(PRIMARY_RAYS),
    keyValue(0.4f),
    whitePoint(1.0f),
    samplesPerPixel(1),
    recursionDepth(3),
    frameIndex(1) {
}

Renderer::~Renderer() {
	initSeedsPass.destroy();
	raygenPrimaryPass.destroy();
	countRayHitsPass.destroy();
	interpolateColorsPass.destroy();
	reconstructSmoothPass.destroy();
	reconstructShadowPass.destroy();

    vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);

	geometryNodes.destroy();
    auxPixels.destroy();
    decreases.destroy();
    seeds.destroy();
    counterDevice.destroy();
    counterHost.destroy();
}

void Renderer::createDescriptorSet(vkglTF::Model& model) {
	uint32_t imageCount = static_cast<uint32_t>(model.textures.size());

    // Create descriptor set
    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = &poolSize;
    descriptorPoolInfo.maxSets = 1;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = imageCount;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutInfo.bindingCount = 1;
    descriptorSetLayoutInfo.pBindings = &binding;
    vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    descriptorSetAllocateInfo.descriptorSetCount = 1;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &descriptorSetAllocateInfo, &descriptorSet));

    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (auto texture : model.textures) {
        VkDescriptorImageInfo descriptor{};
        descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptor.sampler = texture.sampler;
        descriptor.imageView = texture.view;
        textureDescriptors.push_back(descriptor);
    }

    VkWriteDescriptorSet writeDescriptorImgArray{};
    writeDescriptorImgArray.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorImgArray.dstBinding = 0;
    writeDescriptorImgArray.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorImgArray.descriptorCount = imageCount;
    writeDescriptorImgArray.dstSet = descriptorSet;
    writeDescriptorImgArray.pImageInfo = textureDescriptors.data();

    vkUpdateDescriptorSets(device->logicalDevice, 1, &writeDescriptorImgArray, 0, nullptr);
}

void Renderer::createGeometryNodeBuffer(vkglTF::Model& model) {
	std::vector<GeometryNode> geometryNodesVec;
    
    for (auto node : model.linearNodes) {
        if (node->mesh) {
            for (auto primitive : node->mesh->primitives) {
                if (primitive->indexCount > 0) {
                    GeometryNode geometryNode{};
                    geometryNode.vertexBufferDeviceAddress = vks::util::getBufferDeviceAddress(device->logicalDevice, model.vertices.buffer);
                    geometryNode.indexBufferDeviceAddress = vks::util::getBufferDeviceAddress(device->logicalDevice, model.indices.buffer) + primitive->firstIndex * sizeof(uint32_t);
                    geometryNode.textureIndexBaseColor = primitive->material.baseColorTexture ? primitive->material.baseColorTexture->index : -1;
                    geometryNode.textureIndexNormal = primitive->material.normalTexture ? primitive->material.normalTexture->index : -1;
                    geometryNode.textureIndexMetallicRoughness = primitive->material.metallicRoughnessTexture ? primitive->material.metallicRoughnessTexture->index : -1;
                    geometryNode.textureIndexEmissive = primitive->material.emissiveTexture ? primitive->material.emissiveTexture->index : -1;
                    geometryNode.metallicFactor = primitive->material.metallicFactor;
                    geometryNode.roughnessFactor = primitive->material.roughnessFactor;
					geometryNode.baseColorFactor = primitive->material.baseColorFactor;
                    geometryNodesVec.push_back(geometryNode);
                }
            }
        }
    }

    vks::Buffer stagingBuffer;

    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer,
        static_cast<uint32_t>(geometryNodesVec.size()) * sizeof(GeometryNode),
        geometryNodesVec.data()));

    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &geometryNodes,
        static_cast<uint32_t>(geometryNodesVec.size()) * sizeof(GeometryNode)));

    device->copyBuffer(&stagingBuffer, &geometryNodes, queue);

    stagingBuffer.destroy();
}

void Renderer::init(vks::VulkanDevice& _device, VkQueue _queue, GPUTimer& _timer, vkglTF::Model& model) {

    this->device = &_device;
    this->timer = &_timer;
    this->queue = _queue;

    tracer.init(*device, *timer, queue);

    // Create interpolateColors pipeline
    VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsInterpolateColors) };
    ComputePass::PipelineContext pipelineContext;
    pipelineContext.shaderEntry.filePath = std::string(shaderPath) + "interpolateColors.comp.spv";
    pipelineContext.pushConstantRanges = { pushConstantRange };
    interpolateColorsPass.createPipeline(*device, pipelineContext);

    // Create countRayHits pipeline
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsCountRayHits) };
    pipelineContext.shaderEntry.filePath = std::string(shaderPath) + "countRayHits.comp.spv";
    pipelineContext.pushConstantRanges = { pushConstantRange };
    countRayHitsPass.createPipeline(*device, pipelineContext);

    // Create reconstructShadow pipeline
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsReconstructShadow) };
    pipelineContext.shaderEntry.filePath = std::string(shaderPath) + "reconstructShadow.comp.spv";
    pipelineContext.pushConstantRanges = { pushConstantRange };
    reconstructShadowPass.createPipeline(*device, pipelineContext);

    // Create initSeeds pipeline
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsInitSeeds) };
    pipelineContext.shaderEntry.filePath = std::string(shaderPath) + "initSeeds.comp.spv";
    pipelineContext.pushConstantRanges = { pushConstantRange };
    initSeedsPass.createPipeline(*device, pipelineContext);

    // Create raygenPrimary pipeline
    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsRaygenPrimary) };
    pipelineContext.shaderEntry.filePath = std::string(shaderPath) + "raygenPrimary.comp.spv";
    pipelineContext.pushConstantRanges = { pushConstantRange };
    raygenPrimaryPass.createPipeline(*device, pipelineContext);

    // Create reconstructSmooth pipeline
	createDescriptorSet(model);
	createGeometryNodeBuffer(model);

    pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsReconstructSmooth) };
    pipelineContext.shaderEntry.filePath = std::string(shaderPath) + "reconstructSmooth.comp.spv";
	pipelineContext.descriptorSetLayouts = { descriptorSetLayout };
    pipelineContext.pushConstantRanges = { pushConstantRange };
    reconstructSmoothPass.createPipeline(*device, pipelineContext);

	// Create counter buffers
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

    std::string _rayType;
    Environment::getInstance()->getStringValue("Renderer.rayType", _rayType);
    setRayType(stringToRayType(_rayType));

    float _keyValue;
    Environment::getInstance()->getFloatValue("Renderer.keyValue", _keyValue);
    setKeyValue(_keyValue);
    float _whitePoint;
    Environment::getInstance()->getFloatValue("Renderer.whitePoint", _whitePoint);
    setWhitePoint(_whitePoint);
    int _samplesPerPixel;
    Environment::getInstance()->getIntValue("Renderer.samplesPerPixel", _samplesPerPixel);
    setSamplesPerPixel(_samplesPerPixel);
    int _recursionDepth;
    Environment::getInstance()->getIntValue("Renderer.recursionDepth", _recursionDepth);
    setRecursionDepth(_recursionDepth);

	Environment::getInstance()->getStringValue("Application.mode", mode);

    Environment::getInstance()->getBoolValue("Renderer.russianRoulette", russianRoulette);

    Environment::getInstance()->getBoolValue("Renderer.sortShadowRays", sortShadowRays);
    Environment::getInstance()->getBoolValue("Renderer.reorderShadowRays", reorderShadowRays);
    Environment::getInstance()->getBoolValue("Renderer.sortPathRays", sortPathRays);
    Environment::getInstance()->getBoolValue("Renderer.reorderPathRays", reorderPathRays);

    Environment::getInstance()->getBoolValue("Benchmark.printBounceLogs", printBounceLogs);

    Environment::getInstance()->getBoolValue("Scene.headlight", headlight);
    Environment::getInstance()->getVectorValue("Scene.light", light);

    sceneMinPos = model.dimensions.min;
	sceneMaxPos = model.dimensions.max;
	float maxExtent = std::max({ sceneMaxPos.x - sceneMinPos.x, 
                                 sceneMaxPos.y - sceneMinPos.y, 
                                 sceneMaxPos.z - sceneMinPos.z });

    float lightScale;
    Environment::getInstance()->getFloatValue("Scene.lightScale", lightScale);
    setLightRadius(maxExtent * lightScale);
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

float Renderer::getLightRadius() {
    return lightRadius;
}
void Renderer::setLightRadius(float lightRadius) {
    if (lightRadius <= 0 || lightRadius > RENDERER_MAX_RADIUS) {
        std::cout << "WARN <Renderer> Light radius must be in range (0," << RENDERER_MAX_RADIUS << "].\n";
    }
    else {
        this->lightRadius = lightRadius;
        resetFrameIndex();
    }
}

int Renderer::getSamplesPerPixel(void) {
    return samplesPerPixel;
}

void Renderer::setSamplesPerPixel(int samplesPerPixel) {
    if (samplesPerPixel <= 0 || samplesPerPixel > RENDERER_MAX_SAMPLES) {
        std::cout << "WARN <Renderer> Number of primary samples must be in range (0," << RENDERER_MAX_SAMPLES << "].\n";
    }
    else {
        this->samplesPerPixel = samplesPerPixel;
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
    return russianRoulette;
}

void Renderer::setRussianRoulette(bool russianRoulette) {
	this->russianRoulette = russianRoulette;
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

bool Renderer::getReorderShadowRays(void) {
    return reorderShadowRays;
}

void Renderer::setReorderShadowRays(bool reorderShadowRays) {
	this->reorderShadowRays = reorderShadowRays;
}

bool Renderer::getReorderPathRays(void) {
	return reorderPathRays;
}

void Renderer::setReorderPathRays(bool reorderPathRays) {
	this->reorderPathRays = reorderPathRays;
}

bool Renderer::getPrintBounceLogs(void) {
	return printBounceLogs;
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

    if (mode == "benchmark" && printBounceLogs) {
        // Clear bounce counts.
        for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH + 1; ++i) {
            shadowBounceLogs[i].rayCount = 0;
            shadowBounceLogs[i].mortonCodesTime = 0.0f;
            shadowBounceLogs[i].sortTime = 0.0f;
            shadowBounceLogs[i].reorderTime = 0.0f;
            shadowBounceLogs[i].traceTime = 0.0f;
        }

        for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; ++i) {
            pathBounceLogs[i].rayCount = 0;
            pathBounceLogs[i].mortonCodesTime = 0.0f;
            pathBounceLogs[i].sortTime = 0.0f;
            pathBounceLogs[i].reorderTime = 0.0f;
            pathBounceLogs[i].traceTime = 0.0f;
        }
    }

    if (headlight)
        light = camera.position;

    // Init seeds.
    time += initSeeds(extent.x * extent.y, frameIndex);

    // For-each primary ray.
    for (pass = 0; pass < samplesPerPixel; ++pass) {
        if (rayType == PRIMARY_RAYS)
            time += renderPrimary(camera, extent, framePixels);
        else if (rayType == SHADOW_RAYS)
            time += renderShadow(camera, extent, framePixels);
        else if (rayType == PATH_RAYS)
            time += renderPath(camera, extent, framePixels);
    }

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

BounceLog* Renderer::getShadowBounceLogs() {
	return shadowBounceLogs;
}

BounceLog* Renderer::getPathBounceLogs() {
    return pathBounceLogs;
}