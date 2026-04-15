#ifndef _PBR_GLSL_
#define _PBR_GLSL_

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	float factor = clamp(1.0 - cosTheta, 0.0, 1.0);
    return F0 + (1.0 - F0) * factor * factor * factor * factor * factor;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / denom;
}

vec3 CalculateNormal(sampler2D normalMap, vec3 normal, vec2 tex_coord, vec4 tangent)
{
    vec3 tangentNormal = texture(normalMap, tex_coord).xyz * 2.0 - 1.0;

    vec3 N = normalize(normal);
    vec3 T = normalize(tangent.xyz);
    vec3 B = tangent.w * normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

#endif /* _PBR_GLSL_ */