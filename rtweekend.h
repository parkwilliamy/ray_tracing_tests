#ifndef RTWEEKEND_H
#define RTWEEKEND_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>

#include "fixed8.h"

// C++ Std Usings

using std::make_shared;
using std::shared_ptr;

// Constants

const fixed8 fp_infinity = fixed8(7.9375);   // max representable Q3.4 value
const double pi = 3.1415926535897932385;      // keep as double for angle setup

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

// ---------------------------------------------------------------------------
// 32-bit Galois LFSR PRNG
// Polynomial: x^32 + x^31 + x^29 + x^1  (maximal-length, period 2^32 - 1)
// ---------------------------------------------------------------------------
struct lfsr32 {
    uint32_t state;
    lfsr32(uint32_t seed = 0xACE1u) : state(seed ? seed : 1u) {}
    inline uint32_t next() {
        uint32_t lsb = state & 1u;
        state >>= 1;
        if (lsb) state ^= 0xD0000001u;
        return state;
    }
};

inline lfsr32& global_lfsr() {
    static lfsr32 instance(0xACE1u);
    return instance;
}

// Random fixed8 in [0, 0.9375] — uses 4 bits from LFSR -> raw 0x00..0x0F
inline fixed8 random_fixed8() {
    uint32_t bits = global_lfsr().next();
    int8_t raw = static_cast<int8_t>((bits >> 28) & 0x0F);
    return fixed8::from_raw(raw);
}

// Random fixed8 in [min, max)
inline fixed8 random_fixed8(fixed8 mn, fixed8 mx) {
    return mn + random_fixed8() * (mx - mn);
}

// Double versions kept for one-time camera setup math
inline double random_double() {
    return global_lfsr().next() / 4294967296.0;
}

inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

// Common Headers
#include "color.h"
#include "ray.h"
#include "vec3.h"
#include "interval.h"

#endif