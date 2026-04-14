#ifndef _BUFFER_REFERENCES_GLSL_
#define _BUFFER_REFERENCES_GLSL_

#ifdef USE_RAY_BUFFER
layout(buffer_reference, std430) readonly restrict buffer RayBufferRO { Ray data[]; };
layout(buffer_reference, std430) writeonly restrict buffer RayBufferWO { Ray data[]; };

layout(buffer_reference, std430) readonly restrict buffer RayResultBufferRO { RayResult data[]; };
layout(buffer_reference, std430) writeonly restrict buffer RayResultBufferWO { RayResult data[]; };
#endif

#ifdef USE_GEOMETRY_BUFFER
layout(buffer_reference, std430) readonly restrict buffer GeometryBufferRO { Geometry data[]; };
#endif

layout(buffer_reference, std430) readonly restrict buffer IntBufferRO { int data[]; };
layout(buffer_reference, std430) writeonly restrict buffer IntBufferWO { int data[]; };

layout(buffer_reference, std430) readonly restrict buffer UintBufferRO { uint data[]; };
layout(buffer_reference, std430) writeonly restrict buffer UintBufferWO { uint data[]; };
layout(buffer_reference, std430) restrict buffer UintBufferRW { uint data[]; };

layout(buffer_reference, std430) readonly restrict buffer Vec4BufferRO { vec4 data[]; };
layout(buffer_reference, std430) writeonly restrict buffer Vec4BufferWO { vec4 data[]; };
layout(buffer_reference, std430) restrict buffer Vec4BufferRW { vec4 data[]; };

layout(buffer_reference, std430) restrict buffer Counter { uint data; };


#endif /* _BUFFER_REFERENCES_GLSL_ */