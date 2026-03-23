/**
 * \file	Renderer.h
 * \author	Daniel Meister
 * \date	2014/05/10
 * \brief	Renderer class header file.
 */

/**
 * Modified Source
 * \file	Renderer.h
 * \author	Junhyeok Jang
 * \date	2026/03/10
 * \brief	Modified for Vulkan
 */

#pragma once

#include "camera.hpp"
#include "../Ray/RayGen.h"
#include "../Tracer/Tracer.h"

#define RENDERER_MAX_KEY_VALUE 2.0f
#define RENDERER_MAX_WHITE_POINT 2.0f
#define RENDERER_MAX_RADIUS 100.0f
#define RENDERER_MAX_SAMPLES 1024
#define RENDERER_MAX_BATCH_SIZE (8 * 1024 * 1024)
#define RENDERER_MAX_RECURSION_DEPTH 8

#define SORT_LOG 1

class PathQueue {

private:

    RayBuffer rays[2];
    bool swapBuffers;

public:

    PathQueue(void);
    ~PathQueue(void);

    void swap(void);
    void init(vks::VulkanDevice& device, VkQueue queue, int size);

    RayBuffer & getInputRays(void);
    RayBuffer & getOutputRays(void);

};

class Renderer {

public:

    enum RayType {
        PRIMARY_RAYS,
        WHITTED_RAYS,
        SHADOW_RAYS,
        AO_RAYS,
        PATH_RAYS,
        PSEUDOCOLOR_RAYS,
        THERMAL_RAYS,
        MAX_RAYS
    };

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

    ComputePass countRayHitsPass;
    ComputePass interpolateColorsPass;
    ComputePass reconstructSmoothPass;
    ComputePass reconstructShadowPass;

    struct PushConstantsCountRayHits {
        uint64_t rayResultAddr;
        uint64_t rayHitAddr;
        int numberOfRays;
    };

    struct PushConstantsInterpolateColors {
        uint64_t framePixelAddr;
        uint64_t pixelAddr;
        int numberOfPixels;
        int frameIndex;
        float keyValue;
        float whitePoint;
    };

    struct PushConstantsReconstructSmooth {
        uint64_t rayAddr;
        uint64_t resultAddr;
        uint64_t idxToPixelAddr;
        uint64_t pixelAddr;
        uint64_t decreaseAddr;
        uint64_t geometryAddr;

        glm::vec3 light;
        int numberOfRays;
        glm::vec3 backgroundColor;
        int numberOfSamples;
    };

    struct PushConstantsReconstructShadow {
        uint64_t outputResultAddr;
        uint64_t indexToPixelAddr;
        uint64_t indexToSlotAddr;
        uint64_t inPixelAddr;
        uint64_t outPixelAddr;
        int batchBegin;
        int batchSize;
        int numberOfSamples;
        int replace;
    };

    struct Scene {
        vks::Buffer geometries; // Destroyed outside of renderer class
        glm::vec3 minPos{};
        glm::vec3 maxPos{};
        glm::vec3 backgroundColor{};
        glm::vec3 light{};
    } scene;

    RayGen raygen;
    Tracer tracer;

    RayType rayType;
    float keyValue;
    float whitePoint;
    float aoRadius;
    float shadowRadius;
    int numberOfPrimarySamples;
    int numberOfAOSamples;
    int numberOfShadowSamples;
    int recursionDepth;
    int nodeSizeThreshold;
    int thermalThreshold;
    int numberOfHits;

    bool sortShadowRays;
    bool sortAORays;
    bool sortPathRays;

    int avgRayCounts[RENDERER_MAX_RECURSION_DEPTH];
    float sortTimes[RENDERER_MAX_RECURSION_DEPTH];
    float traceSortTimes[RENDERER_MAX_RECURSION_DEPTH];
    float traceTimes[RENDERER_MAX_RECURSION_DEPTH];

    int pass;
    int bounce;
    int frameIndex;

    unsigned long long numberOfPrimaryRays = 0;
    unsigned long long numberOfShadowRays = 0;
    unsigned long long numberOfAORays = 0;
    unsigned long long numberOfPathRays = 0;

    float primaryTraceTime = 0.f;
    float shadowTraceTime = 0.f;
    float aoTraceTime = 0.f;
    float pathTraceTime = 0.f;

    PathQueue pathQueue;
    RayBuffer primaryRays;
    RayBuffer aoRays;
    RayBuffer shadowRays;
    vks::Buffer auxPixels;
    vks::Buffer decreases;
    vks::Buffer seeds;
    vks::Buffer counterDevice;
    vks::Buffer counterHost;

    RayType stringToRayType(const std::string & rayType);

    float computeRayHits(RayBuffer & rays);
    float initDecreases(int numberOfPixels);
    float interpolateColors(int numberOfPixels, vks::Buffer & pixels, vks::Buffer & framePixels);

    float primaryPass(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels);
    float shadowPass(RayBuffer & inRays, vks::Buffer & inPixels, vks::Buffer & outPixels, bool replace);
    //float aoPass(Scene & scene, RayBuffer & inRays, Buffer & inPixels, Buffer & outPixels, bool replace);
    float pathPass(vks::Buffer & pixels, RayBuffer & inRays, RayBuffer & outRays);

    float renderPrimary(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels);
    float renderShadow(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels);
    //float renderAO(Scene & scene, Camera & camera, Buffer & pixels);
    float renderPath(Camera& camera, glm::ivec2 extent, vks:: Buffer& pixels);
    //float renderPseudocolor(Scene & scene, Camera & camera, Buffer & pixels);
    //float renderThermal(Camera & camera, Buffer & pixels);

    float reconstructSmooth(RayBuffer& rays, vks::Buffer& pixels);
    //float reconstructPseudocolor(Scene & scene, Buffer & pixels);
    //float reconstructThermal(Buffer & pixels);
    float reconstructShadow(RayBuffer & inRays, vks::Buffer & inPixels, vks::Buffer & outPixels, int batchBegin, int batchEnd, bool replace);
    //float reconstructAO(RayBuffer & inRays, Buffer & inPixels, Buffer & outPixels, int batchBegin, int batchEnd, bool replace);

    float tracePrimaryRays(Camera & camera, glm::ivec2& extent);
    float traceShadowRays(RayBuffer & inRays, int batchBegin, int batchEnd);
    //float traceAORays(Scene & scene, RayBuffer & inRays, int batchBegin, int batchEnd);
    float tracePathRays(RayBuffer & inRays, RayBuffer & outRays);

public:

    Renderer(void);
    ~Renderer(void);

    void init(vks::VulkanDevice& device, VkQueue queue, GPUTimer& timer, VkAccelerationStructureKHR topLevelAS);

    RayType getRayType(void);
    void setRayType(RayType rayType);
    void setKeyValue(float keyValue);
    float getKeyValue(void);
    void setWhitePoint(float whitePoint);
    float getWhitePoint(void);
    float getAORadius(void);
    void setAORadius(float aoRadius);
    float getShadowRadius(void);
    void setShadowRadius(float shadowRadius);
    int getNumberOfPrimarySamples(void);
    void setNumberOfPrimarySamples(int numberOfPrimarySamples);
    int getNumberOfAOSamples(void);
    void setNumberOfAOSamples(int numberOfAOSamples);
    int getNumberOfShadowSamples(void);
    void setNumberOfShadowSamples(int numberOfShadowSamples);
    int getRecursionDepth(void);
    void setRecursionDepth(int recursionDepth);
    int getNodeSizeThreshold(void);
    void setNodeSizeThreshold(int nodeSizeThreshold);
    int getThermalThreshold(void);
    void setThermalThreshold(int thermalThreshold);
    bool getRussianRoulette(void);
    void setRussianRoulette(bool russianRoulette);

    bool getSortShadowRays(void);
    void setSortShadowRays(bool sortShadowRays);
    bool getSortAORays(void);
    void setSortAORays(bool sortAORays);
    bool getSortPathRays(void);
    void setSortPathRays(bool sortPathRays);

    float getShadowRayLength(void);
    void setShadowRayLength(float shadowRayLength);
    float getAORayLength(void);
    void setAORayLength(float aoRayLength);
    float getPathRayLength(void);
    void setPathRayLength(float pathRayLength);

    void setScene(vks::Buffer& geometries, glm::vec3& sceneMinPos, glm::vec3& sceneMaxPos, glm::vec3& light, glm::vec3& backgroundColor, VkAccelerationStructureKHR topLevelAS);
    void setSceneBounds(glm::vec3& sceneMinPos, glm::vec3& sceneMaxPos);
    void setLight(glm::vec3& light);
    void setBackgroundColor(glm::vec3& backgroundColor);
    void setGeometries(vks::Buffer& geometries);
    void setAccelerationStructure(VkAccelerationStructureKHR topLevelAS);

    float render(Camera & camera, glm::ivec2 extent, vks::Buffer & pixels, vks::Buffer & framePixels);

    void resetFrameIndex(void);

    unsigned long long getNumberOfPrimaryRays(void);
    unsigned long long getNumberOfShadowRays(void);
    unsigned long long getNumberOfAORays(void);
    unsigned long long getNumberOfPathRays(void);
    unsigned long long getNumberOfRays(void);

    float getPrimaryTraceTime(void);
    float getShadowTraceTime(void);
    float getAOTraceTime(void);
    float getPathTraceTime(void);
    float getTraceTime(void);

    float getPrimaryTracePerformance(void);
    float getShadowTracePerformance(void);
    float getAOTracePerformance(void);
    float getPathTracePerformance(void);
    float getTracePerformance(void);

};

