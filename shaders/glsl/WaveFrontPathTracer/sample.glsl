#ifndef _SAMPLE_GLSL_
#define _SAMPLE_GLSL_

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

uint lcg(inout uint seed) {
    const uint LCG_A = 1103515245u;
    const uint LCG_C = 12345u;
    const uint LCG_M = 0x00FFFFFFu;
    seed = (LCG_A * seed + LCG_C);
    return seed & LCG_M;
}

float randf(inout uint seed) {
    return (float(lcg(seed)) / float(0x01000000));
}

void jenkinsMix(inout uint a, inout uint b, inout uint c) {
    a -= b; a -= c; a ^= (c >> 13);
    b -= c; b -= a; b ^= (a << 8);
    c -= a; c -= b; c ^= (b >> 13);
    a -= b; a -= c; a ^= (c >> 12);
    b -= c; b -= a; b ^= (a << 16);
    c -= a; c -= b; c ^= (b >> 5);
    a -= b; a -= c; a ^= (c >> 3);
    b -= c; b -= a; b ^= (a << 10);
    c -= a; c -= b; c ^= (b >> 15); // ~36 instructions
}


// Based on Tom Duff, James Burgess, Per Christensen, Christophe Hery, Andrew Kensler, Max Liani, and
// Ryusuke Villemin, Building an Orthonormal Basis, Revisited, Journal of Computer Graphics
// Techniques (JCGT), vol. 6, no. 1, 1?8, 2017
// http://jcgt.org/published/0006/01/01/
void rightHandedBase(vec3 n, out vec3 b1, out vec3 b2) {
    float s = (n.z >= 0.0) ? 1.0 : -1.0;
    const float a = -1.0f / (s + n.z);
    const float b = n.x * n.y * a;
    b1 = vec3(1.0f + s * n.x * n.x * a, s * b, -s * n.x); // Tangent
    b2 = vec3(b, s + n.y * n.y * a, -n.y); // Binormal
}

vec3 cosineRandomVector(float r0, float r1, vec3 normal) {
    float theta = 2.0f * M_PI * r1;
    float radius = sqrt(r0);
    float x = radius * sin(theta);
    float z = radius * cos(theta);
    float y = sqrt(1.0f - r0);
    vec3 u, v;
    rightHandedBase(normal, u, v);
    return x * u + z * v + y * normal;
}

vec3 sampleGGX(float r0, float r1, vec3 N, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;

    float phi = 2.0 * M_PI * r0;
    float cosTheta = sqrt((1.0 - r1) / (1.0 + (a2 - 1.0) * r1));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    vec3 T, B;
	rightHandedBase(N, T, B);

    return normalize(T * H.x + B * H.y + N * H.z);
}

#endif /* _SAMPLE_GLSL_ */