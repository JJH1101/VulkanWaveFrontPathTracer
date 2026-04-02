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
    rays[0].destroy();
    rays[1].destroy();
    results.destroy();
    indexToSlot[0].destroy();
    indexToSlot[1].destroy();
    slotToIndex[0].destroy();
    slotToIndex[1].destroy();
    mortonCodes.destroy();
    indices.destroy();
    spine.destroy();
}

void RayBuffer::swap() {
    swapBuffers = !swapBuffers;
}

int RayBuffer::getSize() const {
    return size;
}

int RayBuffer::getCapacity() const {
    return capacity;
}

void RayBuffer::resize(vks::VulkanDevice& device, int size) {
    assert(size >= 0);
    if (capacity < size) {
        capacity = size;

        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        int swapIdx = swapBuffers ? 1 : 0;

		vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &rays[swapIdx], size * sizeof(Ray));
        vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &results, size * sizeof(RayResult));
        vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &indexToSlot[swapIdx], size * sizeof(int));
        vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &slotToIndex[swapIdx], size * sizeof(int));
    }
    this->size = size;
}

void RayBuffer::resizeReorderingBuffers(vks::VulkanDevice& device, bool reorderRays) {
    int swapIdx = swapBuffers ? 0 : 1;

    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    if (rays[swapIdx].size < capacity * sizeof(Ray)) {
        if (reorderRays) {
            vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &rays[swapIdx], capacity * sizeof(Ray));
            vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &indexToSlot[swapIdx], capacity * sizeof(int));
            vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &slotToIndex[swapIdx], capacity * sizeof(int));
        }
        vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &indices, capacity * sizeof(uint32_t));
		vks::util::resizeDiscardBuffer(device, usageFlags, memFlags, &mortonCodes, capacity * sizeof(uint32_t));
    }
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
    return (swapBuffers) ? rays[1] : rays[0];
}

vks::Buffer& RayBuffer::getOutRayBuffer() {
    return (swapBuffers) ? rays[0] : rays[1];
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
    return (swapBuffers) ? indexToSlot[1] : indexToSlot[0];
}

vks::Buffer& RayBuffer::getOutIndexToSlotBuffer() {
    return (swapBuffers) ? indexToSlot[0] : indexToSlot[1];
}

vks::Buffer& RayBuffer::getSlotToIndexBuffer() {
    return (swapBuffers) ? slotToIndex[1] : slotToIndex[0];
}

vks::Buffer& RayBuffer::getOutSlotToIndexBuffer() {
    return (swapBuffers) ? slotToIndex[0] : slotToIndex[1];
}
