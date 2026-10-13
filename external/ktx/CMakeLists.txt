# Build the vendored KTX1/KTX2 reader and Basis Universal transcoder.
# The renderer performs Vulkan uploads itself; tools and encoders are unnecessary.
add_library(libktx STATIC
    lib/texture.c lib/texture1.c lib/texture2.c
    lib/checkheader.c lib/hashlist.c lib/swap.c
    lib/filestream.c lib/memstream.c lib/strings.c
    lib/vkformat_check.c lib/vkformat_check_variant.c
    lib/vkformat_typesize.c lib/vk_funcs.c lib/vkloader.c
    lib/basis_transcode.cpp lib/miniz_wrapper.cpp
    external/basisu/transcoder/basisu_transcoder.cpp
    external/basisu/zstd/zstd.c
    external/dfdutils/createdfd.c external/dfdutils/colourspaces.c
    external/dfdutils/interpretdfd.c external/dfdutils/queries.c
    external/dfdutils/vk2dfd.c)
target_include_directories(libktx
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE lib external external/dfdutils external/basisu/zstd other_include utils)
target_compile_definitions(libktx
    PUBLIC KHRONOS_STATIC KTX_FEATURE_KTX1 KTX_FEATURE_KTX2
    PRIVATE LIBKTX KTX_FEATURE_WRITE=0 SUPPORT_SOFTWARE_ETC_UNPACK=0
        BASISD_SUPPORT_KTX2=0 BASISD_SUPPORT_KTX2_ZSTD=0)
target_compile_features(libktx PRIVATE c_std_11 cxx_std_11)
set_target_properties(libktx PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "external")
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(libktx PRIVATE ${CMAKE_DL_LIBS} Threads::Threads m)
endif()
