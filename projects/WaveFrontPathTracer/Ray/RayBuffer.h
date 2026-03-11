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
    vks::Buffer rays;           // Ray
    vks::Buffer results;        // RayResult
    vks::Buffer indexToSlot;    // int
    vks::Buffer slotToIndex;    // int
    vks::Buffer stats;          // glm::ivec2
    vks::Buffer mortonCodes[2]; // uint32_t
    vks::Buffer indices[2];     // int
    vks::Buffer spine;          // Radix sort temporary buffer
    bool closestHit;

    int mortonCodeBits;
    float rayLength;

    void randomSort(void);

    float computeMortonCodes(vks::Buffer& mortonCodes);
    float reorderRays(vks::Buffer& oldRayBuffer, vks::Buffer& oldIndexToPixel);
    float mortonSort(void);

public:

    RayBuffer(void);
    ~RayBuffer(void);

    int getSize(void) const;
    void resize(vks::VulkanDevice& device, VkQueue queue, int n);

    bool getClosestHit(void) const;
    void setClosestHit(bool closestHit);
    int getMortonCodeBits(void);
    void setMortonCodeBits(int mortonCodeBits);
    float getRayLength(void);
    void setRayLength(float rayLength);

    vks::Buffer& getRayBuffer(void);
    vks::Buffer& getResultBuffer(void);
    vks::Buffer& getStatBuffer(void);
    vks::Buffer& getIndexBuffer(void);
    vks::Buffer& getIndexToSlotBuffer(void);
    vks::Buffer& getSlotToIndexBuffer(void);

    friend class HipTracer;

};
