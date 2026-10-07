#pragma once

#include "VulkanglTFModel.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace scene
{
    using Triangle = std::array<vkglTF::Vertex, 3>;
	using Polygon = std::vector<vkglTF::Vertex>;
    using PartitionedCell = std::vector<vkglTF::Primitive>;

	struct Aabb
	{
		glm::vec3 min{};
		glm::vec3 max{};
	};

    struct PartitionedScene {
        vks::VulkanDevice& device;

        struct Vertices {
			int count = 0;
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
		} vertices;

		struct Indices {
			int count = 0;
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
		} indices;

        std::vector<PartitionedCell> cells;

        PartitionedScene(vks::VulkanDevice& device) : device(device) {};
        PartitionedScene(const PartitionedScene&) = delete;
        PartitionedScene& operator=(const PartitionedScene&) = delete;
        PartitionedScene(PartitionedScene&& other) noexcept
            : device(other.device),
              vertices(std::exchange(other.vertices, Vertices{})),
              indices(std::exchange(other.indices, Indices{})),
              cells(std::move(other.cells)) {}
        PartitionedScene& operator=(PartitionedScene&&) = delete;

        ~PartitionedScene() {
            if(device.logicalDevice) {
                if (vertices.buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(device.logicalDevice, vertices.buffer, nullptr);
                }
                if (vertices.memory != VK_NULL_HANDLE) {
                    vkFreeMemory(device.logicalDevice, vertices.memory, nullptr);
                }
                if (indices.buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(device.logicalDevice, indices.buffer, nullptr);
                }
                if (indices.memory != VK_NULL_HANDLE) {
                    vkFreeMemory(device.logicalDevice, indices.memory, nullptr);
                }
            }
        };
    };

	class ScenePartitioner
	{
	public:
		ScenePartitioner() = default;
        ~ScenePartitioner() = default;

        PartitionedScene partition(vkglTF::Model& scene, vks::VulkanDevice& device, VkQueue transferQueue) const;

	private:
		enum class Axis : uint32_t
		{
			X = 0,
			Y = 1,
			Z = 2,
		};

		enum class Side
		{
			Min,
			Max,
		};
        
        static constexpr float CELL_WEIGHT = 0.1f;

		void clipPolygonAgainstPlane(
			const Polygon& from,
			Polygon& to,
			float boundary,
			Axis axis,
			Side side) const;

        Polygon clipTriangleAgainstAabb(
			const Triangle& triangle,
			const Aabb& bounds) const;

		bool isInside(
			float coord,
			float boundary,
			Side side) const;

		vkglTF::Vertex interpolateVertex(
			const vkglTF::Vertex& from,
			const vkglTF::Vertex& to,
			float t) const;
	};
}
