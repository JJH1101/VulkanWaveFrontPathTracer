#pragma once

#include "ScenePartitioner.h"

#include <memory>

namespace scene
{
    class ASBuilder
    {
    public:
        enum class BuildFlags { Default, Partitioned };

        ASBuilder() = default;
        ~ASBuilder();
        ASBuilder(const ASBuilder&) = delete;
        ASBuilder& operator=(const ASBuilder&) = delete;

        // queue must belong to device.commandPool's family and support AS builds.
        void init(vks::VulkanDevice& device, VkQueue queue);
        // Synchronous full rebuild. Model must outlive rendering; Default borrows its buffers.
        // Partitioned requires retained CPU geometry in scene/world coordinates.
        void build(vkglTF::Model& model, BuildFlags flags = BuildFlags::Default);
        VkAccelerationStructureKHR getTLASHandle() const noexcept { return topLevelAS.handle; }
        const vks::Buffer& getGeometryNodeBuffer() const noexcept { return geometryNodeBuffer; }

    private:
        struct ScratchBuffer
        {
            uint64_t deviceAddress = 0;
            VkBuffer handle = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
        };

        struct AccelerationStructure
        {
            VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
            uint64_t deviceAddress = 0;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkBuffer buffer = VK_NULL_HANDLE;
            uint32_t geometryBase = 0;
        };

        // Same 64-byte layout as GeometryNode in geometrytypes.glsl.
        struct GeometryNode
        {
            glm::vec4 baseColorFactor;
            uint64_t vertexBufferDeviceAddress;
            uint64_t indexBufferDeviceAddress;
            int32_t textureIndexBaseColor;
            int32_t textureIndexNormal;
            int32_t textureIndexMetallicRoughness;
            int32_t textureIndexEmissive;
            float metallicFactor;
            float roughnessFactor;
            float _padding0;
            float _padding1;
        };

        // Both modes share the geometry preparation and batch BLAS build implementation.
        void CreateBottomLevelAccelerationStructure(vkglTF::Model& model,
            const PartitionedScene* partitioned = nullptr);
        void CreatePartitionedBottomLevelAccelerationStructure(vkglTF::Model& model);
        void CreateTopLevelAccelerationStructure();
        void clear();

        ScratchBuffer createScratchBuffer(VkDeviceSize size);
        void deleteScratchBuffer(ScratchBuffer& scratchBuffer);
        void createAccelerationStructure(AccelerationStructure& accelerationStructure,
            VkAccelerationStructureTypeKHR type, const VkAccelerationStructureBuildSizesInfoKHR& buildSizeInfo);
        void deleteAccelerationStructure(AccelerationStructure& accelerationStructure);

        vks::VulkanDevice* device = nullptr;
        VkQueue queue = VK_NULL_HANDLE;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR properties{};
        vks::Buffer transformBuffer{};
        vks::Buffer geometryNodeBuffer{};
        std::vector<AccelerationStructure> bottomLevelAS;
        AccelerationStructure topLevelAS;
        ScratchBuffer scratchBuffer;
        // Keep partitioned vertex/index buffers alive for shading, not just AS building.
        std::unique_ptr<PartitionedScene> partitionedScene;

        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    };
}
