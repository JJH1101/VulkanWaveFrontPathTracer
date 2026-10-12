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

Benchmark mode saves a TSV file to `results/<scene>_<timestamp>.tsv` under the working directory after all views finish. Columns are `Scene`, `Ray Type`, `Bounce`, `Ray Counts`, `Trace Time (ms)`, and `Trace Perf (Mrays/s)`. Counts and times are averaged over the measured frames, excluding warmup. Total-row performance matches the console averages; bounce-row performance is calculated from the accumulated ray count and trace time. Enable `Benchmark.printBounceLogs` to include the individual Path and Shadow bounce rows.

On Android, `envs/`, `models/`, and `results/` share the app's external files directory, `/sdcard/Android/data/sogang.graphics.WaveFrontPathTracer/files/`. This allows the app to write results without the SELinux restrictions on `/data/local/tmp/`. After installing the app, run `adbPushAssets.bat` and `adbPushEnvs.bat` to upload inputs to the new location. Results are created automatically and can be retrieved with:

```sh
adb pull /sdcard/Android/data/sogang.graphics.WaveFrontPathTracer/files/results/ ./results/
```

## References
This framework is built upon the following projects:
- [Performance Comparision of Bounding Volume Hierarchies for GPU Ray Tracing](https://github.com/meistdan/hippie): Core wavefront path tracing and ray reordering logic.
- [Vulkan C++ examples and demos](https://github.com/SaschaWillems/Vulkan): Base Vulkan infrastructure and framework.
- [vulkan_radix_sort](https://github.com/jaesung-cs/vulkan_radix_sort): GPU-based Morton code sorting for ray reordering.
