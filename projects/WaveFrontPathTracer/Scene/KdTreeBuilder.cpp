#include "KdTreeBuilder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

scene::KdTreeBuilder::Split scene::KdTreeBuilder::findSplit(
    const std::vector<TriangleInfo>& triangles, const Aabb& bounds,
    float costRatio, int fixedAxis) const
{
    // Divide SAH by CI: leaf cost = N, split cost = CT/CI + PL*NL + PR*NR.
    Split best{double(triangles.size())};
    const glm::dvec3 extent = glm::dvec3(bounds.max) - glm::dvec3(bounds.min);
    const double area = extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
    if (area <= 0.0) return best;

    enum class EdgeType { End, Planar, Start };
    struct Edge { float position; EdgeType type; };
    std::vector<Edge> edges;
    edges.reserve(triangles.size() * 2);
    for (int axis = 0; axis < 3; ++axis) {
        if (fixedAxis >= 0 && axis != fixedAxis) continue;
        edges.clear();
        for (const auto& triangle : triangles) {
            const float min = triangle.bounds.min[axis], max = triangle.bounds.max[axis];
            if (min == max) edges.push_back({min, EdgeType::Planar});
            else {
                edges.push_back({min, EdgeType::Start});
                edges.push_back({max, EdgeType::End});
            }
        }
        std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) { return a.position < b.position; });
        size_t left = 0, right = triangles.size();
        const double otherExtent = extent[(axis + 1) % 3] + extent[(axis + 2) % 3];
        const double otherArea = extent[(axis + 1) % 3] * extent[(axis + 2) % 3];
        for (size_t i = 0; i < edges.size();) {
            const float position = edges[i].position;
            size_t starts = 0, ends = 0, planar = 0;
            do {
                switch (edges[i].type) {
                case EdgeType::Start: ++starts; break;
                case EdgeType::End: ++ends; break;
                case EdgeType::Planar: ++planar; break;
                }
                ++i;
            } while (i < edges.size() && edges[i].position == position);
            right -= ends + planar;
            // Match the reference try_to_split boundary exclusions and sweep termination.
            if (position <= bounds.min[axis] + EPSILON) {
                left += starts + planar;
                continue;
            }
            if (position >= bounds.max[axis] - EPSILON) break;
            {
                const double probLeft = ((double(position) - bounds.min[axis]) * otherExtent + otherArea) / area;
                const double probRight = ((bounds.max[axis] - double(position)) * otherExtent + otherArea) / area;
                // Like the reference builder, assign planar triangles to the cheaper child.
                for (bool planarLeft : {true, false}) {
                    const size_t countLeft = left + (planarLeft ? planar : 0);
                    const size_t countRight = right + (planarLeft ? 0 : planar);
                    const double bonus = countLeft == 0 || countRight == 0 ? EMPTY_BONUS : 1.0;
                    const double cost = costRatio + (probLeft * countLeft + probRight * countRight) * bonus;
                    if (cost < best.cost) best = {cost, position, axis, planarLeft};
                }
            }
            left += starts + planar;
        }
    }
    return best;
}

std::array<scene::Aabb, 2> scene::KdTreeBuilder::clipTriangle(
    const vkglTF::Model& scene, const TriangleInfo& triangle, const Split& split) const
{
    // Reference clip_triangle: original vertices, one split plane, then previous bounds.
    glm::vec3 points[2][4];
    int counts[2]{};
    for (uint32_t corner = 0; corner < 3; ++corner) {
        const auto& point = scene.vertexBuffer[scene.indexBuffer[triangle.firstIndex + corner]].pos;
        const int side = point[split.axis] < split.position ? 0 : 1;
        points[side][counts[side]++] = point;
    }
    const int leftSize = counts[0], rightSize = counts[1];
    for (int left = 0; left < leftSize; ++left) {
        for (int right = 0; right < rightSize; ++right) {
            const glm::vec3 edge = points[1][right] - points[0][left];
            const float denominator = edge[split.axis];
            if (std::abs(denominator) < EPSILON) continue;
            const float t = -(points[0][left][split.axis] - split.position) / denominator;
            if (t < 0.0f || t > 1.0f) continue;
            const glm::vec3 hit = points[0][left] + edge * t;
            points[0][counts[0]++] = hit;
            points[1][counts[1]++] = hit;
        }
    }
    std::array<Aabb, 2> clipped;
    for (int side = 0; side < 2; ++side) {
        clipped[side] = {points[side][0], points[side][0]};
        for (int i = 1; i < counts[side]; ++i) {
            clipped[side].min = glm::min(clipped[side].min, points[side][i]);
            clipped[side].max = glm::max(clipped[side].max, points[side][i]);
        }
        // Keep the reference even if epsilon rejected intersections and only 1-2 points remain.
        clipped[side].min = glm::max(clipped[side].min, triangle.bounds.min);
        clipped[side].max = glm::min(clipped[side].max, triangle.bounds.max);
    }
    return clipped;
}

void scene::KdTreeBuilder::buildRecursive(const vkglTF::Model& scene,
    std::vector<TriangleInfo> triangles, const Aabb& bounds, float costRatio,
    int fixedAxis, uint32_t depth, std::vector<Aabb>& leaves) const
{
    // The reference axis-only AdaptiveGrid splitter stops on SAH, not MIN_TRIANGLES.
    const Split split = depth < MAX_DEPTH && triangles.size() > (fixedAxis < 0 ? MIN_TRIANGLES : 0)
        ? findSplit(triangles, bounds, costRatio, fixedAxis) : Split{double(triangles.size())};
    if (split.axis < 0) {
        // Keep empty spatial leaves too: final clipping has its own boundary ownership rule.
        leaves.push_back(bounds);
        return;
    }
    Aabb childBounds[2]{bounds, bounds};
    childBounds[0].max[split.axis] = split.position;
    childBounds[1].min[split.axis] = split.position;
    std::vector<TriangleInfo> children[2];
    for (const auto& triangle : triangles) {
        const float min = triangle.bounds.min[split.axis], max = triangle.bounds.max[split.axis];
        const bool planar = min == split.position && max == split.position;
        const bool belongs[2]{min < split.position || (planar && split.planarLeft),
                              max > split.position || (planar && !split.planarLeft)};
        const auto clippedBounds = min < split.position && max > split.position
            ? clipTriangle(scene, triangle, split) : std::array<Aabb, 2>{triangle.bounds, triangle.bounds};
        for (int side = 0; side < 2; ++side) {
            if (!belongs[side]) continue;
            children[side].push_back({triangle.firstIndex, clippedBounds[side]});
        }
    }
    // Parent references are no longer needed while descending.
    std::vector<TriangleInfo>{}.swap(triangles);
    buildRecursive(scene, std::move(children[0]), childBounds[0], costRatio, fixedAxis, depth + 1, leaves);
    buildRecursive(scene, std::move(children[1]), childBounds[1], costRatio, fixedAxis, depth + 1, leaves);
}

std::vector<scene::Aabb> scene::KdTreeBuilder::build(const vkglTF::Model& scene,
    float costRatio, Mode mode) const
{
    if (!std::isfinite(costRatio) || costRatio <= 0.0f)
        throw std::invalid_argument("KdTreeBuilder requires a positive finite CT/CI ratio");
    if (mode != Mode::Leaves && mode != Mode::AdaptiveGrid)
        throw std::invalid_argument("Unknown KdTreeBuilder mode");
    if (scene.indexBuffer.empty() || scene.indexBuffer.size() % 3 || scene.vertexBuffer.empty() ||
        scene.indexBuffer.size() > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("KdTreeBuilder requires CPU indexed triangles");
    const Aabb root{scene.dimensions.min, scene.dimensions.max};
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(root.min[axis]) || !std::isfinite(root.max[axis]) || root.min[axis] > root.max[axis])
            throw std::invalid_argument("Invalid KdTreeBuilder scene bounds");

    std::vector<TriangleInfo> triangles;
    triangles.reserve(scene.indexBuffer.size() / 3);
    for (uint32_t i = 0; i < scene.indexBuffer.size(); i += 3) {
        TriangleInfo triangle{i, {glm::vec3(std::numeric_limits<float>::max()), glm::vec3(-std::numeric_limits<float>::max())}};
        for (uint32_t corner = 0; corner < 3; ++corner) {
            const uint32_t index = scene.indexBuffer[i + corner];
            if (index >= scene.vertexBuffer.size()) throw std::invalid_argument("Triangle vertex index is out of range");
            const auto& position = scene.vertexBuffer[index].pos;
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(position[axis]) || position[axis] < root.min[axis] || position[axis] > root.max[axis])
                    throw std::invalid_argument("CPU vertices must lie inside scene bounds");
            triangle.bounds.min = glm::min(triangle.bounds.min, position);
            triangle.bounds.max = glm::max(triangle.bounds.max, position);
        }
        triangles.push_back(triangle);
    }
    std::vector<Aabb> cells;
    if (mode == Mode::Leaves) {
        buildRecursive(scene, std::move(triangles), root, costRatio, -1, 0, cells);
        return cells;
    }

    // Reference AdaptiveGrid: independent, axis-constrained SAH trees, then Cartesian product.
    std::vector<float> splits[3];
    size_t cellCount = 1;
    for (int axis = 0; axis < 3; ++axis) {
        std::vector<Aabb> leaves;
        buildRecursive(scene, triangles, root, costRatio, axis, 0, leaves);
        splits[axis].push_back(root.min[axis]);
        for (const auto& leaf : leaves) splits[axis].push_back(leaf.max[axis]);
        std::sort(splits[axis].begin(), splits[axis].end());
        splits[axis].erase(std::unique(splits[axis].begin(), splits[axis].end()), splits[axis].end());
        if (splits[axis].size() == 1) splits[axis].push_back(root.max[axis]); // Flat axis.
        const size_t count = splits[axis].size() - 1;
        if (cellCount > cells.max_size() / count) throw std::overflow_error("AdaptiveGrid cell count is too large");
        cellCount *= count;
    }
    cells.reserve(cellCount);
    for (size_t z = 0; z + 1 < splits[2].size(); ++z)
        for (size_t y = 0; y + 1 < splits[1].size(); ++y)
            for (size_t x = 0; x + 1 < splits[0].size(); ++x)
                cells.push_back({{splits[0][x], splits[1][y], splits[2][z]},
                                 {splits[0][x + 1], splits[1][y + 1], splits[2][z + 1]}});
    return cells;
}
