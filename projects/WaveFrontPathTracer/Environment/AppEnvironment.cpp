/**
 * \file	AppEnvironment.cpp
 * \author	Daniel Meister
 * \date	2014/05/10
 * \brief	AppEnvironment class source file.
 */

#include "AppEnvironment.h"

void AppEnvironment::registerOptions() {

    registerOption("Application.mode", "interactive", OPT_STRING);

    registerOption("Benchmark.output", "default", OPT_STRING);
    registerOption("Benchmark.images", "0", OPT_BOOL);

    registerOption("Resolution.width", "1024", OPT_INT);
    registerOption("Resolution.height", "768", OPT_INT);

    registerOption("Scene.filename", OPT_STRING);
    registerOption("Scene.light", "5.0 -2.0 1.0", OPT_VECTOR);
    registerOption("Scene.backgroundcolor", "0.0 0.0 1.0", OPT_VECTOR);
    registerOption("Scene.headlight", "0", OPT_BOOL);

    registerOption("Renderer.numberOfPrimarySamples", "1", OPT_INT);
    registerOption("Renderer.numberOfShadowSamples", "2", OPT_INT);
    registerOption("Renderer.shadowRadius", "0.001", OPT_FLOAT);
    registerOption("Renderer.rayType", "primary", OPT_STRING);
    registerOption("Renderer.recursionDepth", "1", OPT_INT);
    registerOption("Renderer.keyValue", "0.6", OPT_FLOAT);
    registerOption("Renderer.whitePoint", "2.0", OPT_FLOAT);
    registerOption("Renderer.russianRoulette", "0", OPT_BOOL);
    registerOption("Renderer.sortShadowRays", "0", OPT_BOOL);
    registerOption("Renderer.sortPathRays", "0", OPT_BOOL);

    registerOption("Camera.position", "0.0 0.0 0.0", OPT_VECTOR);
    registerOption("Camera.rotation", "0.0 0.0 0.0", OPT_VECTOR);
    registerOption("Camera.nearPlane", "0.001", OPT_FLOAT);
    registerOption("Camera.farPlane", "3.0", OPT_FLOAT);
    registerOption("Camera.fieldOfView", "45.0", OPT_FLOAT);

}

AppEnvironment::AppEnvironment() : Environment() {
    registerOptions();
}
