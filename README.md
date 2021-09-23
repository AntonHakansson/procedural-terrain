# Procedural Terrain Engine

[![Windows](https://github.com/AntonHakansson/procedural-terrain/actions/workflows/windows.yml/badge.svg)](https://github.com/AntonHakansson/procedural-terrain/actions/workflows/windows.yml)
[![Linux](https://github.com/AntonHakansson/procedural-terrain/actions/workflows/linux.yml/badge.svg)](https://github.com/AntonHakansson/procedural-terrain/actions/workflows/linux.yml)

Procedural terrain generator with GPU tesselation, cascaded shadows, PBR, and screen space reflections using OpenGL.

https://user-images.githubusercontent.com/15860608/134507664-d40b17b1-c3ac-4082-bff8-971591c49c1e.mov

## Implementation Overview

| Feature                                                            | Comments                                                                                      |
|--------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| Generate a realistic looking terrain geometry with noise functions | Voronoi and Layered Simplex noise                                                             |
| Terrain GPU Tessellation                                           | Adapts triangle granularity based on distance to camera                                       |
| Blend textures depending on height and slope of the terrrain       |                                                                                               |
| Terrain Textures w/ albedo, normal, and displacement maps          | Normal maps applied in tangent space. Displacement map for nicer blending.                    |
| Cascaded shadow maps for nice looking shadows in a bigger world.   | Uses PCF for smoothing and eliminating artifact, smoothly interpolates between cascades       |
| Screen Space reflections                                           | Based on paper "Efficient GPU Screen-Space Ray Tracing"                                       |
| Water with reflection and refraction                               | Implemented with screen space reflections for both refraction and reflection                  |
| Animated waves, shoreline foams and ocean depth                    | Scrolling dudv map for water normals, samples depth buffer for foam and ocean depth           |
| Procedural sun & sky                                               | PostFX filter to create sunsets and horizon with day/night cycle, illuminates world w/ PostFX |
| Screen Space God Rays                                              | Implemented in post process pass with screen space ray marching                               |

## Build

```bash
cmake -B build
cmake --build build -j 4
./build/procedural-terrain # run
```
