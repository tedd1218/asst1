# NFL Renderer Comparison Report
# Generated automatically

## Configuration
- Resolution: 1920x1080
- Frames: 181
- Lights: 5 (1 directional + 4 point lights)

## Results

| Metric | Forward | Deferred | Winner |
|--------|---------|----------|--------|
| Total Time (s) | 24.51 | 25.44 | Forward |
| Avg Frame Time (ms) | 135.39 | 140.54 | Forward |
| FPS | 7.39 | 7.12 | Forward |
| Min Frame Time (ms) | 131.86 | 137.55 | Forward |
| Max Frame Time (ms) | 180.68 | 182.90 | Forward |

## Performance Analysis

- **Speedup**: 0.96x
- **Forward Advantage**: 3.8% faster
- **Deferred Advantage**: -3.7% faster

## Notes
- Forward rendering: Single-pass, calculates lighting per fragment
- Deferred rendering: Two-pass (geometry + lighting), better for many lights
- This scene has 5 lights, which may favor deferred rendering
