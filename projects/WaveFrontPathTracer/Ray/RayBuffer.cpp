/**
 * Original Source 
 * \file	RayBuffer.cpp
 * \author	Daniel Meister
 * \date	2014/05/10
 * \brief	RayBuffer class source file.
 */

 /**
  * Modified Source
  * \file	RayBuffer.cpp
  * \author	Junhyeok Jang
  * \date	2026/03/06
  * \brief	Modify for Vulkan
  */

#include "RayBuffer.h"
#include "Ray.h"
#include "../Utils/BufferUtils.hpp"

void RayBuffer::randomSort() {
    // Not implemented yet
}

float RayBuffer::computeMortonCodes(vks::Buffer & mortonCodes) {

    return 0.f; // Not implemented yet
}

float RayBuffer::reorderRays(vks::Buffer& oldRayBuffer, vks::Buffer& oldIndexToPixel) {

    return 0.f; // Not implemented yet
}

float RayBuffer::mortonSort() {

    return 0.f; // Not implemented yet
}

RayBuffer::RayBuffer() : size(0), capacity(0), closestHit(true), mortonCodeBits(30), rayLength(0.25f) {
}

RayBuffer::~RayBuffer() {
    rays.destroy();
    results.destroy();
    indexToSlot.destroy();
    slotToIndex.destroy();
    stats.destroy();
    mortonCodes[0].destroy();
    mortonCodes[1].destroy();
    indices[0].destroy();
    indices[1].destroy();
    spine.destroy();
}

int RayBuffer::getSize() const {
    return size;
}

void RayBuffer::resize(vks::VulkanDevice& device, VkQueue queue, int size) {
    assert(size >= 0);
    if (capacity < size) {
        capacity = size;

        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &rays, size * sizeof(Ray));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &results, size * sizeof(RayResult));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &stats, size * sizeof(glm::ivec2));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &indexToSlot, size * sizeof(int));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &slotToIndex, size * sizeof(int));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &indices[0], size * sizeof(int));
    }
    this->size = size;
}

bool RayBuffer::getClosestHit() const {
    return closestHit;
}

void RayBuffer::setClosestHit(bool closestHit) {
    this->closestHit = closestHit;
}

int RayBuffer::getMortonCodeBits() {
    return mortonCodeBits;
}

void RayBuffer::setMortonCodeBits(int mortonCodeBits) {
    if (mortonCodeBits < 6 || mortonCodeBits > 64) {}
    else this->mortonCodeBits = mortonCodeBits;
}

float RayBuffer::getRayLength() {
    return rayLength;
}

void RayBuffer::setRayLength(float rayLength) {
    if (rayLength < 0.0f || rayLength > 1.0f) {}
    else this->rayLength = rayLength;
}


vks::Buffer& RayBuffer::getRayBuffer() {
    return rays;
}
vks::Buffer& RayBuffer::getResultBuffer() {
    return results;
}

vks::Buffer& RayBuffer::getStatBuffer() {
    return stats;
}

vks::Buffer& RayBuffer::getIndexBuffer() {
    return indices[0];
}

vks::Buffer& RayBuffer::getIndexToSlotBuffer() {
    return indexToSlot;
}

vks::Buffer& RayBuffer::getSlotToIndexBuffer() {
    return slotToIndex;
}
