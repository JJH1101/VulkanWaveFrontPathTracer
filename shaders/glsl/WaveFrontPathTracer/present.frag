#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "bufferReferences.glsl"

layout(push_constant) uniform PushConstants {
    uint64_t pixelAddr;
    uint width;
    uint height;
} pc;

layout (location = 0) out vec4 outFragcolor;

// Copy to swapchain image
void main() {
    const uvec2 uv = uvec2(gl_FragCoord.xy);

    Vec4BufferRO pixels = Vec4BufferRO(pc.pixelAddr);

    outFragcolor = pixels.data[uv.y * pc.width + uv.x];
}
