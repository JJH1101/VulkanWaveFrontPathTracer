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
    int warmupCyclesPerView = 10;
	int benchmarkCyclesPerView = 100;

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

	BounceLog shadowBounceLogs[RENDERER_MAX_RECURSION_DEPTH + 1];
    BounceLog pathBounceLogs[RENDERER_MAX_RECURSION_DEPTH];

    int frameCount;
    int view;

    struct viewInfos {
		std::vector<glm::vec3> cameraPositions;
		std::vector<glm::vec3> cameraRotations;
		std::vector<glm::vec3> lightPositions;
		int numberOfViews;
    } views;

    void reset(void);
    std::string rayTypeToString(Renderer::RayType rayType);

public:

    Benchmark(Renderer* renderer);
    ~Benchmark(void);

    float run(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels);

};

