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
#include "../Utils/BufferUtils.h"

RayBuffer::RayBuffer() : size(0), capacity(0), closestHit(true), rayLength(0.25f) {
}

RayBuffer::~RayBuffer() {
    rays.destroy();
    results.destroy();
    indexToSlot.destroy();
    slotToIndex.destroy();
    mortonCodes.destroy();
    indices.destroy();
    spine.destroy();
}

int RayBuffer::getSize() const {
    return size;
}

int RayBuffer::getCapacity() const {
    return capacity;
}

void RayBuffer::resize(vks::VulkanDevice& device, VkQueue queue, int size) {
    assert(size >= 0);
    if (capacity < size) {
        capacity = size;

        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &rays, size * sizeof(Ray));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &results, size * sizeof(RayResult));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &indexToSlot, size * sizeof(int));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &slotToIndex, size * sizeof(int));
        vks::util::resizeBuffer(device, queue, usageFlags, memFlags, &indices, size * sizeof(uint32_t));
    }
    this->size = size;
}

bool RayBuffer::getClosestHit() const {
    return closestHit;
}

void RayBuffer::setClosestHit(bool closestHit) {
    this->closestHit = closestHit;
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

vks::Buffer& RayBuffer::getIndexBuffer() {
    return indices;
}

vks::Buffer& RayBuffer::getMortonCodeBuffer() {
    return mortonCodes;
}

vks::Buffer& RayBuffer::getSpineBuffer() {
    return spine;
}

vks::Buffer& RayBuffer::getIndexToSlotBuffer() {
    return indexToSlot;
}

vks::Buffer& RayBuffer::getSlotToIndexBuffer() {
    return slotToIndex;
}
