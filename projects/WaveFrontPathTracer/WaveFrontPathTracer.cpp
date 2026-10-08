/*
 * Vulkan Example - Rendering a glTF model using hardware accelerated ray tracing example (for proper transparency, this sample does frame accumulation)
 *
 * Copyright (C) 2023-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"
#define VK_GLTF_MATERIAL_IDS
#include "VulkanglTFModel.h"
#include "Benchmark/Benchmark.h"
#include "Utils/gpuTimer.h"
#include "Utils/BufferUtils.h"
#include "Environment/AppEnvironment.h"
#include "Scene/ASBuilder.h"

#include <stdexcept>

#include <memory>

#if defined(__ANDROID__)
#include "jni.h"

std::string g_envFile = "default.json";

extern "C" JNIEXPORT void JNICALL
Java_sogang_graphics_WaveFrontPathTracer_VulkanActivity_setEnvironmentName(JNIEnv* env, jobject thiz, jstring envName) {
	if (envName == nullptr) return;

	const char* nativeString = env->GetStringUTFChars(envName, nullptr);
	g_envFile = std::string(nativeString);
	env->ReleaseStringUTFChars(envName, nativeString);
}
#endif

class VulkanExample final : public VulkanExampleBase
{
private:
	vks::Buffer pixels;
	vks::Buffer framePixels;

	struct PushConstants {
		uint64_t pixelAddr;
		uint32_t width;
		uint32_t height;
	};

	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	// Destroy Renderer before ASBuilder, and ASBuilder before Model.
	vkglTF::Model model;
	scene::ASBuilder asBuilder;
	scene::ASBuilder::BuildFlags buildFlags = scene::ASBuilder::BuildFlags::Default;
	GPUTimer timer;
	Renderer renderer;

	double renderKernelTimeAccumulator = 0.0;
	uint32_t renderKernelFPS = 0;
	uint32_t renderFrameCounter = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastRenderTimestamp;

	std::string mode = "interactive";
	std::unique_ptr<Benchmark> benchmark;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};
	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures{};
	VkPhysicalDeviceHostQueryResetFeaturesEXT physicalDeviceHostQueryResetFeatures{};
	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddressFeatures{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};

	void createPipelines() {
		// Pipeline layout.
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstants), 0);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		
		// Pipelines.
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCI.pVertexInputState = &emptyInputState;

		shaderStages[0] = loadShader(getShadersPath() + "WaveFrontPathTracer/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "WaveFrontPathTracer/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}
	/*
		If the window has been resized, we need to recreate the storage image and it's descriptor
	*/
	void handleResize()
	{
		renderer.resetFrameIndex();
		resized = false;
	}

	void getEnabledFeatures()
	{
		// New Features using VkPhysicalDeviceFeatures2 structure.
		// Enable feature required for time stamp command pool reset.
		physicalDeviceHostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
		physicalDeviceHostQueryResetFeatures.hostQueryReset = VK_TRUE;

		// Enable feature required for ray query.
		enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		enabledRayQueryFeatures.rayQuery = VK_TRUE;
		enabledRayQueryFeatures.pNext = &physicalDeviceHostQueryResetFeatures;

		// Enable features required for ray tracing using feature chaining via pNext		
		enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
		enabledBufferDeviceAddressFeatures.pNext = &enabledRayQueryFeatures;

		enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
		enabledAccelerationStructureFeatures.pNext = &enabledBufferDeviceAddressFeatures;

		physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.pNext = &enabledAccelerationStructureFeatures;

		deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;

		// Original Features using VkPhysicalDeviceFeature structure.
		enabledFeatures.shaderInt64 = VK_TRUE;	// Buffer device address requires the 64-bit integer feature to be enabled
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	}

	void loadAssets()
	{
		vkglTF::memoryPropertyFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		const uint32_t fileLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices;

		std::string sceneFile;
		Environment::getInstance()->getStringValue("Scene.filename", sceneFile);
		// Cell bounds and partitioned BLASes use world-space CPU vertices.
		const uint32_t loadingFlags = buildFlags == scene::ASBuilder::BuildFlags::Partitioned
			? fileLoadingFlags | vkglTF::FileLoadingFlags::KeepCpuGeometry
			: fileLoadingFlags;
		model.loadFromFile(getAssetPath() + sceneFile, vulkanDevice, queue, loadingFlags);
		if (buildFlags == scene::ASBuilder::BuildFlags::Partitioned) {
			// Match the grid bounds to the pre-transformed vertices, not local primitive bounds.
			auto& bounds = model.dimensions;
			bounds.min = glm::vec3(FLT_MAX);
			bounds.max = glm::vec3(-FLT_MAX);
			for (const auto& vertex : model.vertexBuffer) {
				bounds.min = glm::min(bounds.min, vertex.pos);
				bounds.max = glm::max(bounds.max, vertex.pos);
			}
			bounds.size = bounds.max - bounds.min;
			bounds.center = (bounds.min + bounds.max) * 0.5f;
			bounds.radius = glm::length(bounds.size) * 0.5f;
		}
	}

	float draw()
	{
		if (camera.updated) {
			renderer.resetFrameIndex();
		}

		float time = 0.f;
		if (mode == "interactive") {
			time = renderer.render(camera, glm::ivec2(width, height), pixels, framePixels);
		}
		else {
			time = benchmark->run(camera, glm::ivec2(width, height), pixels, framePixels);
		}

		return time;
	}

	void present()
	{
		PushConstants pc{
			.pixelAddr = vks::util::getBufferDeviceAddress(device, framePixels.buffer),
			.width = width,
			.height = height
		};

		VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer];

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = frameBuffers[currentImageIndex];

		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
		
		vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		VulkanExampleBase::drawUI(cmdBuffer);
		
		vkCmdEndRenderPass(cmdBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
	}

	void render() override final
	{
		if (!prepared)
			return;

		if (resized)
		{
			handleResize();
		}

		VulkanExampleBase::prepareFrame();
		
		renderKernelTimeAccumulator += draw();
		renderFrameCounter++;

		present();

		VulkanExampleBase::submitFrame();
		if (benchmark && benchmark->isFinished()) {
			prepared = false;
#if defined(_WIN32)
			PostQuitMessage(0);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			ANativeActivity_finish(androidApp->activity);
#else
			quit = true;
#endif
		}

		auto now = std::chrono::high_resolution_clock::now();
		float elapsedMs = std::chrono::duration<double, std::milli>(now - lastRenderTimestamp).count();

		if(elapsedMs >= 1000.0f) {
			renderKernelFPS = static_cast<uint32_t>((float)renderFrameCounter * (1000.0f / renderKernelTimeAccumulator));

			renderKernelTimeAccumulator = 0.0;
			renderFrameCounter = 0;
			lastRenderTimestamp = now;
		}
	}

	void OnUpdateUIOverlay(vks::UIOverlay* overlay) override final
	{
		ImGui::Text("[Kernels] %.2f ms/frame (%.1d fps)", (1000.0f / renderKernelFPS), renderKernelFPS);
	}

public:
	VulkanExample() : VulkanExampleBase()
	{
		title = "Vulkan Wavefront Path Tracer";

		apiVersion = VK_API_VERSION_1_3;
		enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
		enabledFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
		enabledFeatures.shaderStorageImageReadWithoutFormat = VK_TRUE;

		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

		Environment::setInstance(std::make_unique<AppEnvironment>());
		Environment* env = Environment::getInstance();

		std::string envFile;
#if defined(__ANDROID__)
		envFile = g_envFile;
#else
		if(commandLineParser.isSet("environment")) {
			envFile = commandLineParser.getValueAsString("environment", "default.json");
		}
		else {
			envFile = "default.json";
		}
#endif
		env->readEnvFile(getEnvPath() + envFile);

		env->getStringValue("Application.mode", mode);
		if (mode != "interactive" && mode != "benchmark") {
			vks::tools::exitFatal("Application.mode must be interactive or benchmark", -1);
		}
		std::string asMode;
		env->getStringValue("Scene.ASMode", asMode);
		if (asMode == "default") {
			buildFlags = scene::ASBuilder::BuildFlags::Default;
		}
		else if (asMode == "partitioned") {
			buildFlags = scene::ASBuilder::BuildFlags::Partitioned;
		}
		else {
			throw std::invalid_argument("Unknown Scene.ASMode: " + asMode);
		}

#if defined(_WIN32)
		//if (mode == "benchmark")
		setupConsole("Vulkan Wavefront Path Tracer");
#endif

		int w, h;
		env->getIntValue("Resolution.width", w);
		env->getIntValue("Resolution.height", h);
		width = static_cast<uint32_t>(w);
		height = static_cast<uint32_t>(h);

		glm::vec3 cameraPos, cameraRot;
		float nearPlane, farPlane, fov;
		env->getVectorValue("Camera.position", cameraPos);
		env->getVectorValue("Camera.rotation", cameraRot);
		env->getFloatValue("Camera.nearPlane", nearPlane);
		env->getFloatValue("Camera.farPlane", farPlane);
		env->getFloatValue("Camera.fieldOfView", fov);

		camera.type = Camera::CameraType::firstperson;
		camera.setPerspective(fov, (float)width / (float)height, nearPlane, farPlane);
		camera.setPosition(cameraPos);
		camera.setRotation(cameraRot);
	}

	~VulkanExample()
	{
		if (device) {
			vkDeviceWaitIdle(device);
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

			pixels.destroy();
			framePixels.destroy();
		}
	}

	void prepare() override final
	{
		VulkanExampleBase::prepare();

		loadAssets();

		// Create the acceleration structures used to render the ray traced scene
		asBuilder.init(*vulkanDevice, queue);
		asBuilder.build(model, buildFlags);

		createPipelines();

		timer.init(*vulkanDevice);
		renderer.init(*vulkanDevice, queue, timer, model, asBuilder.getGeometryNodeBuffer());

		if (mode == "benchmark") {
			benchmark = std::make_unique<Benchmark>(&renderer);
		}

		renderer.setAccelerationStructure(asBuilder.getTLASHandle());

		lastTimestamp = std::chrono::high_resolution_clock::now();

		prepared = true;
	}
};

VULKAN_EXAMPLE_MAIN()
