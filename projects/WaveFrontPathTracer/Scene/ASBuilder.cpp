#include "ASBuilder.h"
#include "../Utils/BufferUtils.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr VkTransformMatrixKHR identityTransform = {
        {{1.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 1.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 1.0f, 0.0f}}
    };
    constexpr VkBufferUsageFlags buildInputUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    constexpr VkMemoryPropertyFlags hostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    struct ScopedBuffer : vks::Buffer
    {
        ~ScopedBuffer() { destroy(); }
    };
}

scene::ASBuilder::~ASBuilder()
{
    if (device) {
        vkDeviceWaitIdle(device->logicalDevice);
        clear();
        transformBuffer.destroy();
    }
}

void scene::ASBuilder::init(vks::VulkanDevice& device, VkQueue queue)
{
    if (this->device || !device.logicalDevice || !queue)
        throw std::logic_error("ASBuilder::init requires a valid device/queue and may only be called once");

    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device.logicalDevice, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device.logicalDevice, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device.logicalDevice, "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device.logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device.logicalDevice, "vkCmdBuildAccelerationStructuresKHR"));
    if (!vkCreateAccelerationStructureKHR || !vkDestroyAccelerationStructureKHR ||
        !vkGetAccelerationStructureBuildSizesKHR || !vkGetAccelerationStructureDeviceAddressKHR ||
        !vkCmdBuildAccelerationStructuresKHR)
        throw std::runtime_error("VK_KHR_acceleration_structure is not enabled");

    this->device = &device;
    this->queue = queue;
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    deviceProperties.pNext = &properties;
    vkGetPhysicalDeviceProperties2(device.physicalDevice, &deviceProperties);

    auto transform = identityTransform;
    VK_CHECK_RESULT(device.createBuffer(buildInputUsage, hostMemory, &transformBuffer,
        sizeof(transform), &transform));
    transformBuffer.deviceAddress = vks::util::getBufferDeviceAddress(device.logicalDevice, transformBuffer.buffer);
}

void scene::ASBuilder::build(vkglTF::Model& model, BuildFlags flags)
{
    if (!device) throw std::logic_error("Call ASBuilder::init before build");
    if (flags != BuildFlags::Default && flags != BuildFlags::Partitioned)
        throw std::invalid_argument("Unknown ASBuilder build flag");
    if (model.device != device) throw std::invalid_argument("Model and ASBuilder must use the same device");

    // Old handles may still be referenced by rendering commands. Getters change after rebuilding.
    VK_CHECK_RESULT(vkDeviceWaitIdle(device->logicalDevice));
    clear();
    try {
        if (flags == BuildFlags::Partitioned)
            CreatePartitionedBottomLevelAccelerationStructure(model);
        else
            CreateBottomLevelAccelerationStructure(model);
        CreateTopLevelAccelerationStructure();
    } catch (...) {
        clear();
        throw;
    }
}

void scene::ASBuilder::CreatePartitionedBottomLevelAccelerationStructure(vkglTF::Model& model)
{
    if (model.vertexBuffer.empty() || model.indexBuffer.empty())
        throw std::invalid_argument("Partitioned mode requires Model loaded with KeepCpuGeometry");
    if ((vkglTF::memoryPropertyFlags & buildInputUsage) != buildInputUsage)
        throw std::invalid_argument("Partitioned geometry buffers require device-address and AS-build usage flags");

    // Consume the owning return value directly; ScenePartitioner must return a valid owning result.
    partitionedScene.reset(new PartitionedScene(ScenePartitioner{}.partition(model, *device, queue)));
    CreateBottomLevelAccelerationStructure(model, partitionedScene.get());
}

void scene::ASBuilder::CreateBottomLevelAccelerationStructure(vkglTF::Model& model,
    const PartitionedScene* partitioned)
{
    static_assert(sizeof(GeometryNode) == 64 && offsetof(GeometryNode, vertexBufferDeviceAddress) == 16 &&
        offsetof(GeometryNode, metallicFactor) == 48, "GeometryNode must match the shader layout");
    const VkBuffer vertexBuffer = partitioned ? partitioned->vertices.buffer : model.vertices.buffer;
    const VkBuffer indexBuffer = partitioned ? partitioned->indices.buffer : model.indices.buffer;
    const int vertexCount = partitioned ? partitioned->vertices.count : model.vertices.count;
    const int indexCount = partitioned ? partitioned->indices.count : model.indices.count;
    if (!vertexBuffer || !indexBuffer || vertexCount <= 0 || indexCount <= 0)
        throw std::invalid_argument("AS build requires nonempty GPU vertex/index buffers");
    const auto vertexAddress = vks::util::getBufferDeviceAddress(device->logicalDevice, vertexBuffer);
    const auto indexAddress = vks::util::getBufferDeviceAddress(device->logicalDevice, indexBuffer);

    struct BlasBuild
    {
        std::vector<VkAccelerationStructureGeometryKHR> geometries;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
        std::vector<uint32_t> primitiveCounts;
        uint32_t geometryBase;
    };
    std::vector<BlasBuild> builds;
    std::vector<GeometryNode> geometryNodes;
    std::vector<VkTransformMatrixKHR> nodeTransforms;

    auto appendGeometry = [&](BlasBuild& build, const vkglTF::Primitive& primitive) {
        if (primitive.indexCount == 0) return;
        if (primitive.indexCount % 3 || uint64_t(primitive.firstIndex) + primitive.indexCount > uint64_t(indexCount))
            throw std::invalid_argument("Invalid triangle primitive index range");
        const auto geometryIndexAddress = indexAddress + VkDeviceSize(primitive.firstIndex) * sizeof(uint32_t);
        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        auto& triangles = geometry.geometry.triangles;
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress = vertexAddress;
        triangles.vertexStride = sizeof(vkglTF::Vertex);
        triangles.maxVertex = static_cast<uint32_t>(vertexCount - 1);
        triangles.indexType = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress = geometryIndexAddress;
        triangles.transformData.deviceAddress = transformBuffer.deviceAddress;
        build.geometries.push_back(geometry);
        build.ranges.push_back({primitive.indexCount / 3, 0, 0, 0});
        build.primitiveCounts.push_back(primitive.indexCount / 3);

        const auto& material = primitive.material;
        GeometryNode node{};
        node.vertexBufferDeviceAddress = vertexAddress;
        node.indexBufferDeviceAddress = geometryIndexAddress;
        node.baseColorFactor = material.baseColorFactor;
        node.textureIndexBaseColor = material.baseColorTexture ? material.baseColorTexture->index : -1;
        node.textureIndexNormal = material.normalTexture ? material.normalTexture->index : -1;
        node.textureIndexMetallicRoughness = material.metallicRoughnessTexture ? material.metallicRoughnessTexture->index : -1;
        node.textureIndexEmissive = material.emissiveTexture ? material.emissiveTexture->index : -1;
        node.metallicFactor = material.metallicFactor;
        node.roughnessFactor = material.roughnessFactor;
        geometryNodes.push_back(node);
    };

    if (partitioned) {
        // Prefix bases and geometry metadata are generated in precisely the BLAS geometry order.
        for (const auto& cell : partitioned->cells) {
            if (geometryNodes.size() > 0xffffffu)
                throw std::overflow_error("TLAS instanceCustomIndex exceeds its 24-bit limit");
            BlasBuild build{{}, {}, {}, static_cast<uint32_t>(geometryNodes.size())};
            for (const auto& primitive : cell) appendGeometry(build, primitive);
            if (!build.geometries.empty()) builds.push_back(std::move(build));
        }
    } else {
        BlasBuild build{{}, {}, {}, 0};
        for (auto node : model.linearNodes) {
            if (!node->mesh) continue;
            const auto matrix = glm::mat3x4(glm::transpose(node->getMatrix()));
            VkTransformMatrixKHR transform{};
            std::memcpy(&transform, &matrix, sizeof(transform));
            for (auto primitive : node->mesh->primitives) {
                if (!primitive->indexCount) continue;
                appendGeometry(build, *primitive);
                nodeTransforms.push_back(transform);
            }
        }
        if (!build.geometries.empty()) builds.push_back(std::move(build));
    }
    if (builds.empty()) throw std::invalid_argument("Scene contains no indexed triangle geometries");
    if (builds.size() > properties.maxInstanceCount)
        throw std::invalid_argument("BLAS count exceeds the TLAS instance limit");

    ScopedBuffer nodeTransformBuffer{};
    if (!nodeTransforms.empty()) {
        VK_CHECK_RESULT(device->createBuffer(buildInputUsage, hostMemory, &nodeTransformBuffer,
            nodeTransforms.size() * sizeof(VkTransformMatrixKHR), nodeTransforms.data()));
        const auto address = vks::util::getBufferDeviceAddress(device->logicalDevice, nodeTransformBuffer.buffer);
        for (size_t i = 0; i < builds[0].geometries.size(); ++i)
            builds[0].geometries[i].geometry.triangles.transformData.deviceAddress = address + i * sizeof(VkTransformMatrixKHR);
    }

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(builds.size());
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> rangePointers(builds.size());
    bottomLevelAS.resize(builds.size());
    VkDeviceSize scratchSize = 0;
    const VkDeviceSize alignment = properties.minAccelerationStructureScratchOffsetAlignment;
    for (size_t i = 0; i < builds.size(); ++i) {
        auto& input = builds[i];
        uint64_t triangleCount = 0;
        for (auto count : input.primitiveCounts) triangleCount += count;
        if (input.geometries.size() > properties.maxGeometryCount || triangleCount > properties.maxPrimitiveCount) {
            throw std::invalid_argument("BLAS geometry/primitive count exceeds device limits");
        }
        auto& info = buildInfos[i];
        info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        info.geometryCount = static_cast<uint32_t>(input.geometries.size());
        info.pGeometries = input.geometries.data();
        VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device->logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &info, input.primitiveCounts.data(), &sizes);
        createAccelerationStructure(bottomLevelAS[i], info.type, sizes);
        bottomLevelAS[i].geometryBase = input.geometryBase;
        info.dstAccelerationStructure = bottomLevelAS[i].handle;
        scratchSize = (scratchSize + alignment - 1) / alignment * alignment;
        info.scratchData.deviceAddress = scratchSize; // Offset until the buffer is allocated.
        scratchSize += sizes.buildScratchSize;
        rangePointers[i] = input.ranges.data();
    }
    scratchBuffer = createScratchBuffer(scratchSize);
    for (auto& info : buildInfos) info.scratchData.deviceAddress += scratchBuffer.deviceAddress;

    VkCommandBuffer command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    vkCmdBuildAccelerationStructuresKHR(command, static_cast<uint32_t>(buildInfos.size()),
        buildInfos.data(), rangePointers.data());
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    device->flushCommandBuffer(command, queue);
    deleteScratchBuffer(scratchBuffer);
    nodeTransformBuffer.destroy();

    ScopedBuffer staging{};
    const VkDeviceSize nodeBufferSize = geometryNodes.size() * sizeof(GeometryNode);
    VK_CHECK_RESULT(device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, hostMemory,
        &staging, nodeBufferSize, geometryNodes.data()));
    VK_CHECK_RESULT(device->createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &geometryNodeBuffer, nodeBufferSize));
    device->copyBuffer(&staging, &geometryNodeBuffer, queue);
    geometryNodeBuffer.deviceAddress = vks::util::getBufferDeviceAddress(device->logicalDevice, geometryNodeBuffer.buffer);
}

void scene::ASBuilder::CreateTopLevelAccelerationStructure()
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(bottomLevelAS.size());
    for (const auto& blas : bottomLevelAS) {
        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = identityTransform;
        instance.instanceCustomIndex = blas.geometryBase;
        instance.mask = 0xff;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = blas.deviceAddress;
        instances.push_back(instance);
    }
    ScopedBuffer instanceBuffer{};
    VK_CHECK_RESULT(device->createBuffer(buildInputUsage, hostMemory, &instanceBuffer,
        instances.size() * sizeof(VkAccelerationStructureInstanceKHR), instances.data()));
    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress = vks::util::getBufferDeviceAddress(device->logicalDevice, instanceBuffer.buffer);

    VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.geometryCount = 1;
    info.pGeometries = &geometry;
    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = static_cast<uint32_t>(instances.size());
    const auto* rangePointer = &range;
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device->logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &info, &range.primitiveCount, &sizes);
    createAccelerationStructure(topLevelAS, info.type, sizes);
    scratchBuffer = createScratchBuffer(sizes.buildScratchSize);
    info.dstAccelerationStructure = topLevelAS.handle;
    info.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkCommandBuffer command = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBuildAccelerationStructuresKHR(command, 1, &info, &rangePointer);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    device->flushCommandBuffer(command, queue);
    deleteScratchBuffer(scratchBuffer);
}

scene::ASBuilder::ScratchBuffer scene::ASBuilder::createScratchBuffer(VkDeviceSize size)
{
    ScratchBuffer scratch{};
    const VkDeviceSize alignment = properties.minAccelerationStructureScratchOffsetAlignment;
    VK_CHECK_RESULT(device->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, size + alignment - 1, &scratch.handle, &scratch.memory));
    const auto address = vks::util::getBufferDeviceAddress(device->logicalDevice, scratch.handle);
    scratch.deviceAddress = (address + alignment - 1) / alignment * alignment;
    return scratch;
}

void scene::ASBuilder::deleteScratchBuffer(ScratchBuffer& scratch)
{
    if (scratch.handle) vkDestroyBuffer(device->logicalDevice, scratch.handle, nullptr);
    if (scratch.memory) vkFreeMemory(device->logicalDevice, scratch.memory, nullptr);
    scratch = {};
}

void scene::ASBuilder::createAccelerationStructure(AccelerationStructure& as,
    VkAccelerationStructureTypeKHR type, const VkAccelerationStructureBuildSizesInfoKHR& sizes)
{
    VK_CHECK_RESULT(device->createBuffer(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        sizes.accelerationStructureSize, &as.buffer, &as.memory));
    VkAccelerationStructureCreateInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    info.buffer = as.buffer;
    info.size = sizes.accelerationStructureSize;
    info.type = type;
    VK_CHECK_RESULT(vkCreateAccelerationStructureKHR(device->logicalDevice, &info, nullptr, &as.handle));
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = as.handle;
    as.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device->logicalDevice, &addressInfo);
}

void scene::ASBuilder::deleteAccelerationStructure(AccelerationStructure& as)
{
    if (as.handle) vkDestroyAccelerationStructureKHR(device->logicalDevice, as.handle, nullptr);
    if (as.buffer) vkDestroyBuffer(device->logicalDevice, as.buffer, nullptr);
    if (as.memory) vkFreeMemory(device->logicalDevice, as.memory, nullptr);
    as = {};
}

void scene::ASBuilder::clear()
{
    deleteAccelerationStructure(topLevelAS);
    for (auto& blas : bottomLevelAS) deleteAccelerationStructure(blas);
    bottomLevelAS.clear();
    deleteScratchBuffer(scratchBuffer);
    geometryNodeBuffer.destroy();
    geometryNodeBuffer.deviceAddress = 0;
    partitionedScene.reset();
}

