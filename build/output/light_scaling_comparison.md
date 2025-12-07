# Light Scaling Comparison Report
# Forward vs Deferred Rendering with Varying Light Counts

## Summary

This test compares forward and deferred rendering performance as the number
of lights increases. Forward rendering has O(fragments × lights) complexity,
while deferred rendering has O(fragments + lights × affected_pixels) complexity.

**Theoretical crossover point**: Deferred becomes faster when light count is high
enough that the G-Buffer overhead is offset by reduced per-fragment light calculations.

**Crossover Point**: ~10 lights
(Deferred becomes faster at around this many lights)

## Results

| Lights | Forward FPS | Forward ms | Deferred FPS | Deferred ms | Winner | Speedup |
|--------|-------------|------------|--------------|-------------|--------|----------|
| 1 | 8.72 | 114.65 | 7.10 | 140.77 | Forward | 0.81x |
| 5 | 7.36 | 135.91 | 7.14 | 139.97 | Forward | 0.97x |
| 10 | 6.16 | 162.43 | 7.16 | 139.71 | Deferred | 1.16x |
| 25 | 4.12 | 242.83 | 7.08 | 141.15 | Deferred | 1.72x |
| 50 | 2.62 | 381.98 | 7.12 | 140.37 | Deferred | 2.72x |
| 75 | 1.92 | 519.54 | 7.19 | 139.13 | Deferred | 3.73x |
| 100 | 1.51 | 660.85 | 7.20 | 138.97 | Deferred | 4.76x |

## Analysis

### Performance Trends

- **Forward slowdown** (1 → 100 lights): 5.76x
- **Deferred slowdown** (1 → 100 lights): 0.99x

### Observations

- Forward rendering scales poorly with light count (5.76x slowdown)
- Deferred rendering handles many lights more efficiently (0.99x slowdown)

## Notes

- Forward rendering: Single-pass, O(fragments × lights)
- Deferred rendering: Multi-pass, O(fragments + lights × pixels_per_light)
- Deferred has fixed overhead from G-Buffer generation
- Crossover point depends on scene complexity, resolution, and light properties
