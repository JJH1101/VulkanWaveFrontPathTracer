#ifndef _GEOMETRYTYPES_GLSL_
#define _GEOMETRYTYPES_GLSL_

#ifndef USE_GEOMETRY_BUFFER
#define USE_GEOMETRY_BUFFER
#endif

struct Triangle {
	vec3 normal;
	vec2 uv;
	vec4 color;
	vec4 tangent;
};

struct GeometryNode {
	vec4 baseColorFactor;
	uint64_t vertexBufferDeviceAddress;
	uint64_t indexBufferDeviceAddress;
	int textureIndexBaseColor;
	int textureIndexNormal;
	int textureIndexMetallicRoughness;
	int textureIndexEmissive;
	float metallicFactor;
	float roughnessFactor;
	float _padding0;
	float _padding1;
};

layout(buffer_reference, std430) readonly restrict buffer Vertices { vec4 v[]; };
layout(buffer_reference, std430) readonly restrict buffer Indices { uint i[]; };

// This function will unpack our vertex buffer data into a single triangle and calculates uv coordinates
Triangle unpackTriangle(uint primitiveId, 
						uint64_t vertexBufferDeviceAddress, 
						uint64_t indexBufferDeviceAddress, 
						vec2 barycentric) {
	const uint triIndex = primitiveId * 3;

	Indices indices = Indices(indexBufferDeviceAddress);
	Vertices vertices = Vertices(vertexBufferDeviceAddress);
	// Unpack vertices
	// Data is packed as vec4 so we can map to the glTF vertex structure from the host side
	// We match vkglTF::Vertex: pos.xyz+normal.x, normalyz+uv.xy
	// glm::vec3 pos;
	// glm::vec3 normal;
	// glm::vec2 uv;
	// ...
	vec3 barycentricCoords = vec3(1.0f - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
	Triangle tri;
	tri.normal = vec3(0.0f);
	tri.uv = vec2(0.0f);
	tri.color = vec4(0.0f);
	tri.tangent = vec4(0.0f);

	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i] * 6;

		vec4 d0 = vertices.v[offset + 0]; // pos.xyz, n.x
		vec4 d1 = vertices.v[offset + 1]; // n.yz, uv.xy
		vec4 d2 = vertices.v[offset + 2]; // color.rgba
		vec4 d3 = vertices.v[offset + 5]; // tangent.xyzw

		// Calculate values at barycentric coordinates
		float bary = barycentricCoords[i];
		tri.normal += vec3(d0.w, d1.xy) * bary;
		tri.uv += d1.zw * bary;
		tri.color += d2 * bary;
		tri.tangent += d3 * bary;
	}
	
	tri.normal = normalize(tri.normal);
	tri.tangent.xyz = normalize(tri.tangent.xyz);
	
	return tri;
}

#endif /* _GEOMETRYTYPES_GLSL_ */