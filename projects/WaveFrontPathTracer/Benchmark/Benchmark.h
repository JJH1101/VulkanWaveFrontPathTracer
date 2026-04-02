/**
 * \file	Benchmark.h
 * \author	Junhyeok Jang
 * \date	2026/03/25
 * \brief	Benchmark class header file.
 */

#pragma once
#include "../Renderer/Renderer.h"

class Benchmark {

private:
    static constexpr uint32_t warmupCycle = 10;
#ifdef __ANDROID__
    static constexpr uint32_t benchmarkCycle = 100;
#else
    static constexpr uint32_t benchmarkCycle = 500;
#endif

    Renderer* renderer;

    float bvhKernelsTime;
    float bvhAbsoluteTime;
    float renderKernelsTime;
    float renderAbsoluteTime;
    float rtPerformance;
    float rtPerformancePrimary;
    float rtPerformanceShadow;
    float rtPerformancePath;
    float traceTime;
    float traceTimePrimary;
    float traceTimeShadow;
    float traceTimePath;
    uint64_t numberOfRays;
    uint64_t numberOfPrimaryRays;
    uint64_t numberOfShadowRays;
    uint64_t numberOfPathRays;

	SortLog shadowSortLogs[RENDERER_MAX_RECURSION_DEPTH + 1];
	SortLog pathSortLogs[RENDERER_MAX_RECURSION_DEPTH];

    uint32_t frameCount;

    void reset(void);
    std::string rayTypeToString(Renderer::RayType rayType);

public:

    Benchmark(Renderer* renderer);
    ~Benchmark(void);

    float run(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels);

};

