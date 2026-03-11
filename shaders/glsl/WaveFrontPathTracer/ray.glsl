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
    // Really needed all of these properties?
    int primitiveId;
    int geometryId;
    int instanceId;
    int customId;
    float t;
    float u;
    float v;
    float _pad;
};

struct RayStats {
    int traversedNodes;
    int testedTriangles;
};

#endif /* _RAY_GLSL_ */