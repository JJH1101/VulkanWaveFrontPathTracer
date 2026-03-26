/**
 * \file	Benchmark.cpp
 * \author	Junhyeok Jang
 * \date	2026/03/25
 * \brief	Benchmark class source file.
 */

#include "Benchmark.h"
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
   numberOfRays = 0;
   numberOfPrimaryRays = 0;
   numberOfShadowRays = 0;
   numberOfPathRays = 0;

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

	if (frameCount >= warmupCycle) {
        renderKernelsTime += renderKernelsTimeCur;
        renderAbsoluteTime += renderAbsoluteTimeCur;
		rtPerformance += renderer->getTracePerformance();
		rtPerformancePrimary += renderer->getPrimaryTracePerformance();
		rtPerformanceShadow +=  renderer->getShadowTracePerformance();
		rtPerformancePath += renderer->getPathTracePerformance();
		numberOfRays += renderer->getNumberOfRays();
		numberOfPrimaryRays += renderer->getNumberOfPrimaryRays();
		numberOfShadowRays += renderer->getNumberOfShadowRays();
		numberOfPathRays += renderer->getNumberOfPathRays();
	}

	if (frameCount == warmupCycle + benchmarkCycle - 1) {
        // Write results.
        std::cout << "RAY TYPE\n";
        std::cout << rayTypeToString(renderer->getRayType()) << "\n\n";

        std::cout << "NUMBER OF RAYS\n";
        std::cout << numberOfRays / benchmarkCycle << "\n\n";
        std::cout << "NUMBER OF PRIMARY RAYS\n";
        std::cout << numberOfPrimaryRays / benchmarkCycle << "\n\n";
        std::cout << "NUMBER OF SHADOW RAYS\n";
        std::cout << numberOfShadowRays / benchmarkCycle << "\n\n";
        std::cout << "NUMBER OF PATH RAYS\n";
        std::cout << numberOfPathRays / benchmarkCycle << "\n\n";
        std::cout << "RT PERFORMANCE\n";
        std::cout << rtPerformance / benchmarkCycle << "\n\n";
        std::cout << "RT PERFORMANCE PRIMARY\n";
        std::cout << rtPerformancePrimary / benchmarkCycle << "\n\n";
        std::cout << "RT PERFORMANCE SHADOW\n";
        std::cout << rtPerformanceShadow / benchmarkCycle << "\n\n";
        std::cout << "RT PERFORMANCE PATH\n";
        std::cout << rtPerformancePath / benchmarkCycle << "\n\n";
        std::cout << "RENDER TIME KERNELS\n";
        std::cout << renderKernelsTime / benchmarkCycle << "\n\n";
        std::cout << "RENDER TIME ABSOLUTE\n";
        std::cout << renderAbsoluteTime / benchmarkCycle << "\n\n";

        system("pause");
        exit(0);
	}

	frameCount++;

    return renderKernelsTimeCur;
}