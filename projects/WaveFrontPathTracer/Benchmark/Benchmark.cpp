/**
 * \file	Benchmark.cpp
 * \author	Junhyeok Jang
 * \date	2026/03/25
 * \brief	Benchmark class source file.
 */

#include "Benchmark.h"
#include "../Environment/Environment.h"
#include <chrono>


Benchmark::Benchmark(Renderer* _renderer) : renderer(_renderer) {
	reset();
}

Benchmark::~Benchmark() {

}

std::string Benchmark::rayTypeToString(Renderer::RayType rayType) {
    if (rayType == Renderer::PATH_RAYS)
        return "PATH";
    else if (rayType == Renderer::SHADOW_RAYS)
        return "SHADOW";
    else
        return "PRIMARY";
}

void Benchmark::reset() {

   bvhKernelsTime = 0.0f;
   bvhAbsoluteTime = 0.0f;
   renderKernelsTime = 0.0f;
   renderAbsoluteTime = 0.0f;
   rtPerformance = 0.0f;
   rtPerformancePrimary = 0.0f;
   rtPerformanceShadow = 0.0f;
   rtPerformancePath = 0.0f;
   traceTime = 0.0f;
   traceTimePrimary = 0.0f;
   traceTimeShadow = 0.0f;
   traceTimePath = 0.0f;
   numberOfRays = 0;
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
#endif

   frameCount = 0;

}

float Benchmark::run(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels) {

    float renderKernelsTimeCur;
    float renderAbsoluteTimeCur;

#if SORT_LOG
    int shadowRayCountsCur[RENDERER_MAX_RECURSION_DEPTH + 1];
    float shadowMortoncodesTimesCur[RENDERER_MAX_RECURSION_DEPTH + 1];
    float shadowSortTimesCur[RENDERER_MAX_RECURSION_DEPTH + 1];
    float shadowTraceSortTimesCur[RENDERER_MAX_RECURSION_DEPTH + 1];
    float shadowTraceTimesCur[RENDERER_MAX_RECURSION_DEPTH + 1];

    int pathRayCountsCur[RENDERER_MAX_RECURSION_DEPTH];
    float pathMortoncodesTimesCur[RENDERER_MAX_RECURSION_DEPTH];
    float pathSortTimesCur[RENDERER_MAX_RECURSION_DEPTH];
    float pathTraceSortTimesCur[RENDERER_MAX_RECURSION_DEPTH];
    float pathTraceTimesCur[RENDERER_MAX_RECURSION_DEPTH];
#endif

	auto start = std::chrono::high_resolution_clock::now();
	renderKernelsTimeCur = renderer->render(camera, extent, pixels, framePixels);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> duration = end - start;
	renderAbsoluteTimeCur = duration.count();

    int recursionDepth;
    Environment::getInstance()->getIntValue("Renderer.recursionDepth", recursionDepth);

	if (frameCount >= warmupCycle) {
        renderKernelsTime += renderKernelsTimeCur;
        renderAbsoluteTime += renderAbsoluteTimeCur;
		rtPerformance += renderer->getTracePerformance();
		rtPerformancePrimary += renderer->getPrimaryTracePerformance();
		rtPerformanceShadow +=  renderer->getShadowTracePerformance();
		rtPerformancePath += renderer->getPathTracePerformance();
        traceTime += renderer->getTraceTime();
        traceTimePrimary += renderer->getPrimaryTraceTime();
        traceTimeShadow += renderer->getShadowTraceTime();
        traceTimePath += renderer->getPathTraceTime();
		numberOfRays += renderer->getNumberOfRays();
		numberOfPrimaryRays += renderer->getNumberOfPrimaryRays();
		numberOfShadowRays += renderer->getNumberOfShadowRays();
		numberOfPathRays += renderer->getNumberOfPathRays();

#if SORT_LOG
        renderer->getShadowSortLog(shadowRayCountsCur, shadowMortoncodesTimesCur, shadowSortTimesCur, shadowTraceSortTimesCur, shadowTraceTimesCur);
        renderer->getPathSortLog(pathRayCountsCur, pathMortoncodesTimesCur, pathSortTimesCur, pathTraceSortTimesCur, pathTraceTimesCur);

        for (int i = 0; i < recursionDepth + 1; ++i) {
            shadowRayCounts[i] += shadowRayCountsCur[i];
            shadowMortoncodesTimes[i] += shadowMortoncodesTimesCur[i];
            shadowSortTimes[i] += shadowSortTimesCur[i];
            shadowTraceSortTimes[i] += shadowTraceSortTimesCur[i];
            shadowTraceTimes[i] += shadowTraceTimesCur[i];
        }

        for (int i = 0; i < recursionDepth; ++i) {
            pathRayCounts[i] += pathRayCountsCur[i];
            pathMortoncodesTimes[i] += pathMortoncodesTimesCur[i];
            pathSortTimes[i] += pathSortTimesCur[i];
            pathTraceSortTimes[i] += pathTraceSortTimesCur[i];
            pathTraceTimes[i] += pathTraceTimesCur[i];
        }

#endif
	}

	if (frameCount == warmupCycle + benchmarkCycle - 1) {
        // Write results.
        std::cout << "RAY TYPE: ";
        std::cout << rayTypeToString(renderer->getRayType()) << "\n\n";

        std::cout << "NUMBER OF RAYS: ";
        std::cout << numberOfRays / benchmarkCycle << "\n";
        std::cout << "NUMBER OF PRIMARY RAYS: ";
        std::cout << numberOfPrimaryRays / benchmarkCycle << "\n";
        std::cout << "NUMBER OF SHADOW RAYS: ";
        std::cout << numberOfShadowRays / benchmarkCycle << "\n";
        std::cout << "NUMBER OF PATH RAYS: ";
        std::cout << numberOfPathRays / benchmarkCycle << "\n";
        std::cout << "TRACE TIME: ";
        std::cout << traceTime / benchmarkCycle << "\n";
        std::cout << "TRACE TIME PRIMARY: ";
        std::cout << traceTimePrimary / benchmarkCycle << "\n";
        std::cout << "TRACE TIME SHADOW: ";
        std::cout << traceTimeShadow / benchmarkCycle << "\n";
        std::cout << "TRACE TIME PATH: ";
        std::cout << traceTimePath / benchmarkCycle << "\n";
        std::cout << "RT PERFORMANCE: ";
        std::cout << rtPerformance / benchmarkCycle << "\n";
        std::cout << "RT PERFORMANCE PRIMARY: ";
        std::cout << rtPerformancePrimary / benchmarkCycle << "\n";
        std::cout << "RT PERFORMANCE SHADOW: ";
        std::cout << rtPerformanceShadow / benchmarkCycle << "\n";
        std::cout << "RT PERFORMANCE PATH: ";
        std::cout << rtPerformancePath / benchmarkCycle << "\n";
        std::cout << "RENDER TIME KERNELS: ";
        std::cout << renderKernelsTime / benchmarkCycle << "\n";
        std::cout << "RENDER TIME ABSOLUTE: ";
        std::cout << renderAbsoluteTime / benchmarkCycle << "\n\n";

#if SORT_LOG
        uint64_t rayCountsTotal = 0;
        float mortoncodesTimesTotal = 0.0f;
        float sortTimesTotal = 0.0f;
        float traceSortTimesTotal = 0.0f;
        float traceTimesTotal = 0.0f;

        for (int i = 0; i < recursionDepth + 1; i++) {
            std::cout << "SORT LOG SHADOW AT DEPTH " << i << "\n";
            std::cout << "RAY COUNTS: ";
            std::cout << shadowRayCounts[i] / benchmarkCycle << "\n";
            rayCountsTotal += shadowRayCounts[i];
            std::cout << "MORTONCODES TIME: ";
            std::cout << shadowMortoncodesTimes[i] / benchmarkCycle << "\n";
            mortoncodesTimesTotal += shadowMortoncodesTimes[i];
            std::cout << "SORT TIME: ";
            std::cout << shadowSortTimes[i] / benchmarkCycle << "\n";
            sortTimesTotal += shadowSortTimes[i];
            std::cout << "TRACE SORT TIME: ";
            std::cout << shadowTraceSortTimes[i] / benchmarkCycle << "\n";
            traceSortTimesTotal += shadowTraceSortTimes[i];
            std::cout << "TRACE TIME: ";
            std::cout << shadowTraceTimes[i] / benchmarkCycle << "\n\n";
            traceTimesTotal += shadowTraceTimes[i];
        }

        std::cout << "SORT LOG SHADOW TOTAL\n";
        std::cout << "RAY COUNTS: ";
        std::cout << rayCountsTotal / benchmarkCycle << "\n";
        std::cout << "MORTONCODES TIME: ";
        std::cout << mortoncodesTimesTotal / benchmarkCycle << "\n";
        std::cout << "SORT TIME: ";
        std::cout << sortTimesTotal / benchmarkCycle << "\n";
        std::cout << "TRACE SORT TIME: ";
        std::cout << traceSortTimesTotal / benchmarkCycle << "\n";
        std::cout << "TRACE TIME: ";
        std::cout << traceTimesTotal / benchmarkCycle << "\n\n";

        rayCountsTotal = 0;
        mortoncodesTimesTotal = 0.0f;
        sortTimesTotal = 0.0f;
        traceSortTimesTotal = 0.0f;
        traceTimesTotal = 0.0f;

        for (int i = 0; i < recursionDepth; i++) {
            std::cout << "SORT LOG PATH AT DEPTH " << i + 1 << "\n";
            std::cout << "RAY COUNTS: ";
            std::cout << pathRayCounts[i] / benchmarkCycle << "\n";
            rayCountsTotal += pathRayCounts[i];
            std::cout << "MORTONCODES TIME: ";
            std::cout << pathMortoncodesTimes[i] / benchmarkCycle << "\n";
            mortoncodesTimesTotal += pathMortoncodesTimes[i];
            std::cout << "SORT TIME: ";
            std::cout << pathSortTimes[i] / benchmarkCycle << "\n";
            sortTimesTotal += pathSortTimes[i];
            std::cout << "TRACE SORT TIME: ";
            std::cout << pathTraceSortTimes[i] / benchmarkCycle << "\n";
            traceSortTimesTotal += pathTraceSortTimes[i];
            std::cout << "TRACE TIME: ";
            std::cout << pathTraceTimes[i] / benchmarkCycle << "\n\n";
            traceTimesTotal += pathTraceTimes[i];
        }

        std::cout << "SORT LOG PATH TOTAL\n";
        std::cout << "RAY COUNTS: ";
        std::cout << rayCountsTotal / benchmarkCycle << "\n";
        std::cout << "MORTONCODES TIME: ";
        std::cout << mortoncodesTimesTotal / benchmarkCycle << "\n";
        std::cout << "SORT TIME: ";
        std::cout << sortTimesTotal / benchmarkCycle << "\n";
        std::cout << "TRACE SORT TIME: ";
        std::cout << traceSortTimesTotal / benchmarkCycle << "\n";
        std::cout << "TRACE TIME: ";
        std::cout << traceTimesTotal / benchmarkCycle << "\n\n";
#endif

        system("pause");
        exit(0);
	}

	frameCount++;

    return renderKernelsTimeCur;
}