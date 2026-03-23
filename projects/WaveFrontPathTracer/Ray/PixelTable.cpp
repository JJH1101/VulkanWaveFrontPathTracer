/**
  * Original Source
  * \file	PixelTable.cpp
  * \author	Daniel Meister
  * \date	2014/05/10
  * \brief	PixelTable class source file.
  */

  /**
    * Modified Source
    * \file	  PixelTable.cpp
    * \author Junhyeok Jang
    * \date	  2026/03/09
    * \brief  Modified for Vulkan
    */

#include "PixelTable.h"
#include "../Utils/BufferUtils.h"
#include <vector>

void PixelTable::recalculate(vks::VulkanDevice& device, VkQueue queue) {

    // Construct LUTs.
    vks::util::resizeDiscardBuffer(
        device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &indexToPixel,
        size.x * size.y * sizeof(int));
    vks::util::resizeDiscardBuffer(
        device,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &pixelToIndex,
        size.x * size.y * sizeof(int));
    std::vector<int> idxtopos(size.x * size.y);
    std::vector<int> postoidx(size.x * size.y);

    // Smart mode.
    int idx = 0;
    int bheight = size.y & ~7;
    int bwidth = size.x  & ~7;

    // Bulk of the image, sort blocks in in morton order.
    int maxdim = (bwidth > bheight) ? bwidth : bheight;

    // Round up to nearest power of two.
    maxdim |= maxdim >> 1;
    maxdim |= maxdim >> 2;
    maxdim |= maxdim >> 4;
    maxdim |= maxdim >> 8;
    maxdim |= maxdim >> 16;
    maxdim = (maxdim + 1) >> 1;

    int width8 = bwidth >> 3;
    int height8 = bheight >> 3;
    for (int i = 0; i < maxdim * maxdim; i++) {

        // Get interleaved bit positions.
        int tx = 0;
        int ty = 0;
        int val = i;
        int bit = 1;

        while (val) {
            if (val & 1) tx |= bit;
            if (val & 2) ty |= bit;
            bit += bit;
            val >>= 2;
        }

        if (tx < width8 && ty < height8) {
            for (int inner = 0; inner < 64; inner++) {
                // Swizzle ix and iy within blocks as well.
                int ix = ((inner & 1) >> 0) | ((inner & 4) >> 1) | ((inner & 16) >> 2);
                int iy = ((inner & 2) >> 1) | ((inner & 8) >> 2) | ((inner & 32) >> 3);
                int pos = (ty * 8 + iy) * size.x + (tx * 8 + ix);
                postoidx[pos] = idx;
                idxtopos[idx++] = pos;
            }
        }

    }

    // If height not divisible, add horizontal stripe below bulk.
    for (int px = 0; px < bwidth; px++)
        for (int py = bheight; py < size.y; py++) {
            int pos = px + py * size.x;
            postoidx[pos] = idx;
            idxtopos[idx++] = pos;
        }

    // If width not divisible, add vertical stripe and the corner.
    for (int py = 0; py < size.y; py++)
        for (int px = bwidth; px < size.x; px++) {
            int pos = px + py * size.x;
            postoidx[pos] = idx;
            idxtopos[idx++] = pos;
        }

    // Copy to device buffer
    VkDeviceSize bufferSize = idxtopos.size() * sizeof(int);
    vks::Buffer stagingIdxToPix, stagingPixToIdx;

    device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingIdxToPix, bufferSize, idxtopos.data());

    device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingPixToIdx, bufferSize, postoidx.data());

    VkCommandBuffer copyCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion = { 0, 0, bufferSize };

    vkCmdCopyBuffer(copyCmd, stagingIdxToPix.buffer, indexToPixel.buffer, 1, &copyRegion);
    vkCmdCopyBuffer(copyCmd, stagingPixToIdx.buffer, pixelToIndex.buffer, 1, &copyRegion);

    device.flushCommandBuffer(copyCmd, queue, true);

    stagingIdxToPix.destroy();
    stagingPixToIdx.destroy();

    // Done!
}

PixelTable::PixelTable() : size(glm::ivec2(0)) {
}

PixelTable::~PixelTable() {
    indexToPixel.destroy();
    pixelToIndex.destroy();
}

void PixelTable::setSize(const glm::ivec2 & _size, vks::VulkanDevice & device, VkQueue queue) {
    bool recalc = (_size != size);
    size = _size;
    if (recalc) recalculate(device, queue);
}

const glm::ivec2 & PixelTable::getSize() {
    return size;
}

vks::Buffer & PixelTable::getIndexToPixel() {
    return indexToPixel;
}

vks::Buffer & PixelTable::getPixelToIndex() {
    return pixelToIndex;
}
