#ifndef _GEOMETRYTYPES_GLSL_
#define _GEOMETRYTYPES_GLSL_

struct Triangle {
	vec3 normal;
	vec2 uv;
};

struct Geometry {
	uint64_t vertexAddr;
	uint64_t indexAddr;
};

layout(buffer_reference, std430) buffer Vertices { vec4 v[]; };
layout(buffer_reference, std430) buffer Indices { uint i[]; };

// This function will unpack our vertex buffer data into a single triangle and calculates uv coordinates
Triangle unpackTriangle(uint primitiveId, Geometry geometry, vec2 barycentric) {
	const uint triIndex = primitiveId * 3;

	Indices indices = Indices(geometry.indexAddr);
	Vertices vertices = Vertices(geometry.vertexAddr);

	// Unpack vertices
	// Data is packed as vec4 so we can map to the glTF vertex structure from the host side
	// We match vkglTF::Vertex: pos.xyz+normal.x, normalyz+uv.xy
	// glm::vec3 pos;
	// glm::vec3 normal;
	// glm::vec2 uv;
	// ...
	vec3 normal[3];
	vec2 uv[3];
	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i] * 6;
		vec4 d0 = vertices.v[offset + 0]; // pos.xyz, n.x
		vec4 d1 = vertices.v[offset + 1]; // n.yz, uv.xy
		normal[i] = vec3(d0.w, d1.xy);
		uv[i] = d1.zw;
	}
	// Calculate values at barycentric coordinates
	vec3 barycentricCoords = vec3(1.0f - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
	Triangle tri;
	tri.normal = normalize(normal[0] * barycentricCoords.x + normal[1] * barycentricCoords.y + normal[2] * barycentricCoords.z);
	tri.uv = uv[0] * barycentricCoords.x + uv[1] * barycentricCoords.y + uv[2] * barycentricCoords.z;

	return tri;
}

#endif /* _GEOMETRYTYPES_GLSL_ */