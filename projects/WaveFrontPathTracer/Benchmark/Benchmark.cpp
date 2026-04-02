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

   if (renderer->getPrintSortLogs()) {
       // Clear sort counts.
       for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH + 1; ++i) {
           shadowSortLogs[i].rayCount = 0;
           shadowSortLogs[i].mortonCodesTime = 0.0f;
           shadowSortLogs[i].sortTime = 0.0f;
           shadowSortLogs[i].reorderTime = 0.0f;
           shadowSortLogs[i].traceSortTime = 0.0f;
           shadowSortLogs[i].traceTime = 0.0f;
       }

       for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; ++i) {
           pathSortLogs[i].rayCount = 0;
           pathSortLogs[i].mortonCodesTime = 0.0f;
           pathSortLogs[i].sortTime = 0.0f;
           pathSortLogs[i].reorderTime = 0.0f;
           pathSortLogs[i].traceSortTime = 0.0f;
           pathSortLogs[i].traceTime = 0.0f;
       }
   }

   frameCount = 0;

}

float Benchmark::run(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels) {

    float renderKernelsTimeCur;
    float renderAbsoluteTimeCur;

	auto start = std::chrono::high_resolution_clock::now();
	renderKernelsTimeCur = renderer->render(camera, extent, pixels, framePixels);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> duration = end - start;
	renderAbsoluteTimeCur = duration.count();

    int recursionDepth = renderer->getRecursionDepth();
	bool printSortLogs = renderer->getPrintSortLogs();
    
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

        if (printSortLogs) {
            SortLog* shadowSortLogsCur = renderer->getShadowSortLogs();
            SortLog* pathSortLogsCur = renderer->getPathSortLogs();

            for (int i = 0; i < recursionDepth + 1; ++i) {
                shadowSortLogs[i] += shadowSortLogsCur[i];
            }

            for (int i = 0; i < recursionDepth; ++i) {
                pathSortLogs[i] += pathSortLogsCur[i];
            }
        }
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

        if (printSortLogs) {
            auto printAndSum = [&](const std::string& label, SortLog sortLogs[], int steps) {
                SortLog totalLog{};

                logInfo("==================================================");
                logInfo("--------------------------------------------------");
                for (int i = 0; i < steps; i++) {
                    int displayDepth = (label == "PATH") ? i + 1 : i;

                    logInfo("SORT LOG {} AT DEPTH {}", label, displayDepth);
                    logInfo("RAY COUNTS        : {}", sortLogs[i].rayCount / benchmarkCycle);
                    logInfo("MORTONCODES TIME  : {:.4f} ms", sortLogs[i].mortonCodesTime / benchmarkCycle);
                    logInfo("SORT TIME         : {:.4f} ms", sortLogs[i].sortTime / benchmarkCycle);
                    logInfo("REORDER TIME      : {:.4f} ms", sortLogs[i].reorderTime / benchmarkCycle);
                    logInfo("TRACE SORT TIME   : {:.4f} ms", sortLogs[i].traceSortTime / benchmarkCycle);
                    logInfo("TRACE TIME        : {:.4f} ms\n", sortLogs[i].traceTime / benchmarkCycle);
                    logInfo("");

                    totalLog += sortLogs[i];
                }

                logInfo("--------------------------------------------------");
                logInfo("SORT LOG {} TOTAL", label);
                logInfo("RAY COUNTS        : {}", totalLog.rayCount / benchmarkCycle);
                logInfo("MORTONCODES TIME  : {:.4f} ms", totalLog.mortonCodesTime / benchmarkCycle);
                logInfo("SORT TIME         : {:.4f} ms", totalLog.sortTime / benchmarkCycle);
                logInfo("REORDER TIME      : {:.4f} ms", totalLog.reorderTime / benchmarkCycle);
                logInfo("TRACE SORT TIME   : {:.4f} ms", totalLog.traceSortTime / benchmarkCycle);
                logInfo("TRACE TIME        : {:.4f} ms\n", totalLog.traceTime / benchmarkCycle);
                logInfo("--------------------------------------------------");
                logInfo("==================================================");
                };

            printAndSum("SHADOW", shadowSortLogs, recursionDepth + 1);

            printAndSum("PATH", pathSortLogs, recursionDepth);
        }

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