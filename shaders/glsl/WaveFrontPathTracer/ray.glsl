#ifndef _RAY_GLSL_
#define _RAY_GLSL_

/*
* @note: These structures are shared between cpp (Ray.h) and shader.
* Keep both definitions synchronozed when modifying
*/

struct Ray {
    vec3 origin;
    float tmin;
    vec3 direction;
    float tmax;
};

struct RayResult {
    int primitiveId;
    int geometryId;
    float t;
    uint packedBary; /* pack/unpack barycentric coords using packUnorm2x16()/unpackUnorm2x16() */
};

#endif /* _RAY_GLSL_ */