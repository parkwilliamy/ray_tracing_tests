# Configurable Ray Tracer — User Guide

## Overview

This ray tracer renders 3D scenes to PPM images with two independent quality-control axes: **number encoding format** (how precision is stored) and **arithmetic implementation** (how each math operation is computed). By varying these, you can study how hardware design choices affect rendering quality.

**Supported primitives:** Spheres and Triangles.
**Supported materials:** Lambertian (matte), Metal (reflective), Dielectric (glass).
**Camera:** Fixed at origin, looking down −Z, 90° vertical FOV.

---

## Compilation

**Requirements:** A C++17 compiler (GCC 7+, Clang 5+).

```bash
# Build with optimizations
make

# Build with debug symbols + AddressSanitizer
make debug

# Clean build artifacts
make clean
```

The build intentionally avoids `-ffast-math` because it would reorder floating-point operations and defeat the number format simulation.

---

## Running

Three modes are available:

```bash
# Interactive mode — prompts for every setting
./raytracer

# Quick test — all defaults (fp32, standard math, built-in scene)
./raytracer --defaults

# Specify scene file, configure rest interactively
./raytracer --scene scenes/example.scene
```

---

## Render Quality Parameters

### Samples Per Pixel (SPP)

Controls antialiasing. Each pixel casts this many rays with random sub-pixel jitter, then averages the results. Higher values reduce noise but increase render time linearly.

| SPP | Quality | Use case |
|-----|---------|----------|
| 1–10 | Very noisy | Quick previews |
| 50–100 | Moderate | Development testing |
| 200–500 | Good | Quality comparison renders |
| 1000+ | Publication | Final reference images |

### Max Ray Bounces

Limits how many times a ray can scatter off surfaces before it's terminated. Affects reflections, refractions, and global illumination.

| Depth | Effect |
|-------|--------|
| 1 | Direct lighting only, no reflections |
| 3–5 | Basic reflections and refractions |
| 10 | Good for glass with multiple internal bounces |
| 50 | Diminishing returns for most scenes |

### Number Encoding Format

Simulates limited-precision hardware by quantizing every intermediate value.

| Format | Syntax | Example | Range / Precision |
|--------|--------|---------|-------------------|
| 32-bit float | `fp32` | `fp32` | Full IEEE 754 (reference) |
| 16-bit float | `fp16` | `fp16` | ±65504, ~3 decimal digits |
| 8-bit float | `fp8` | `fp8` | ±448, ~2 decimal digits (E4M3) |
| 8-bit fixed | `Q8n.m` | `Q84.4` | Signed, n int + m frac = 8 |
| 16-bit fixed | `Q16n.m` | `Q168.8` | Signed, n int + m frac = 16 |

**Fixed-point examples:**
- `Q84.4` → range ≈ [−8, +7.9375], step = 0.0625
- `Q82.6` → range ≈ [−2, +1.984], step = 0.015625 (more precision, less range)
- `Q168.8` → range ≈ [−128, +127.996], step = 0.00390625

The MSB of the integer bits is always the sign bit.

---

## Arithmetic Operation Implementations

The program asks you to choose an algorithm for each operation category. Every math call in the renderer routes through your selection.

### Addition / Subtraction
| Method | Description |
|--------|-------------|
| Standard | Native IEEE 754 float add/sub |
| Saturating | Clamps to ±3.4×10³⁸ on overflow instead of producing ±inf |

### Multiplication
| Method | Description |
|--------|-------------|
| Standard | Native IEEE 754 float mul |
| Shift-and-Add | Serial multiplier with 10-bit mantissa (truncation error) |
| Log-Domain | Computes exp(log(a) + log(b)); simulates logarithmic number system |

### Division
| Method | Description |
|--------|-------------|
| Standard | Native IEEE 754 float div |
| Newton-Raphson | Iterative reciprocal (4 iterations, ~16 bits accuracy) |
| Goldschmidt | Convergence division (5 iterations) |
| SRT | Radix-2 digit-recurrence (24-bit quotient) |
| LUT | 256-entry reciprocal ROM with linear interpolation |

### Square Root
| Method | Description |
|--------|-------------|
| Standard | Hardware sqrt (libm) |
| Newton (Heron) | Iterative: xₙ₊₁ = (xₙ + a/xₙ)/2, 6 iterations |
| Fast Inv Sqrt | Quake III magic constant (0x5f3759df), 2 refinements |
| CORDIC | Hyperbolic vectoring mode, 20 iterations |
| LUT | 256-entry ROM with linear interpolation |

### Trigonometry (sin, cos, atan2)
| Method | Description |
|--------|-------------|
| Standard | Hardware trig (libm) |
| Taylor | Maclaurin series (6 terms for sin, polynomial for atan) |
| CORDIC | Rotational mode, 20 iterations with precomputed arctan table |
| LUT | 1024-entry sine table + quadrant symmetry |
| Chebyshev | Chebyshev polynomial (uniform error across interval) |

### Logarithm / Exponential
| Method | Description |
|--------|-------------|
| Standard | Hardware log/exp (libm) |
| Taylor | Range-reduced series (7 terms) |
| LUT | 256-entry ROM with linear interpolation |

### Random Number Generation
| Method | Description |
|--------|-------------|
| LFSR | 32-bit Galois LFSR; fast, low quality, visible patterns in noise |
| LCG | Linear Congruential Generator (Numerical Recipes constants) |
| Xorshift32 | Marsaglia's xorshift; good speed/quality balance |
| Mersenne Twister | MT19937; gold-standard quality, large state |

---

## Scene File Format

Scene files are plain text. Lines starting with `#` are comments. Materials must be defined before the objects that reference them.

### Material Definitions

```
material lambertian <name> <r> <g> <b>
material metal <name> <r> <g> <b> <fuzz>
material dielectric <name> <ior>
```

**Parameters:**
- `r g b` — color channels in [0, 1]
- `fuzz` — metal roughness in [0, 1] (0 = perfect mirror)
- `ior` — index of refraction (glass ≈ 1.5, water ≈ 1.33, diamond ≈ 2.42)

### Object Definitions

```
sphere <cx> <cy> <cz> <radius> <material_name>
triangle <x0> <y0> <z0> <x1> <y1> <z1> <x2> <y2> <z2> <material_name>
```

**Sphere notes:**
- A negative radius creates an inward-facing surface (used for hollow glass spheres).
- A very large sphere (radius 100+) approximates a flat ground plane.

**Triangle notes:**
- Vertices should be in counter-clockwise order (right-hand rule defines the outward normal).
- Both sides of the triangle are rendered (the normal flips to face the incoming ray).

### Coordinate System

- **+X** = right, **+Y** = up, **−Z** = into the screen (camera looks down −Z)
- Camera is at (0, 0, 0). Place objects at negative Z values to be visible.
- The 90° FOV makes objects at z = −1 span roughly the viewport width.

### Example Scene File

```
# Materials
material lambertian ground 0.8 0.8 0.0
material lambertian blue   0.1 0.2 0.5
material dielectric glass  1.5
material metal      gold   0.8 0.6 0.2 0.0
material metal      copper 0.7 0.3 0.2 0.3
material lambertian red    0.9 0.15 0.15

# Ground (large sphere as plane)
sphere 0.0 -100.5 -1.0 100.0 ground

# Three main spheres
sphere  0.0  0.0 -1.0  0.5  blue
sphere -1.0  0.0 -1.0  0.5  glass
sphere -1.0  0.0 -1.0 -0.4  glass    # hollow interior
sphere  1.0  0.0 -1.0  0.5  gold

# Triangle
triangle -0.3 0.7 -0.8  0.3 0.7 -0.8  0.0 1.2 -1.0  red
```

---

## Output

The renderer outputs PPM (Portable Pixmap) files. To convert to PNG:

```bash
# Using ImageMagick
convert output.ppm output.png

# Using Python + Pillow
python3 -c "from PIL import Image; Image.open('output.ppm').save('output.png')"

# Using ffmpeg
ffmpeg -i output.ppm output.png
```

---

## Source Files

| File | Purpose |
|------|---------|
| `number_format.h` | Quantization layer (fp8/fp16/fp32/Q8/Q16) |
| `math_engine.h` | All arithmetic algorithm implementations |
| `scalar.h` | Quantized scalar type wrapping float |
| `vec3.h` | 3D vector class using Scalar |
| `ray.h` | Ray representation (origin + direction) |
| `geometry.h` | Hittable interface, Sphere, Triangle, HittableList |
| `material.h` | Lambertian, Metal, Dielectric materials |
| `camera.h` | Fixed-position camera with ray generation |
| `renderer.h` | Render loop, recursive ray_color, PPM output |
| `scene_loader.h` | Scene file parser + default scene builder |
| `main.cpp` | CLI entry point with interactive configuration |
| `Makefile` | Build system |

---

## Quick Start

```bash
make
./raytracer --defaults    # Renders the built-in scene with fp32/standard math
# → Creates output.ppm (400×225, 50 SPP)
```

For a comparison study, render the same scene with different settings:

```bash
# Reference render
echo -e "800\n450\n200\n10\nfp32\n1\n1\n1\n1\n1\n1\n1\n4\ndefault\nref_fp32.ppm" | ./raytracer

# Same scene with fp16 and CORDIC sqrt
echo -e "800\n450\n200\n10\nfp16\n1\n1\n1\n1\n4\n1\n1\n4\ndefault\ntest_fp16_cordic.ppm" | ./raytracer
```
