# Vulkan Wavefront Path Tracer

A Wavefront Path Tracing Framework built on Vulkan. This project focuses on analyzing ray tracing performance and includes ray reordering techniques.

## Building

```
1. Clone this repository.
2. Make build system using CMake.
3. Build using Visual Studio IDE.
```

## Running

Confiure settings via `env/env.json`.
All options can be found in `projects/WaveFrontPathTracer/Environment/AppEnvironment.cpp`.
```
"Application": {
    "mode": "interactive" // Choose between "interactive" or "benchmark"
},
"Scene": {
    "filename": "models/sponza/sponza.gltf", // Path to your GLTF model
    ...
},
...
```

## References
This framework is built upon the following projects:
- [Performance Comparision of Bounding Volume Hierarchies for GPU Ray Tracing](https://github.com/meistdan/hippie): Core wavefront path tracing and ray reordering logic.
- [Vulkan C++ examples and demos](https://github.com/SaschaWillems/Vulkan): Base Vulkan infrastructure and framework.
- [vulkan_radix_sort](https://github.com/jaesung-cs/vulkan_radix_sort): GPU-based Morton code sorting for ray reordering.