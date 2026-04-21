/**
  * Original Source
  * \file	RayBuffer.h
  * \author	Daniel Meister
  * \date	2014/05/10
  * \brief	RayBuffer class header file.
  */

  /**
    * Modified Source
    * \file   RayBuffer.h
    * \author Junhyeok Jang
    * \date	  2026/03/06
    * \brief  Modify the structure for Vulkan environment
    */


#pragma once

#include "VulkanDevice.h"

class RayBuffer {

private:

    int capacity;
    int size;
    vks::Buffer rays[2];        // Ray
    vks::Buffer results;        // RayResult
    vks::Buffer indexToPixel[2]; // int
    vks::Buffer mortonCodes;    // uint32_t
    vks::Buffer indices;        // int
    vks::Buffer spine;          // Radix sort temporary buffer
    bool closestHit;

    bool swapBuffers = false;
    float rayLength;

public:

    RayBuffer(void);
    ~RayBuffer(void);

    void swap(void);

    int getSize(void) const;
    int getCapacity(void) const;
    void resize(vks::VulkanDevice& device, int n, bool isPrimary = false);
    void resizeReorderingBuffers(vks::VulkanDevice& device, bool reorderRays);

    bool getClosestHit(void) const;
    void setClosestHit(bool closestHit);
    float getRayLength(void);
    void setRayLength(float rayLength);

    vks::Buffer& getRayBuffer(void);
    vks::Buffer& getOutRayBuffer(void);
    vks::Buffer& getResultBuffer(void);
    vks::Buffer& getIndexBuffer(void);
    vks::Buffer& getMortonCodeBuffer(void);
    vks::Buffer& getSpineBuffer(void);
    vks::Buffer& getIndexToPixelBuffer(void);
    vks::Buffer& getOutIndexToPixelBuffer(void);

    friend class HipTracer;

};
