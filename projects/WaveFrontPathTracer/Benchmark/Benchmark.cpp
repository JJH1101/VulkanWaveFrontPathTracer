/**
 * \file	Benchmark.cpp
 * \author	Junhyeok Jang
 * \date	2026/03/25
 * \brief	Benchmark class source file.
 */

#include "Benchmark.h"
#include "../Environment/Environment.h"
#include <chrono>

template <typename... Args>
inline void logInfo(std::string_view fmt, Args&&... args) {
    std::string message = std::vformat(fmt, std::make_format_args(args...));

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "WaveFrontPT", "%s", message.c_str());
#else
    std::cout << "[INFO] " << message << std::endl;
#endif
}

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
        logInfo("==================================================");
        logInfo(" RAY TRACING BENCHMARK RESULTS ");
        logInfo("--------------------------------------------------");
        logInfo(" RAY TYPE                : {}", rayTypeToString(renderer->getRayType()));
        logInfo("");
        logInfo(" [Count (Average)]");
        logInfo(" TOTAL RAYS              : {}", numberOfRays / benchmarkCycle);
        logInfo(" PRIMARY RAYS            : {}", numberOfPrimaryRays / benchmarkCycle);
        logInfo(" SHADOW RAYS             : {}", numberOfShadowRays / benchmarkCycle);
        logInfo(" PATH RAYS               : {}", numberOfPathRays / benchmarkCycle);
        logInfo("");
        logInfo(" [Time (ms)]");
        logInfo(" TRACE TOTAL             : {:.4f} ms", traceTime / benchmarkCycle);
        logInfo(" TRACE PRIMARY           : {:.4f} ms", traceTimePrimary / benchmarkCycle);
        logInfo(" TRACE SHADOW            : {:.4f} ms", traceTimeShadow / benchmarkCycle);
        logInfo(" TRACE PATH              : {:.4f} ms", traceTimePath / benchmarkCycle);
        logInfo("");
        logInfo(" [Performance (Mrays/s)]");
        logInfo(" RT PERF TOTAL           : {:.2f}", rtPerformance / benchmarkCycle);
        logInfo(" RT PERF PRIMARY         : {:.2f}", rtPerformancePrimary / benchmarkCycle);
        logInfo(" RT PERF SHADOW          : {:.2f}", rtPerformanceShadow / benchmarkCycle);
        logInfo(" RT PERF PATH            : {:.2f}", rtPerformancePath / benchmarkCycle);
        logInfo("");
        logInfo(" [Overall Time]");
        logInfo(" RENDER KERNELS          : {:.4f} ms", renderKernelsTime / benchmarkCycle);
        logInfo(" RENDER ABSOLUTE         : {:.4f} ms", renderAbsoluteTime / benchmarkCycle);
        logInfo("--------------------------------------------------");
        logInfo("==================================================");

#if SORT_LOG
        auto printAndSum = [&](const std::string& label, uint64_t counts[], float morton[], float sort[], float traceSort[], float trace[], int steps) {
            uint64_t totalCounts = 0;
            float totalMorton = 0.0f, totalSort = 0.0f, totalTraceSort = 0.0f, totalTrace = 0.0f;

            logInfo("==================================================");
            logInfo("--------------------------------------------------");
            for (int i = 0; i < steps; i++) {
                int displayDepth = (label == "PATH") ? i + 1 : i;

                logInfo("SORT LOG {} AT DEPTH {}", label, displayDepth);
                logInfo("RAY COUNTS        : {}", counts[i] / benchmarkCycle);
                logInfo("MORTONCODES TIME  : {:.4f} ms", morton[i] / benchmarkCycle);
                logInfo("SORT TIME         : {:.4f} ms", sort[i] / benchmarkCycle);
                logInfo("TRACE SORT TIME   : {:.4f} ms", traceSort[i] / benchmarkCycle);
                logInfo("TRACE TIME        : {:.4f} ms\n", trace[i] / benchmarkCycle);
                logInfo("");

                totalCounts += counts[i];
                totalMorton += morton[i];
                totalSort += sort[i];
                totalTraceSort += traceSort[i];
                totalTrace += trace[i];
            }

            logInfo("--------------------------------------------------");
            logInfo("SORT LOG {} TOTAL", label);
            logInfo("RAY COUNTS        : {}", totalCounts / benchmarkCycle);
            logInfo("MORTONCODES TIME  : {:.4f} ms", totalMorton / benchmarkCycle);
            logInfo("SORT TIME         : {:.4f} ms", totalSort / benchmarkCycle);
            logInfo("TRACE SORT TIME   : {:.4f} ms", totalTraceSort / benchmarkCycle);
            logInfo("TRACE TIME        : {:.4f} ms\n", totalTrace / benchmarkCycle);
            logInfo("--------------------------------------------------");
            logInfo("==================================================");
            };

        printAndSum("SHADOW", shadowRayCounts, shadowMortoncodesTimes, shadowSortTimes, shadowTraceSortTimes, shadowTraceTimes, recursionDepth + 1);

        printAndSum("PATH", pathRayCounts, pathMortoncodesTimes, pathSortTimes, pathTraceSortTimes, pathTraceTimes, recursionDepth);
#endif

#ifdef __ANDROID__
        ANativeActivity_finish(androidApp->activity);
#else
        system("pause");
        exit(0);
#endif
	}

	frameCount++;

    return renderKernelsTimeCur;
}