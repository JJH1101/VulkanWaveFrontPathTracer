/**
  * Original Source
  * \file	Ray.h
  * \author	Daniel Meister
  * \date	2014/05/10
  * \brief	Ray structure header file.
  */

 /**
   * Modified Source
   * \file   Ray.h
   * \author Junhyeok Jang
   * \date	 2026/03/06
   * \brief	 Modify the structures for Vulkan & glsl environment
   */

#pragma once

#ifdef __cplusplus

#include <glm/glm.hpp>
using vec3 = glm::vec3;
using vec2 = glm::vec2;
using uint = uint32_t;
#endif

/*
* @note: These structures are shared between cpp and shader (ray.glsl).
* Keep both definitions synchronozed when modifying
*/

struct Ray {
    vec3 origin;
    float tmin;
    vec3 direction;
    float tmax;
};

struct RayResult {
    int primitiveId;
    int geometryId;
    float t;
    uint packedBary;
};