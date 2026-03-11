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
    bool russianRoulette;

    vks::VulkanDevice* device;
    VkQueue queue;
    GPUTimer* timer;

    ComputePass initSeedsPass;
    ComputePass primaryPass;

    struct PushConstantsInitSeeds {
        uint64_t seedAddr;
        int numberOfPixels;
        int frameIndex;
    } pcInitSeeds;

    struct PushConstantsPrimary {
        uint64_t indexToPixelAddr;
        uint64_t rayBufferAddr;
        glm::mat4 screenToWorld;
        glm::vec3 origin;
        int sampleIndex;
        glm::ivec2 size;
        float maxDist;
    } pcPrimary;

public:

    RayGen(void) = default;
    ~RayGen(void) {
        seeds.destroy();
    }

    void init(vks::VulkanDevice& device, GPUTimer& timer, VkQueue queue);

    float initSeeds(int numberOfPixels, int frameIndex = 1);

    float primary(RayBuffer & orays, Camera & camera, glm::ivec2 & extent, int sampleIndex);
    //float shadow(RayBuffer & orays, RayBuffer & irays, int batchBegin, int batchEnd, int numberOfSamples, const Vec3f & light, float lightRadius);
    //float ao(RayBuffer & orays, RayBuffer & irays, Scene & scene, int batchBegin, int batchEnd, int numberOfSamples, float maxDist);
    //float path(RayBuffer & orays, RayBuffer & irays, Buffer & decreases, Scene & scene);

    bool getRussianRoulette(void);
    void setRussianRoulette(bool russianRoulette);

};
