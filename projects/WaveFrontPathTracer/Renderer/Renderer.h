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
#include "VulkanglTFModel.h"

#include "../Ray/RayBuffer.h"
#include "../Ray/PixelTable.h"
#include "../Compute/ComputePass.h"
#include "../Utils/gpuTimer.h"
#include "../Tracer/Tracer.h"

#define RENDERER_MAX_KEY_VALUE 2.0f
#define RENDERER_MAX_WHITE_POINT 2.0f
#define RENDERER_MAX_RADIUS 100.0f
#define RENDERER_MAX_SAMPLES 1024
#define RENDERER_MAX_RECURSION_DEPTH 8

struct SortLog {
    int rayCount = 0;
    float mortonCodesTime = 0.0f;
    float sortTime = 0.0f;
    float reorderTime = 0.0f;
    float traceSortTime = 0.0f;

    SortLog& operator+=(const SortLog& rhs) {
        rayCount += rhs.rayCount;
        mortonCodesTime += rhs.mortonCodesTime;
        sortTime += rhs.sortTime;
        reorderTime += rhs.reorderTime;
		traceSortTime += rhs.traceSortTime;
        return *this;
    }
};

class PathQueue {

private:

    RayBuffer rays[2];
    bool swapBuffers;

public:

    PathQueue(void);
    ~PathQueue(void);

    void swap(void);
    void init(vks::VulkanDevice& device, int size);

    RayBuffer & getInputRays(void);
    RayBuffer & getOutputRays(void);

};

class Renderer {

public:

    enum RayType {
        PRIMARY_RAYS,
        SHADOW_RAYS,
        PATH_RAYS,
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

    ComputePass initSeedsPass;
    ComputePass raygenPrimaryPass;
    ComputePass countRayHitsPass;
    ComputePass interpolateColorsPass;
    ComputePass reconstructSmoothPass;
    ComputePass reconstructShadowPass;

    struct PushConstantsInitSeeds {
        uint64_t seedAddr;
        int numberOfPixels;
        int frameIndex;
    };

    struct PushConstantsRaygenPrimary {
        glm::mat4 screenToWorld;
        uint64_t indexToPixelAddr;
        uint64_t rayBufferAddr;
        glm::vec3 origin;
        int sampleIndex;
        glm::ivec2 size;
        float maxDist;
    };

    struct PushConstantsCountRayHits {
        uint64_t rayResultAddr;
        uint64_t rayHitAddr;
        int numberOfRays;
    };

    struct PushConstantsInterpolateColors {
        uint64_t framePixelAddr;
        uint64_t pixelAddr;
        int numberOfPixels;
        int numberOfSamples;
        int frameIndex;
        float keyValue;
        float whitePoint;
    };

    struct PushConstantsReconstructSmooth {
        uint64_t inputRayAddr;
        uint64_t inputResultAddr;
        uint64_t inputIdxToPixelAddr;

        uint64_t pixelAddr;
        uint64_t decreaseAddr;
        uint64_t seedAddr;
        uint64_t geometryNodeAddr;

        uint64_t shadowRayAddr;
        uint64_t shadowIdxToPixelAddr;

        uint64_t pathRayAddr;
        uint64_t pathIdxToPixelAddr;

        uint64_t rayCounterAddr;

        glm::vec3 light;
        float lightRadius;
        
        uint32_t russianRoulette;
        uint32_t numberOfRays;
    };

    struct PushConstantsReconstructShadow {
        uint64_t outputResultAddr;
        uint64_t indexToPixelAddr;
        uint64_t inPixelAddr;
        uint64_t outPixelAddr;
        uint32_t numberOfRays;
        uint32_t replace;
    };

    VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

    struct GeometryNode {
        glm::vec4 baseColorFactor;
        uint64_t vertexBufferDeviceAddress;
        uint64_t indexBufferDeviceAddress;
        int32_t textureIndexBaseColor;
        int32_t textureIndexNormal;
        int32_t textureIndexMetallicRoughness;
        int32_t textureIndexEmissive;
        float metallicFactor;
        float roughnessFactor;
        float _padding0;
		float _padding1;
    };
    vks::Buffer geometryNodes;

    glm::vec3 light;
	float lightRadius;

	glm::vec3 sceneMinPos;
	glm::vec3 sceneMaxPos;

    PixelTable pixelTable;
    Tracer tracer;

    RayType rayType;
    float keyValue;
    float whitePoint;
    int samplesPerPixel;
    int recursionDepth;
    int numberOfHits;

    std::string mode{};

	bool russianRoulette = false;

    bool sortShadowRays = false;
	bool reorderShadowRays = false;
    bool sortPathRays = false;
	bool reorderPathRays = false;

    bool printSortLogs = false;

    bool headlight = false;

	SortLog shadowSortLogs[RENDERER_MAX_RECURSION_DEPTH + 1];
	SortLog pathSortLogs[RENDERER_MAX_RECURSION_DEPTH];

    int pass;
    int bounce;
    int frameIndex;

    unsigned long long numberOfPrimaryRays = 0;
    unsigned long long numberOfShadowRays = 0;
    unsigned long long numberOfPathRays = 0;

    float primaryTraceTime = 0.f;
    float shadowTraceTime = 0.f;
    float pathTraceTime = 0.f;

    PathQueue pathQueue;
    RayBuffer primaryRays;
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
    float initSeeds(int numberOfPixels, int frameIndex = 1);

    float raygenPrimary(Camera& camera, glm::ivec2& extent, int sampleIndex);

    float renderPrimary(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels);
    float renderShadow(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels);
    float renderPath(Camera& camera, glm::ivec2 extent, vks:: Buffer& pixels);

    float reconstructSmooth(RayBuffer& inRays, vks::Buffer& pixels, bool genShadow = true);
    float reconstructSmooth(RayBuffer& inRays, RayBuffer& outRays, vks::Buffer& pixels);
    float reconstructShadow(vks::Buffer & inPixels, vks::Buffer & outPixels, bool replace = false);

    void createDescriptorSet(vkglTF::Model& model);
	void createGeometryNodeBuffer(vkglTF::Model& model);

public:

    Renderer(void);
    ~Renderer(void);

    void init(vks::VulkanDevice& device, VkQueue queue, GPUTimer& timer, vkglTF::Model& model);

    RayType getRayType(void);
    void setRayType(RayType rayType);
    void setKeyValue(float keyValue);
    float getKeyValue(void);
    void setWhitePoint(float whitePoint);
    float getWhitePoint(void);
    float getLightRadius(void);
    void setLightRadius(float lightRadius);
    int getSamplesPerPixel(void);
    void setSamplesPerPixel(int samplesPerPixel);
    int getRecursionDepth(void);
    void setRecursionDepth(int recursionDepth);
    bool getRussianRoulette(void);
    void setRussianRoulette(bool russianRoulette);

    bool getSortShadowRays(void);
    void setSortShadowRays(bool sortShadowRays);
    bool getSortPathRays(void);
    void setSortPathRays(bool sortPathRays);

    bool getReorderShadowRays(void);
    void setReorderShadowRays(bool reorderShadowRays);
    bool getReorderPathRays(void);
    void setReorderPathRays(bool reorderPathRays);

	bool getPrintSortLogs(void);

    void setAccelerationStructure(VkAccelerationStructureKHR topLevelAS);

    float render(Camera & camera, glm::ivec2 extent, vks::Buffer & pixels, vks::Buffer & framePixels);

    void resetFrameIndex(void);

    unsigned long long getNumberOfPrimaryRays(void);
    unsigned long long getNumberOfShadowRays(void);
    unsigned long long getNumberOfPathRays(void);
    unsigned long long getNumberOfRays(void);

    float getPrimaryTraceTime(void);
    float getShadowTraceTime(void);
    float getPathTraceTime(void);
    float getTraceTime(void);

    float getPrimaryTracePerformance(void);
    float getShadowTracePerformance(void);
    float getPathTracePerformance(void);
    float getTracePerformance(void);

    SortLog* getShadowSortLogs(void);
    SortLog* getPathSortLogs(void);

};

