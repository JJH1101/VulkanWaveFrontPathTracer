#pragma once

#include "../Compute/ComputePass.h"
#include "../Ray/RayBuffer.h"
#include "glm/glm.hpp"
#include "vulkan_radix_sort/vk_radix_sort.h"

class Tracer {

private:

    static constexpr uint32_t workGroupSize = 256;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    static constexpr std::string_view shaderPath = "shaders/glsl/WaveFrontPathTracer/";
#else
    static constexpr std::string_view shaderPath = "./../shaders/glsl/WaveFrontPathTracer/";
#endif

    vks::VulkanDevice* device = nullptr;
    VkQueue queue{ VK_NULL_HANDLE };
    GPUTimer* timer = nullptr;

    ComputePass tracePass;
    ComputePass computeMortonCodesPass;
    ComputePass reorderRaysPass;

    VrdxSorter sorter{ VK_NULL_HANDLE };

    struct PushConstantsTrace {
        uint64_t rayBufferAddr;
        uint64_t resultBufferAddr;
        uint64_t rayIndexBufferAddr;
        uint32_t numRays;
        uint32_t isClosestHit;
    };

    struct PushConstantsMortonCode {
        alignas(8) uint64_t rayAddr;
        alignas(8) uint64_t mortonCodeAddr;
        alignas(8) uint64_t rayIndexAddr;
        alignas(4) uint32_t numberOfRays;
        alignas(16) glm::vec3 sceneMin;
        alignas(16) glm::vec3 sceneSize;
    };

    struct PushConstantsReorderRays {
        uint64_t rayIndexAddr;
        uint64_t inRayAddr;
        uint64_t outRayAddr;
        uint64_t inSlotToIndexAddr;
        uint64_t outSlotToIndexAddr;
        uint64_t outIndexToSlotAddr;
        int numberOfRays;
    };

    VkDescriptorPool descriptorPoolTrace{ VK_NULL_HANDLE };
    VkDescriptorSetLayout descriptorSetLayoutTrace{ VK_NULL_HANDLE };
    VkDescriptorSet descriptorSetTrace{ VK_NULL_HANDLE };

    float computeMortonCodes(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos);
    float radixSort(vks::Buffer& keyBuffer, vks::Buffer& valueBuffer, vks::Buffer& storageBuffer, int size);
    float reorderRays(RayBuffer& rays);

    float trace(RayBuffer& rays, bool indirectIndexing);

public:

    Tracer(void);
    ~Tracer(void);

    void init(vks::VulkanDevice& device, GPUTimer& timer, VkQueue queue);
    void setAccererationStructure(VkAccelerationStructureKHR topLevelAS);

    float trace(RayBuffer& rays);
    float traceSort(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos, bool reorderRays);
    float traceSort(RayBuffer& rays, glm::vec3 sceneMinPos, glm::vec3 sceneMaxPos, bool reorderRays, std::array<float, 4>& times);
};

