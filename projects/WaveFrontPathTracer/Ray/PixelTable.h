/**
  * Original Source
  * \file	PixelTable.h
  * \author	Daniel Meister
  * \date	2014/05/10
  * \brief	PixelTable class header file.
  */

  /**
    * Modified Source 
    * \file	  PixelTable.h
    * \author Junhyeok Jang
    * \date	  2026/03/09
    * \brief  Modified for Vulkan
    */

#pragma once

#include "VulkanDevice.h"
#include "glm/glm.hpp"

class PixelTable {

private:

    glm::ivec2 size;
    vks::Buffer indexToPixel; // int
    vks::Buffer pixelToIndex; // int

    void recalculate(vks::VulkanDevice& device, VkQueue queue);

public:

    PixelTable(void);
    ~PixelTable(void);

    void setSize(const glm::ivec2 & size, vks::VulkanDevice & device, VkQueue queue);

    const glm::ivec2 & getSize(void);
    vks::Buffer & getIndexToPixel(void);
    vks::Buffer & getPixelToIndex(void);

};