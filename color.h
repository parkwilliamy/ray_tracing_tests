#ifndef COLOR_H
#define COLOR_H

#include "vec3.h"
#include "rtweekend.h"
#include "interval.h"

using color = vec3;

inline fixed8 linear_to_gamma(fixed8 linear_component) {
    if (linear_component > fixed8(0.0))
        return fp_sqrt(linear_component);
    return fixed8(0.0);
}

void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    // Apply linear to gamma transform
    r = linear_to_gamma(r);
    g = linear_to_gamma(g);
    b = linear_to_gamma(b);

    // Translate [0,1) fixed8 to byte range [0,255]
    static const interval intensity(0.0, 0.9375);  // max representable < 1.0 in Q3.4
    fixed8 scale = fixed8(0.0);  // we'll do this in double for output only

    // Convert to double just for the final pixel write
    double rd = intensity.clamp(r).to_double();
    double gd = intensity.clamp(g).to_double();
    double bd = intensity.clamp(b).to_double();

    int rbyte = int(256 * rd);
    int gbyte = int(256 * gd);
    int bbyte = int(256 * bd);

    // Clamp to [0, 255] just in case
    rbyte = (rbyte > 255) ? 255 : (rbyte < 0 ? 0 : rbyte);
    gbyte = (gbyte > 255) ? 255 : (gbyte < 0 ? 0 : gbyte);
    bbyte = (bbyte > 255) ? 255 : (bbyte < 0 ? 0 : bbyte);

    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

#endif