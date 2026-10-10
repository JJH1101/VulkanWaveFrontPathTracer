#include "ScenePartitioner.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

scene::PartitionedScene scene::ScenePartitioner::partition(vkglTF::Model& scene, vks::VulkanDevice& device, VkQueue transferQueue) const
{
	PartitionedScene partitionedScene(device);

	const uint32_t triangleCount = static_cast<uint32_t>(scene.indexBuffer.size() / 3);
	if (triangleCount == 0 || scene.vertexBuffer.empty())
		throw std::invalid_argument("Partitioning requires CPU triangle geometry");
	const float sceneVolume = scene.dimensions.size.x * scene.dimensions.size.y * scene.dimensions.size.z;
	// Flat scenes use one cell; volumetric scenes retain the existing density heuristic.
	const float cellWeight = sceneVolume > 0.0f ? std::cbrt(triangleCount / sceneVolume) * CELL_WEIGHT : 0.0f;

	const glm::uvec3 numCells = glm::uvec3(glm::max(scene.dimensions.size * cellWeight + 0.5f, glm::vec3(1.0f)));
	const glm::vec3 cellSize = scene.dimensions.size / glm::vec3(numCells);

	std::vector<Aabb> cellBounds(numCells.x * numCells.y * numCells.z);

	auto calcIdx = [numCells](glm::uvec3 idx)->uint32_t {return numCells.x * numCells.y * idx.z + numCells.x * idx.y + idx.x; };
	for(uint32_t z = 0; z < numCells.z; ++z) {
		for(uint32_t y = 0; y < numCells.y; ++y) {
			for(uint32_t x = 0; x < numCells.x; ++x) {
				const uint32_t idx = calcIdx(glm::uvec3(x, y, z));
				cellBounds[idx].min = scene.dimensions.min + glm::vec3(x, y, z) * cellSize;
				cellBounds[idx].max = cellBounds[idx].min + cellSize;
			}
		}
	}

	std::vector<uint32_t> partitionedIndices;
	std::vector<vkglTF::Vertex> partitionedVertices;
	std::unordered_map<vkglTF::Vertex, uint32_t> uniqueVertices;

	for (const auto& cellBound: cellBounds) {
		PartitionedCell partitionedCell;

		for (auto node : scene.linearNodes) {
        	if (node->mesh) {
            	for (auto primitive : node->mesh->primitives) {
                	if (primitive->indexCount > 0) {
						uint32_t indexCount = 0;
						for (uint32_t i = primitive->firstIndex; i < primitive->firstIndex + primitive->indexCount; i += 3) {
							const uint32_t idx0 = scene.indexBuffer[i + 0];
							const uint32_t idx1 = scene.indexBuffer[i + 1];
							const uint32_t idx2 = scene.indexBuffer[i + 2];

							Triangle triangle{
								scene.vertexBuffer[idx0],
								scene.vertexBuffer[idx1],
								scene.vertexBuffer[idx2]
							};

							glm::vec3 triMinPos = glm::min(glm::min(triangle[0].pos, triangle[1].pos), triangle[2].pos);
							glm::vec3 triMaxPos = glm::max(glm::max(triangle[0].pos, triangle[1].pos), triangle[2].pos);

							// Shared boundary faces belong to the lower cell; the scene minimum has no lower cell.
							if (cellBound.max.x < triMinPos.x || cellBound.min.x > triMaxPos.x ||
								(cellBound.min.x == triMaxPos.x && cellBound.min.x != scene.dimensions.min.x)) continue;
							if (cellBound.max.y < triMinPos.y || cellBound.min.y > triMaxPos.y ||
								(cellBound.min.y == triMaxPos.y && cellBound.min.y != scene.dimensions.min.y)) continue;
							if (cellBound.max.z < triMinPos.z || cellBound.min.z > triMaxPos.z ||
								(cellBound.min.z == triMaxPos.z && cellBound.min.z != scene.dimensions.min.z)) continue;

							Polygon clippedPolygon = clipTriangleAgainstAabb(triangle, cellBound);
							if (clippedPolygon.size() < 3) continue;

							for (uint32_t j = 1; j < clippedPolygon.size() - 1; ++j) {
								if (uniqueVertices.count(clippedPolygon[0]) == 0) {
									uniqueVertices[clippedPolygon[0]] = static_cast<uint32_t>(partitionedVertices.size());
									partitionedVertices.push_back(clippedPolygon[0]);
								}
								partitionedIndices.push_back(uniqueVertices[clippedPolygon[0]]);

								if (uniqueVertices.count(clippedPolygon[j]) == 0) {
									uniqueVertices[clippedPolygon[j]] = static_cast<uint32_t>(partitionedVertices.size());
									partitionedVertices.push_back(clippedPolygon[j]);
								}
								partitionedIndices.push_back(uniqueVertices[clippedPolygon[j]]);

								if (uniqueVertices.count(clippedPolygon[j + 1]) == 0) {
									uniqueVertices[clippedPolygon[j + 1]] = static_cast<uint32_t>(partitionedVertices.size());
									partitionedVertices.push_back(clippedPolygon[j + 1]);
								}
								partitionedIndices.push_back(uniqueVertices[clippedPolygon[j + 1]]);
							}
							indexCount += (clippedPolygon.size() - 2) * 3;
						}

						if (indexCount > 0) {
							partitionedCell.push_back(vkglTF::Primitive(partitionedIndices.size() - indexCount, indexCount, primitive->material));
						}
					}
				}
			}
		}

		if(!partitionedCell.empty()) {
			partitionedScene.cells.push_back(partitionedCell);
		}
	}

	size_t vertexBufferSize = partitionedVertices.size() * sizeof(vkglTF::Vertex);
	size_t indexBufferSize = partitionedIndices.size() * sizeof(uint32_t);
	partitionedScene.vertices.count = static_cast<uint32_t>(partitionedVertices.size());
	partitionedScene.indices.count = static_cast<uint32_t>(partitionedIndices.size());

	assert((vertexBufferSize > 0) && (indexBufferSize > 0));

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging{}, indexStaging{};

	// Create staging buffers
	// Vertex data
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize,
		&vertexStaging.buffer,
		&vertexStaging.memory,
		partitionedVertices.data()));
	// Index data
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize,
		&indexStaging.buffer,
		&indexStaging.memory,
		partitionedIndices.data()));

	// Create device local buffers
	// Vertex buffer
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::memoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&partitionedScene.vertices.buffer,
		&partitionedScene.vertices.memory));
	// Index buffer
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::memoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&partitionedScene.indices.buffer,
		&partitionedScene.indices.memory));

	// Copy from staging buffers
	VkCommandBuffer copyCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, partitionedScene.vertices.buffer, 1, &copyRegion);
	
	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(copyCmd, indexStaging.buffer, partitionedScene.indices.buffer, 1, &copyRegion);

	device.flushCommandBuffer(copyCmd, transferQueue, true);

	vkDestroyBuffer(device.logicalDevice, vertexStaging.buffer, nullptr);
	vkFreeMemory(device.logicalDevice, vertexStaging.memory, nullptr);
	vkDestroyBuffer(device.logicalDevice, indexStaging.buffer, nullptr);
	vkFreeMemory(device.logicalDevice, indexStaging.memory, nullptr);

	return partitionedScene;
}

void scene::ScenePartitioner::clipPolygonAgainstPlane(
	const Polygon& from,
	Polygon& to,
	float boundary,
	Axis axis,
	Side side) const
{
	to.clear();
	bool insidePrev, insideCur;
	uint32_t axisIndex = static_cast<uint32_t>(axis);

	const uint32_t fromSize = static_cast<uint32_t>(from.size());
	if (fromSize < 3) return;

	insidePrev = isInside(from[0].pos[axisIndex], boundary, side);
	for(uint32_t indexPrev = 0; indexPrev < fromSize; ++indexPrev) {
		const uint32_t indexCur = (indexPrev + 1) % fromSize;
		insideCur = isInside(from[indexCur].pos[axisIndex], boundary, side);

		if(insidePrev) {
			if(insideCur) { // prev in & cur in
				to.push_back(from[indexPrev]);
			}
			else { // prev in & cur out
				to.push_back(from[indexPrev]);

				if(from[indexPrev].pos[axisIndex] != boundary) {
					const float t = std::abs(from[indexPrev].pos[axisIndex] - boundary) / 
						std::abs(from[indexPrev].pos[axisIndex] - from[indexCur].pos[axisIndex]);
					to.push_back(interpolateVertex(from[indexPrev], from[indexCur], t));
				}

			}
		}
		else {
			if(insideCur) { // prev out & cur in
				if(from[indexCur].pos[axisIndex] != boundary) {
					const float t = std::abs(from[indexPrev].pos[axisIndex] - boundary) / 
						std::abs(from[indexPrev].pos[axisIndex] - from[indexCur].pos[axisIndex]);
					to.push_back(interpolateVertex(from[indexPrev], from[indexCur], t));
				}
			}
			else { // prev out & cur out
				// do nothing
			}
		}

		insidePrev = insideCur;
	}
}

scene::Polygon scene::ScenePartitioner::clipTriangleAgainstAabb(
	const Triangle& triangle,
	const Aabb& bounds) const
{
	Polygon pg[2];
	pg[0].reserve(9);
	pg[1].reserve(9);

	for(uint32_t i = 0; i < 3; ++i)
		pg[0].push_back(triangle[i]);

	clipPolygonAgainstPlane(pg[0], pg[1], bounds.min.x, Axis::X, Side::Min);
	clipPolygonAgainstPlane(pg[1], pg[0], bounds.max.x, Axis::X, Side::Max);

	clipPolygonAgainstPlane(pg[0], pg[1], bounds.min.y, Axis::Y, Side::Min);
	clipPolygonAgainstPlane(pg[1], pg[0], bounds.max.y, Axis::Y, Side::Max);

	clipPolygonAgainstPlane(pg[0], pg[1], bounds.min.z, Axis::Z, Side::Min);
	clipPolygonAgainstPlane(pg[1], pg[0], bounds.max.z, Axis::Z, Side::Max);

	return pg[0];
}

bool scene::ScenePartitioner::isInside(
	float coord,
	float boundary,
	Side side) const
{
	return side == Side::Min ? coord >= boundary : coord <= boundary;
}

vkglTF::Vertex scene::ScenePartitioner::interpolateVertex(
	const vkglTF::Vertex& from,
	const vkglTF::Vertex& to,
	float t) const
{
	if(t <= 0.0f) {
		return from;
	}
	else if(t >= 1.0f) {
		return to;
	}

	vkglTF::Vertex result{};
	result.pos = glm::mix(from.pos, to.pos, t);
	// Preserve the original linear attribute field; unpackTriangle normalizes after interpolation.
	result.normal = glm::mix(from.normal, to.normal, t);
	result.uv = glm::mix(from.uv, to.uv, t);
	result.color = glm::mix(from.color, to.color, t);
	result.joint0 = glm::mix(from.joint0, to.joint0, t);
	result.weight0 = glm::mix(from.weight0, to.weight0, t);
	result.tangent = glm::mix(from.tangent, to.tangent, t);

	return result;
}
