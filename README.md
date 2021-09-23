# Procedural Terrain Engine

https://user-images.githubusercontent.com/15860608/134507664-d40b17b1-c3ac-4082-bff8-971591c49c1e.mov

## Terrain

## Build

```bash
cmake -B build
cmake --build build -j 4
./build/procedural-terrain # run
```

## Implementation Overview

|--------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| Features                                                           | Comments                                                                                      |
|--------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| Generate a realistic looking terrain geometry with noise functions | Voronoi and Layered Simplex noise                                                             |
| Terrain GPU Tessellation                                           | Adapts triangle granularity based on distance to camera                                       |
| Render with blended textures depending on height and slope         | 2h for the implementation, ~7h for fine-tuning and tinkering                                  |
| Terrain Textures w/ albedo, normal, and displacement maps          | Normal maps applied in tangent space. Displacement map for nicer blending.                    |
| Cascaded shadow maps for nice looking shadows in a bigger world.   | Uses PCF for smoothing and eliminating artifact, smoothly interpolates between cascades       |
| Screen Space reflections                                           | Based on "Efficient GPU Screen-Space Ray Tracing"                                             |
| Water with reflection and refraction                               | Implemented with screen space reflections for both refraction and reflection                  |
| Animated waves, shoreline foams and ocean depth                    | Scrolling dudv map for water normals, samples depth buffer for foam and ocean depth           |
| Procedural sun & sky                                               | PostFX filter to create sunsets and horizon with day/night cycle, illuminates world w/ PostFX |
| Screen Space God Rays                                              | Implemented in post process pass with screen space ray marching                               |
|--------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|

