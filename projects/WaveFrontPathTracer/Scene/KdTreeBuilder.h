#pragma once

#include "ScenePartitioner.h"

namespace scene
{
    // CPU-only SAH spatial subdivision. Final triangle clipping/upload stays in ScenePartitioner.
    class KdTreeBuilder
    {
    public:
        enum class Mode { Leaves, AdaptiveGrid };

        std::vector<Aabb> build(const vkglTF::Model& scene, float costRatio,
            Mode mode = Mode::Leaves) const;

    private:
        struct TriangleInfo
        {
            uint32_t firstIndex;
            Aabb bounds;
        };

        struct Split
        {
            double cost;
            float position = 0.0f;
            int axis = -1;
            bool planarLeft = true;
        };

        static constexpr uint32_t MAX_DEPTH = 64;
        static constexpr uint32_t MIN_TRIANGLES = 100;
        static constexpr double EMPTY_BONUS = 0.9;
        static constexpr float EPSILON = 0.00001f;

        Split findSplit(const std::vector<TriangleInfo>& triangles, const Aabb& bounds,
            float costRatio, int fixedAxis) const;
        std::array<Aabb, 2> clipTriangle(const vkglTF::Model& scene,
            const TriangleInfo& triangle, const Split& split) const;
        void buildRecursive(const vkglTF::Model& scene, std::vector<TriangleInfo> triangles,
            const Aabb& bounds, float costRatio, int fixedAxis, uint32_t depth,
            std::vector<Aabb>& leaves) const;
    };
}
