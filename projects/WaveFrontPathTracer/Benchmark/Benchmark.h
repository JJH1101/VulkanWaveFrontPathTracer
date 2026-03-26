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
    static constexpr uint32_t benchmarkCycle = 500;

    Renderer* renderer;

    float bvhKernelsTime;
    float bvhAbsoluteTime;
    float renderKernelsTime;
    float renderAbsoluteTime;
    float rtPerformance;
    float rtPerformancePrimary;
    float rtPerformanceShadow;
    float rtPerformancePath;
    uint64_t numberOfRays;
    uint64_t numberOfPrimaryRays;
    uint64_t numberOfShadowRays;
    uint64_t numberOfPathRays;

    uint32_t frameCount;

    void reset(void);
    std::string rayTypeToString(Renderer::RayType rayType);

public:

    Benchmark(Renderer* renderer);
    ~Benchmark(void);

    float run(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels);

};

