/**
  * Original Source
  * \file	RayGen.h
  * \author	Daniel Meister
  * \date	2014/05/10
  * \brief	RayGen class header file.
  */

/**
  * Modified Source 
  * \file	RayGen.h
  * \author	Junhyeok Jang
  * \date	2026/03/10
  * \brief	Modified for Vulkan
  */

#pragma once

#include "RayBuffer.h"
#include "PixelTable.h"
#include "camera.hpp"
#include "../Compute/ComputePass.h"
#include "../Utils/gpuTimer.h"


class RayGen {

private:
    static constexpr uint32_t workGroupSize = 256;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    static constexpr std::string_view shaderPath = "shaders/glsl/WaveFrontPathTracer/";
#else
    static constexpr std::string_view shaderPath = "./../shaders/glsl/WaveFrontPathTracer/";
#endif

    PixelTable pixelTable;
    vks::Buffer seeds;
    vks::Buffer counterDevice;
    vks::Buffer counterHost;
    bool russianRoulette;

    vks::VulkanDevice* device;
    VkQueue queue;
    GPUTimer* timer;

    ComputePass initSeedsPass;
    ComputePass primaryPass;
    ComputePass shadowPass;
    ComputePass pathPass;

    struct PushConstantsInitSeeds {
        uint64_t seedAddr;
        int numberOfPixels;
        int frameIndex;
    };

    struct PushConstantsPrimary {
        glm::mat4 screenToWorld;
        uint64_t indexToPixelAddr;
        uint64_t rayBufferAddr;
        glm::vec3 origin;
        int sampleIndex;
        glm::ivec2 size;
        float maxDist;
    };

    struct PushConstantsShadow{
        uint64_t seedAddr;
        uint64_t inputRayAddr;
        uint64_t outputRayAddr;
        uint64_t inputResultAddr;
        uint64_t outputSlotToIndexAddr;
        uint64_t outputIndexToSlotAddr;
        glm::vec3 light;
        float lightRadius;
        int batchBegin;
        int batchSize;
        int numberOfSamples;
    };

    struct PushConstantsPath {
        uint64_t seedAddr;
        uint64_t geometryAddr;
        uint64_t inputRayAddr;
        uint64_t outputRayAddr;
        uint64_t inputIndexToPixelAddr;
        uint64_t outputIndexToPixelAddr;
        uint64_t inputResultAddr;
        uint64_t decreaseAddr;
        uint64_t numberOfOutputRayAddr;
        int numberOfInputRays;
        int russianRoulette;
    };

public:

    RayGen(void) = default;
    ~RayGen(void) {
        seeds.destroy();
        counterDevice.destroy();
        counterHost.destroy();
    }

    void init(vks::VulkanDevice& device, GPUTimer& timer, VkQueue queue);

    float initSeeds(int numberOfPixels, int frameIndex = 1);

    float primary(RayBuffer & orays, Camera & camera, glm::ivec2 & extent, int sampleIndex);
    float shadow(RayBuffer & orays, RayBuffer & irays, int batchBegin, int batchEnd, int numberOfSamples, const glm::vec3 & light, float lightRadius);
    //float ao(RayBuffer & orays, RayBuffer & irays, Scene & scene, int batchBegin, int batchEnd, int numberOfSamples, float maxDist);
    float path(RayBuffer & orays, RayBuffer & irays, vks::Buffer & decreases, vks::Buffer & geometries);

    bool getRussianRoulette(void);
    void setRussianRoulette(bool russianRoulette);

};
