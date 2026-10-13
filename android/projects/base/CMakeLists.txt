file(GLOB BASE_SRC "../../../base/*.cpp" "../../../external/imgui/*.cpp")

add_library(libbase SHARED ${BASE_SRC})

include_directories(${BASE_DIR})
include_directories(../../../external)
include_directories(../../../external/glm)
include_directories(../../../external/gli)
include_directories(../../../external/imgui)
include_directories(../../../external/vulkan_radix_sort)
include_directories(${EXTERNAL_DIR}/tinygltf)
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)

add_subdirectory(../../../external/ktx ${CMAKE_CURRENT_BINARY_DIR}/ktx)


target_link_libraries(
	libbase
	android
	log
	z
	libktx
)
