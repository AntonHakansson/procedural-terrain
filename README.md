# Advanced Computer Graphics Project (DAT205)

## Terrain

## Build

```bash
cmake -B build
cmake --build build -j 4
./build/AdvGfxProject # run
```

## Project Report

| Thing we did                                                       | Approx time it took | Points we want | Comments                                                                                      |
|--------------------------------------------------------------------|---------------------|----------------|-----------------------------------------------------------------------------------------------|
| Generate a realistic looking terrain geometry with noise functions | 6h                  | 2p             | Voronoi and Layered Simplex noise                                                             |
| Terrain GPU Tessellation                                           | 6h                  | 2p             | Adapts triangle granularity based on distance to camera                                       |
| Render with blended textures depending on height and slope         | 2h                  | 2p             | 2h for the implementation, ~7h for fine-tuning                                                |
| Terrain Textures w/ albedo, normal, and displacement maps          | 4h                  | 1p             | Normal maps applied in tangent space. Displacement map for nicer blending.                    |
| Cascaded shadow maps for nice looking shadows in a bigger world.   | 18h                 | 3p             | Uses PCF for smoothing and eliminating artifact, smoothly interpolates between cascades       |
| Screen Space reflections                                           | 15h                 | 2p             | Based on "Efficient GPU Screen-Space Ray Tracing"                                             |
| Water with reflection and refraction                               | 10h                 | 2p             | Implemented with screen space reflections for both refraction and reflection                  |
| Animated waves, shoreline foams and ocean depth                    | 4h                  | 1p             | Scrolling dudv map for water normals, samples depth buffer for foam and ocean depth           |
| Procedural sun & sky                                               | 6h                  | 2p             | PostFX filter to create sunsets and horizon with day/night cycle, illuminates world w/ PostFX |
| Screen Space God Rays                                              | 4h                  | 1p             | Implemented in post process pass with screen space ray marching                               |
|--------------------------------------------------------------------|---------------------|----------------|-----------------------------------------------------------------------------------------------|
| Total:                                                             | 75h                 | 18p            |                                                                                               |

