# Copyright (c) 2016-2025, Sascha Willems
# SPDX-License-Identifier: MIT

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

add_subdirectory(../external/ktx ${CMAKE_CURRENT_BINARY_DIR}/ktx)

add_library(base STATIC ${BASE_SRC})
target_link_libraries(base libktx)
if(WIN32)
    target_link_libraries(base ${Vulkan_LIBRARY} ${WINLIBS})
 else(WIN32)
    target_link_libraries(base ${Vulkan_LIBRARY} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)
