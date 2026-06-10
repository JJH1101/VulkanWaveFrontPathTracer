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

   if (renderer->getPrintBounceLogs()) {
       // Clear sort counts.
       for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH + 1; ++i) {
           shadowBounceLogs[i].rayCount = 0;
           shadowBounceLogs[i].mortonCodesTime = 0.0f;
           shadowBounceLogs[i].sortTime = 0.0f;
           shadowBounceLogs[i].reorderTime = 0.0f;
           shadowBounceLogs[i].traceTime = 0.0f;
       }

       for (int i = 0; i < RENDERER_MAX_RECURSION_DEPTH; ++i) {
           pathBounceLogs[i].rayCount = 0;
           pathBounceLogs[i].mortonCodesTime = 0.0f;
           pathBounceLogs[i].sortTime = 0.0f;
           pathBounceLogs[i].reorderTime = 0.0f;
           pathBounceLogs[i].traceTime = 0.0f;
       }
   }

   frameCount = 0;
   view = 0;

   Environment::getInstance()->getIntValue("Benchmark.warmupCyclesPerView", warmupCyclesPerView);
   Environment::getInstance()->getIntValue("Benchmark.benchmarkCyclesPerView", benchmarkCyclesPerView);

   views.cameraPositions.clear();
   views.cameraRotations.clear();
   views.lightPositions.clear();

   Environment::getInstance()->getVectorValues("Benchmark.views.cameraPositions", views.cameraPositions);
   Environment::getInstance()->getVectorValues("Benchmark.views.cameraRotations", views.cameraRotations);
   Environment::getInstance()->getVectorValues("Benchmark.views.lightPositions", views.lightPositions);

   views.numberOfViews = std::min({ views.cameraPositions.size(), views.cameraRotations.size(), views.lightPositions.size() });
}

float Benchmark::run(Camera& camera, glm::ivec2 extent, vks::Buffer& pixels, vks::Buffer& framePixels) {

    if(frameCount == 0) {
        camera.setPosition(views.cameraPositions[view]);
        camera.setRotation(views.cameraRotations[view]);
        renderer->setLightPosition(views.lightPositions[view]);
	}

    float renderKernelsTimeCur;
    float renderAbsoluteTimeCur;

	auto start = std::chrono::high_resolution_clock::now();
	renderKernelsTimeCur = renderer->render(camera, extent, pixels, framePixels);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> duration = end - start;
	renderAbsoluteTimeCur = duration.count();

    int recursionDepth = renderer->getRecursionDepth();
	bool printBounceLogs = renderer->getPrintBounceLogs();
    
	if (frameCount >= warmupCyclesPerView) {
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

        if (printBounceLogs) {
            BounceLog* shadowBounceLogsCur = renderer->getShadowBounceLogs();
            BounceLog* pathBounceLogsCur = renderer->getPathBounceLogs();

            for (int i = 0; i < recursionDepth + 1; ++i) {
                shadowBounceLogs[i] += shadowBounceLogsCur[i];
            }

            for (int i = 0; i < recursionDepth; ++i) {
                pathBounceLogs[i] += pathBounceLogsCur[i];
            }
        }
	}

	if (frameCount == warmupCyclesPerView + benchmarkCyclesPerView - 1) {
        if (view == views.numberOfViews - 1) {
			uint32_t benchmarkCyclesTotal = benchmarkCyclesPerView * views.numberOfViews;

            // Write results.
            logInfo("==================================================");
            logInfo(" RAY TRACING BENCHMARK RESULTS ");
            logInfo("--------------------------------------------------");
            logInfo(" RAY TYPE                : {}", rayTypeToString(renderer->getRayType()));
            logInfo("");
            logInfo(" [Count (Average)]");
            logInfo(" TOTAL RAYS              : {}", numberOfRays / benchmarkCyclesTotal);
            logInfo(" PRIMARY RAYS            : {}", numberOfPrimaryRays / benchmarkCyclesTotal);
            logInfo(" SHADOW RAYS             : {}", numberOfShadowRays / benchmarkCyclesTotal);
            logInfo(" PATH RAYS               : {}", numberOfPathRays / benchmarkCyclesTotal);
            logInfo("");
            logInfo(" [Time (ms)]");
            logInfo(" TRACE TOTAL             : {:.4f} ms", traceTime / benchmarkCyclesTotal);
            logInfo(" TRACE PRIMARY           : {:.4f} ms", traceTimePrimary / benchmarkCyclesTotal);
            logInfo(" TRACE SHADOW            : {:.4f} ms", traceTimeShadow / benchmarkCyclesTotal);
            logInfo(" TRACE PATH              : {:.4f} ms", traceTimePath / benchmarkCyclesTotal);
            logInfo("");
            logInfo(" [Performance (Mrays/s)]");
            logInfo(" RT PERF TOTAL           : {:.2f}", rtPerformance / benchmarkCyclesTotal);
            logInfo(" RT PERF PRIMARY         : {:.2f}", rtPerformancePrimary / benchmarkCyclesTotal);
            logInfo(" RT PERF SHADOW          : {:.2f}", rtPerformanceShadow / benchmarkCyclesTotal);
            logInfo(" RT PERF PATH            : {:.2f}", rtPerformancePath / benchmarkCyclesTotal);
            logInfo("");
            logInfo(" [Overall Time]");
            logInfo(" RENDER KERNELS          : {:.4f} ms", renderKernelsTime / benchmarkCyclesTotal);
            logInfo(" RENDER ABSOLUTE         : {:.4f} ms", renderAbsoluteTime / benchmarkCyclesTotal);
            logInfo("--------------------------------------------------");
            logInfo("==================================================");

            if (printBounceLogs) {
                auto printAndSum = [&](const std::string& label, BounceLog bounceLogs[], int steps, bool sortRays, bool reorderRays) {
                    BounceLog totalLog{};

                    logInfo("==================================================");
                    logInfo("--------------------------------------------------");
                    for (int i = 0; i < steps; i++) {
                        int displayDepth = (label == "PATH") ? i + 1 : i;

                        logInfo("BOUNCE LOG {} AT DEPTH {}", label, displayDepth);
                        logInfo("RAY COUNTS        : {}", bounceLogs[i].rayCount / benchmarkCyclesTotal);
                        if (sortRays) {
                            logInfo("MORTONCODES TIME  : {:.4f} ms", bounceLogs[i].mortonCodesTime / benchmarkCyclesTotal);
                            logInfo("SORT TIME         : {:.4f} ms", bounceLogs[i].sortTime / benchmarkCyclesTotal);
                            if (reorderRays) {
                                logInfo("REORDER TIME      : {:.4f} ms", bounceLogs[i].reorderTime / benchmarkCyclesTotal);
                            }
                        }
                        logInfo("TRACE TIME        : {:.4f} ms", bounceLogs[i].traceTime / benchmarkCyclesTotal);
                        logInfo("");

                        totalLog += bounceLogs[i];
                    }

                    logInfo("--------------------------------------------------");
                    logInfo("BOUNCE LOG {} TOTAL", label);
                    logInfo("RAY COUNTS        : {}", totalLog.rayCount / benchmarkCyclesTotal);
                    if (sortRays) {
                        logInfo("MORTONCODES TIME  : {:.4f} ms", totalLog.mortonCodesTime / benchmarkCyclesTotal);
                        logInfo("SORT TIME         : {:.4f} ms", totalLog.sortTime / benchmarkCyclesTotal);
                        if (reorderRays) {
                            logInfo("REORDER TIME      : {:.4f} ms", totalLog.reorderTime / benchmarkCyclesTotal);
                        }
                    }
                    logInfo("TRACE TIME        : {:.4f} ms", totalLog.traceTime / benchmarkCyclesTotal);
                    logInfo("--------------------------------------------------");
                    logInfo("==================================================");
                    };

                printAndSum("SHADOW", shadowBounceLogs, recursionDepth + 1, renderer->getSortShadowRays(), renderer->getReorderShadowRays());

                printAndSum("PATH", pathBounceLogs, recursionDepth, renderer->getSortPathRays(), renderer->getReorderPathRays());
            }

#ifdef __ANDROID__
            ANativeActivity_finish(androidApp->activity);
#else
            system("pause");
            exit(0);
#endif
        }
        else {
            frameCount = -1;
            view++;
        }
	}

	frameCount++;

    return renderKernelsTimeCur;
}