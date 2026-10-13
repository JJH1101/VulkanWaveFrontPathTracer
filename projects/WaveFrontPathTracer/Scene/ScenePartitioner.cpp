#include "ScenePartitioner.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

std::vector<scene::Aabb> scene::ScenePartitioner::createUniformGrid(const vkglTF::Model& scene, float cellWeight) const
{
	if (!std::isfinite(cellWeight) || cellWeight <= 0.0f)
		throw std::invalid_argument("UniformGrid requires a positive finite cell weight");
	const auto& bounds = scene.dimensions;
	const glm::vec3 sceneSize = bounds.max - bounds.min;
	const double sceneVolume = double(sceneSize.x) * sceneSize.y * sceneSize.z;
	// Flat scenes use one cell; volumetric scenes retain the existing density heuristic.
	const double density = sceneVolume > 0.0 ? std::cbrt((scene.indexBuffer.size() / 3) / sceneVolume) * cellWeight : 0.0;
	glm::uvec3 numCells;
	size_t cellCount = 1;
	for (uint32_t axis = 0; axis < 3; ++axis) {
		const double count = std::max(double(sceneSize[axis]) * density + 0.5, 1.0);
		if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) || sceneSize[axis] < 0.0f ||
			!std::isfinite(count) || count > std::numeric_limits<uint32_t>::max())
			throw std::invalid_argument("Invalid UniformGrid bounds or cell count");
		numCells[axis] = static_cast<uint32_t>(count);
		if (cellCount > std::vector<Aabb>{}.max_size() / numCells[axis])
			throw std::overflow_error("UniformGrid cell count is too large");
		cellCount *= numCells[axis];
	}
	const glm::vec3 cellSize = sceneSize / glm::vec3(numCells);
	std::vector<Aabb> cellBounds(cellCount);
	auto calcIdx = [numCells](glm::uvec3 idx)->size_t {return size_t(numCells.x) * numCells.y * idx.z + size_t(numCells.x) * idx.y + idx.x; };
	for(uint32_t z = 0; z < numCells.z; ++z) {
		for(uint32_t y = 0; y < numCells.y; ++y) {
			for(uint32_t x = 0; x < numCells.x; ++x) {
				const size_t idx = calcIdx(glm::uvec3(x, y, z));
				cellBounds[idx].min = bounds.min + glm::vec3(x, y, z) * cellSize;
				// Evaluate shared boundaries identically; pin the outer faces to scene bounds.
				for (uint32_t axis = 0; axis < 3; ++axis) {
					const uint32_t next = glm::uvec3(x, y, z)[axis] + 1;
					cellBounds[idx].max[axis] = next == numCells[axis] ? bounds.max[axis]
						: bounds.min[axis] + float(next) * cellSize[axis];
				}
			}
		}
	}
	return cellBounds;
}

scene::PartitionedScene scene::ScenePartitioner::partition(vkglTF::Model& scene,
	vks::VulkanDevice& device, VkQueue transferQueue, const std::vector<Aabb>& cellBounds) const
{
	PartitionedScene partitionedScene(device);
	if (scene.indexBuffer.empty() || scene.indexBuffer.size() % 3 || scene.vertexBuffer.empty() || cellBounds.empty())
		throw std::invalid_argument("Partitioning requires CPU triangles and cell bounds");

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

							const auto triangulation = triangulateClippedPolygon(clippedPolygon);
							for (uint32_t j = 0; j < triangulation.count; ++j) {
								for (uint32_t corner : triangulation.triangles[j]) {
									const auto& vertex = clippedPolygon[corner];
									auto [entry, inserted] = uniqueVertices.try_emplace(vertex,
										static_cast<uint32_t>(partitionedVertices.size()));
									if (inserted) partitionedVertices.push_back(vertex);
									partitionedIndices.push_back(entry->second);
								}
							}
							indexCount += triangulation.count * 3;
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

scene::ClippedPolygonTriangulation scene::ScenePartitioner::triangulateClippedPolygon(
    const Polygon& polygon) const
{
    ClippedPolygonTriangulation result;
    constexpr uint32_t capacity = ClippedPolygonTriangulation::maxVertices;
    std::array<uint32_t, capacity> boundary{};
    uint32_t count = 0;
    for (uint32_t i = 0; i < polygon.size(); ++i) {
        // Remove only exact duplicate boundary points; never collapse a thin valid region.
        if (count && polygon[boundary[count - 1]].pos == polygon[i].pos) continue;
        if (i + 1 == polygon.size() && count && polygon[boundary[0]].pos == polygon[i].pos) continue;
        if (count == capacity) throw std::invalid_argument("Clipped polygon exceeds nine distinct vertices");
        boundary[count++] = i;
    }
    if (count < 3) return result;

    if (count == 3) {
        const glm::dvec3 a = glm::dvec3(polygon[boundary[1]].pos) - glm::dvec3(polygon[boundary[0]].pos);
        const glm::dvec3 b = glm::dvec3(polygon[boundary[2]].pos) - glm::dvec3(polygon[boundary[0]].pos);
        const glm::dvec3 cross = glm::cross(a, b);
        if (glm::dot(cross, cross) > 0.0)
            result.triangles[result.count++] = {boundary[0], boundary[1], boundary[2]};
        return result;
    }

    std::array<glm::dvec3, capacity> points{};
    const glm::dvec3 origin(polygon[boundary[0]].pos);
    for (uint32_t i = 0; i < count; ++i)
        points[i] = glm::dvec3(polygon[boundary[i]].pos) - origin;
    glm::dvec3 normal(0.0);
    for (uint32_t i = 1; i + 1 < count; ++i)
        normal += glm::cross(points[i], points[i + 1]);
    if (glm::dot(normal, normal) == 0.0) return result;

    auto quality = [&](uint32_t i, uint32_t k, uint32_t j) {
        const glm::dvec3 a = points[k] - points[i], b = points[j] - points[i];
        const glm::dvec3 cross = glm::cross(a, b);
        const double orientation = glm::dot(cross, normal);
        if (orientation < 0.0) return -1.0; // Preserve the input winding.
        const double areaSquared = glm::dot(cross, cross);
        if (areaSquared == 0.0) return 0.0;
        const double ab = glm::dot(a, a), ac = glm::dot(b, b);
        const glm::dvec3 c = points[j] - points[k];
        const double bc = glm::dot(c, c);
        const double longest = std::max({ab, ac, bc});
        const double secondLongest = std::max({std::min(ab, ac), std::min(ab, bc), std::min(ac, bc)});
        // sin^2(smallest angle), monotonic because that angle is at most 60 degrees.
        // Double precision and a ratio avoid acos and any scene-scale area threshold.
        return areaSquared / (longest * secondLongest);
    };

    if (count == 4) {
        const double q012 = quality(0, 1, 2), q023 = quality(0, 2, 3);
        const double q013 = quality(0, 1, 3), q123 = quality(1, 2, 3);
        if (std::min(q013, q123) > std::min(q012, q023)) {
            if (q013 > 0.0) result.triangles[result.count++] = {boundary[0], boundary[1], boundary[3]};
            if (q123 > 0.0) result.triangles[result.count++] = {boundary[1], boundary[2], boundary[3]};
        }
        else {
            if (q012 > 0.0) result.triangles[result.count++] = {boundary[0], boundary[1], boundary[2]};
            if (q023 > 0.0) result.triangles[result.count++] = {boundary[0], boundary[2], boundary[3]};
        }
        return result;
    }

    // Project onto an orthonormal basis of the polygon plane. Dropping an axis
    // would distort distances and the Delaunay criterion for oblique triangles.
    const glm::dvec3 unitNormal = glm::normalize(normal);
    uint32_t farthest = 1;
    for (uint32_t i = 2; i < count; ++i) {
        if (glm::dot(points[i], points[i]) > glm::dot(points[farthest], points[farthest])) farthest = i;
    }
    const glm::dvec3 axisU = glm::normalize(points[farthest] - unitNormal * glm::dot(points[farthest], unitNormal));
    const glm::dvec3 axisV = glm::cross(unitNormal, axisU);
    std::array<std::array<double, 2>, capacity> projected{};
    for (uint32_t i = 0; i < count; ++i)
        projected[i] = {glm::dot(points[i], axisU), glm::dot(points[i], axisV)};

    auto orientation = [&](uint32_t a, uint32_t b, uint32_t c) {
        const double ax = projected[a][0] - projected[c][0], ay = projected[a][1] - projected[c][1];
        const double bx = projected[b][0] - projected[c][0], by = projected[b][1] - projected[c][1];
        return ax * by - ay * bx;
    };
    auto insideCircle = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        const double ax = projected[a][0] - projected[d][0], ay = projected[a][1] - projected[d][1];
        const double bx = projected[b][0] - projected[d][0], by = projected[b][1] - projected[d][1];
        const double cx = projected[c][0] - projected[d][0], cy = projected[c][1] - projected[d][1];
        const double a2 = ax * ax + ay * ay, b2 = bx * bx + by * by, c2 = cx * cx + cy * cy;
        const double determinant = a2 * (bx * cy - cx * by) + b2 * (cx * ay - ax * cy) + c2 * (ax * by - bx * ay);
        const double magnitude = a2 * (std::abs(bx * cy) + std::abs(cx * by))
            + b2 * (std::abs(cx * ay) + std::abs(ax * cy))
            + c2 * (std::abs(ax * by) + std::abs(bx * ay));
        // Retain the existing diagonal for near-cocircular points. A scale-relative
        // rounding bound avoids unstable flips without collapsing thin triangles.
        return determinant > 32.0 * std::numeric_limits<double>::epsilon() * magnitude;
    };

    // Start with a fan. Each neighbor is an encoded triangle edge (3 * triangle + side),
    // allowing constant-time adjacency updates and a small bit-mask work queue.
    constexpr uint32_t maxTriangles = capacity - 2;
    std::array<std::array<int32_t, 3>, maxTriangles> neighbors{};
    std::array<bool, maxTriangles> positiveArea{};
    uint32_t pending = 0;
    result.count = count - 2;
    for (uint32_t i = 0; i < result.count; ++i) {
        result.triangles[i] = {0, i + 1, i + 2};
        positiveArea[i] = orientation(0, i + 1, i + 2) > 0.0;
        neighbors[i] = {i > 0 ? int32_t(3 * (i - 1) + 2) : -1, -1,
            i + 1 < result.count ? int32_t(3 * (i + 1)) : -1};
        if (i + 1 < result.count) pending |= 1u << (3 * i + 2);
    }
    while (pending) {
        const uint32_t edge = std::countr_zero(pending);
        pending &= pending - 1;
        const uint32_t first = edge / 3, side = edge % 3;
        const int32_t opposite = neighbors[first][side];
        if (opposite < 0) continue; // Boundary edges are never flipped.
        const uint32_t second = uint32_t(opposite) / 3, otherSide = uint32_t(opposite) % 3;
        const uint32_t a = result.triangles[first][side];
        const uint32_t b = result.triangles[first][(side + 1) % 3];
        const uint32_t c = result.triangles[first][(side + 2) % 3];
        const uint32_t d = result.triangles[second][(otherSide + 2) % 3];
        // A collinear initial fan triangle has no circumcircle; test its neighbor instead.
        if (!positiveArea[first] && !positiveArea[second]) continue;
        const bool flip = positiveArea[first] ? insideCircle(a, b, c, d) : insideCircle(b, a, d, c);
        if (!flip) continue;
        // Most edges are already legal. Test convexity only for proposed flips.
        if (orientation(c, d, b) <= 0.0 || orientation(d, c, a) <= 0.0) continue;

        const int32_t bc = neighbors[first][(side + 1) % 3];
        const int32_t ca = neighbors[first][(side + 2) % 3];
        const int32_t ad = neighbors[second][(otherSide + 1) % 3];
        const int32_t db = neighbors[second][(otherSide + 2) % 3];
        result.triangles[first] = {c, d, b};
        result.triangles[second] = {d, c, a};
        positiveArea[first] = positiveArea[second] = true;
        neighbors[first] = {int32_t(3 * second), db, bc};
        neighbors[second] = {int32_t(3 * first), ca, ad};
        for (uint32_t triangle : {first, second}) {
            for (uint32_t s = 1; s < 3; ++s) {
                const int32_t neighbor = neighbors[triangle][s];
                if (neighbor >= 0) {
                    neighbors[uint32_t(neighbor) / 3][uint32_t(neighbor) % 3] = int32_t(3 * triangle + s);
                    pending |= 1u << (3 * triangle + s);
                }
            }
        }
    }
    uint32_t triangleCount = 0;
    for (uint32_t i = 0; i < result.count; ++i) {
        const auto& triangle = result.triangles[i];
        if (positiveArea[i])
            result.triangles[triangleCount++] = {boundary[triangle[0]], boundary[triangle[1]], boundary[triangle[2]]};
    }
    result.count = triangleCount;
    return result;
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
					to.back().pos[axisIndex] = boundary;
				}

			}
		}
		else {
			if(insideCur) { // prev out & cur in
				if(from[indexCur].pos[axisIndex] != boundary) {
					const float t = std::abs(from[indexPrev].pos[axisIndex] - boundary) / 
						std::abs(from[indexPrev].pos[axisIndex] - from[indexCur].pos[axisIndex]);
					to.push_back(interpolateVertex(from[indexPrev], from[indexCur], t));
					to.back().pos[axisIndex] = boundary;
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
