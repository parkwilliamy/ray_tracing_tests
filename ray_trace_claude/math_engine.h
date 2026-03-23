/**
 * math_engine.h — Configurable Arithmetic Operations Engine
 * ==========================================================
 * This is the core module that allows the user to select HOW each mathematical
 * operation is performed. The MathEngine singleton stores function pointers for
 * every operation category. At startup, the user selects an implementation for
 * each category, and all subsequent computations route through these pointers.
 *
 * This design simulates different hardware arithmetic units. For example,
 * selecting CORDIC for sqrt simulates an FPGA/ASIC that uses a CORDIC
 * coprocessor, while selecting NEWTON simulates iterative convergence logic.
 *
 * Operation categories and available implementations:
 *
 *   ADDITION       : STANDARD, SATURATING
 *   SUBTRACTION    : STANDARD, SATURATING
 *   MULTIPLICATION : STANDARD, SHIFT_ADD, LOG_DOMAIN
 *   DIVISION       : STANDARD, NEWTON_RAPHSON, GOLDSCHMIDT, SRT, LUT
 *   SQUARE ROOT    : STANDARD, NEWTON, FAST_INV_SQRT, CORDIC, LUT
 *   TRIGONOMETRY   : STANDARD, TAYLOR, CORDIC, LUT, CHEBYSHEV
 *   LOG / EXP      : STANDARD, TAYLOR, LUT
 *   RNG            : LFSR, LCG, XORSHIFT, MERSENNE_TWISTER
 *
 * Each implementation introduces characteristic approximation errors that
 * compound through the ray tracing pipeline, letting the user study how
 * arithmetic unit design affects final image quality.
 */

#ifndef MATH_ENGINE_H
#define MATH_ENGINE_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <map>
#include <iostream>
#include <random>

// ---------------------------------------------------------------------------
// Method enumerations — one enum per operation category
// ---------------------------------------------------------------------------

enum class AddMethod       { STANDARD, SATURATING };
enum class SubMethod       { STANDARD, SATURATING };
enum class MulMethod       { STANDARD, SHIFT_ADD, LOG_DOMAIN };
enum class DivMethod       { STANDARD, NEWTON_RAPHSON, GOLDSCHMIDT, SRT, LUT };
enum class SqrtMethod      { STANDARD, NEWTON, FAST_INV_SQRT, CORDIC, LUT };
enum class TrigMethod      { STANDARD, TAYLOR, CORDIC, LUT, CHEBYSHEV };
enum class LogExpMethod    { STANDARD, TAYLOR, LUT };
enum class RngMethod       { LFSR, LCG, XORSHIFT, MERSENNE_TWISTER };

// ---------------------------------------------------------------------------
// Individual algorithm implementations — namespaced for clarity
// ---------------------------------------------------------------------------
namespace math_impl {

// === ADDITION ===

inline float add_standard(float a, float b) {
    return a + b;
}

/**
 * Saturating addition: clamps to ±max instead of overflowing.
 * Mimics hardware saturating arithmetic (common in DSP/fixed-point units).
 */
inline float add_saturating(float a, float b) {
    float result = a + b;
    if (std::isinf(result)) {
        return (result > 0) ? 3.4e38f : -3.4e38f;
    }
    return result;
}

// === SUBTRACTION ===

inline float sub_standard(float a, float b) {
    return a - b;
}

inline float sub_saturating(float a, float b) {
    float result = a - b;
    if (std::isinf(result)) {
        return (result > 0) ? 3.4e38f : -3.4e38f;
    }
    return result;
}

// === MULTIPLICATION ===

inline float mul_standard(float a, float b) {
    return a * b;
}

/**
 * Shift-and-add multiplication: simulates a serial multiplier.
 * We decompose one operand into powers of two and accumulate partial products.
 * Introduces rounding error because we truncate after a limited number of bits.
 * Uses 10 most significant bits of the mantissa for partial products.
 */
inline float mul_shift_add(float a, float b) {
    if (a == 0.0f || b == 0.0f) return 0.0f;

    float sign = ((a < 0) != (b < 0)) ? -1.0f : 1.0f;
    float abs_a = std::fabs(a);
    float abs_b = std::fabs(b);

    // Decompose abs_b into sum of powers of two (limited precision)
    // Find the exponent of abs_b
    int exp_b;
    float frac_b = std::frexp(abs_b, &exp_b);  // abs_b = frac_b * 2^exp_b

    // Use 10-bit mantissa approximation (simulates limited shifter width)
    int mantissa_bits = 10;
    float result = 0.0f;
    float current_frac = frac_b;

    for (int i = 0; i < mantissa_bits && current_frac > 0.0f; i++) {
        current_frac *= 2.0f;
        if (current_frac >= 1.0f) {
            // This bit is set: add abs_a * 2^(exp_b - 1 - i)
            result += std::ldexp(abs_a, exp_b - 1 - i);
            current_frac -= 1.0f;
        }
    }
    return sign * result;
}

/**
 * Log-domain multiplication: compute a*b = exp(log(|a|) + log(|b|)).
 * Introduces the combined approximation error of log and exp.
 * This simulates a logarithmic number system (LNS) multiplier.
 */
inline float mul_log_domain(float a, float b) {
    if (a == 0.0f || b == 0.0f) return 0.0f;
    float sign = ((a < 0) != (b < 0)) ? -1.0f : 1.0f;
    float log_sum = std::log(std::fabs(a)) + std::log(std::fabs(b));
    return sign * std::exp(log_sum);
}

// === DIVISION ===

inline float div_standard(float a, float b) {
    if (b == 0.0f) return (a >= 0) ? 1e30f : -1e30f;
    return a / b;
}

/**
 * Newton-Raphson division: computes a/b by finding 1/b iteratively.
 * Uses the iteration: x_{n+1} = x_n * (2 - b * x_n)
 * Starting guess: use bit manipulation for initial reciprocal estimate.
 * 4 iterations gives roughly 16 bits of accuracy.
 */
inline float div_newton_raphson(float a, float b) {
    if (b == 0.0f) return (a >= 0) ? 1e30f : -1e30f;

    float sign = ((a < 0) != (b < 0)) ? -1.0f : 1.0f;
    float abs_b = std::fabs(b);

    // Initial guess for 1/abs_b using bit manipulation
    // Magic number approach (similar to fast inverse sqrt idea)
    uint32_t bi;
    std::memcpy(&bi, &abs_b, 4);
    bi = 0x7EF311C7 - bi;  // approximate reciprocal
    float x;
    std::memcpy(&x, &bi, 4);

    // Newton-Raphson iterations: x = x * (2 - abs_b * x)
    for (int i = 0; i < 4; i++) {
        x = x * (2.0f - abs_b * x);
    }

    return sign * std::fabs(a) * x;
}

/**
 * Goldschmidt division: simultaneously converges on quotient and remainder.
 * Uses the iteration: multiply both numerator and denominator by (2 - d)
 * where d approaches 1, so n/d approaches the quotient.
 * 5 iterations for good convergence.
 */
inline float div_goldschmidt(float a, float b) {
    if (b == 0.0f) return (a >= 0) ? 1e30f : -1e30f;

    float sign = ((a < 0) != (b < 0)) ? -1.0f : 1.0f;
    float n = std::fabs(a);
    float d = std::fabs(b);

    // Normalize d to [0.5, 1.0) range
    int exp_d;
    float frac_d = std::frexp(d, &exp_d);
    n = std::ldexp(n, -exp_d);
    d = frac_d;

    // Goldschmidt iterations
    for (int i = 0; i < 5; i++) {
        float f = 2.0f - d;
        n *= f;
        d *= f;
    }

    return sign * n;
}

/**
 * SRT division: simulates digit-recurrence division (radix-2).
 * Produces one bit of quotient per iteration. We run 24 iterations
 * to fill a single-precision mantissa. This introduces characteristic
 * truncation error at the LSB.
 */
inline float div_srt(float a, float b) {
    if (b == 0.0f) return (a >= 0) ? 1e30f : -1e30f;

    float sign = ((a < 0) != (b < 0)) ? -1.0f : 1.0f;
    float abs_a = std::fabs(a);
    float abs_b = std::fabs(b);

    // Normalize both to get the quotient bit by bit
    int exp_a, exp_b;
    float frac_a = std::frexp(abs_a, &exp_a);
    float frac_b = std::frexp(abs_b, &exp_b);

    float remainder = frac_a;
    float quotient = 0.0f;
    int result_exp = exp_a - exp_b;

    // Radix-2 digit recurrence: extract 24 bits
    for (int i = 0; i < 24; i++) {
        quotient *= 2.0f;
        if (remainder >= frac_b) {
            quotient += 1.0f;
            remainder -= frac_b;
        }
        remainder *= 2.0f;
    }

    return sign * std::ldexp(quotient, result_exp - 24);
}

/**
 * LUT-based division: uses a 256-entry lookup table for 1/x in [1,2),
 * with linear interpolation between entries. Simulates ROM-based dividers.
 */
inline float div_lut(float a, float b) {
    if (b == 0.0f) return (a >= 0) ? 1e30f : -1e30f;

    float sign = ((a < 0) != (b < 0)) ? -1.0f : 1.0f;
    float abs_b = std::fabs(b);

    // Normalize to [1, 2)
    int exp_b;
    float frac_b = std::frexp(abs_b, &exp_b);
    frac_b *= 2.0f;  // now in [1, 2)
    exp_b -= 1;

    // Build LUT (in practice this would be a static ROM)
    // 256 entries for 1/x where x in [1, 2)
    static float lut[256];
    static bool lut_init = false;
    if (!lut_init) {
        for (int i = 0; i < 256; i++) {
            float x = 1.0f + static_cast<float>(i) / 256.0f;
            lut[i] = 1.0f / x;
        }
        lut_init = true;
    }

    // Lookup with linear interpolation
    float idx_f = (frac_b - 1.0f) * 256.0f;
    int idx = static_cast<int>(idx_f);
    idx = std::clamp(idx, 0, 254);
    float frac = idx_f - idx;
    float recip = lut[idx] * (1.0f - frac) + lut[idx + 1] * frac;

    return sign * std::fabs(a) * std::ldexp(recip, -exp_b);
}

// === SQUARE ROOT ===

inline float sqrt_standard(float a) {
    return (a < 0.0f) ? 0.0f : std::sqrt(a);
}

/**
 * Newton's method (Heron's method) for sqrt: x_{n+1} = (x_n + a/x_n) / 2
 * Starting guess: a/2. 6 iterations.
 */
inline float sqrt_newton(float a) {
    if (a <= 0.0f) return 0.0f;
    float x = a * 0.5f;
    for (int i = 0; i < 6; i++) {
        x = 0.5f * (x + a / x);
    }
    return x;
}

/**
 * Fast inverse square root (Quake III algorithm): computes 1/sqrt(a),
 * then multiplies by a to get sqrt(a). Famous for its magic constant.
 * 2 Newton refinement iterations after the bit hack.
 */
inline float sqrt_fast_inv(float a) {
    if (a <= 0.0f) return 0.0f;

    float half_a = 0.5f * a;
    uint32_t i;
    std::memcpy(&i, &a, 4);
    i = 0x5f3759df - (i >> 1);  // The magic constant
    float y;
    std::memcpy(&y, &i, 4);

    // Two Newton refinement steps for 1/sqrt(a)
    y = y * (1.5f - half_a * y * y);
    y = y * (1.5f - half_a * y * y);

    return a * y;  // sqrt(a) = a * (1/sqrt(a))
}

/**
 * CORDIC square root: uses the hyperbolic CORDIC vectoring mode.
 * Computes sqrt(a) by driving y to zero in the (x,y) plane.
 * 20 iterations with precomputed atanh gain correction.
 */
inline float sqrt_cordic(float a) {
    if (a <= 0.0f) return 0.0f;

    // Normalize a to [0.25, 1) for convergence, track scale
    int scale = 0;
    float v = a;
    while (v >= 1.0f) { v *= 0.25f; scale++; }
    while (v < 0.25f) { v *= 4.0f; scale--; }

    // Hyperbolic CORDIC: compute sqrt using x = v + 0.25, y = v - 0.25
    // After convergence, x ≈ sqrt(v) * gain
    float x = v + 0.25f;
    float y = v - 0.25f;

    for (int i = 1; i <= 20; i++) {
        // In hyperbolic CORDIC, repeat iterations 4, 13, ... for convergence
        int k = i;
        float sigma = (y < 0) ? 1.0f : -1.0f;
        float shift = std::ldexp(1.0f, -k);
        float x_new = x + sigma * y * shift;
        float y_new = y + sigma * x * shift;
        x = x_new;
        y = y_new;
    }

    // CORDIC gain for hyperbolic mode (precomputed)
    float gain = 0.82816f;
    float result = x * gain;

    // Undo scaling: each scale step was *4 on v, so sqrt scales by *2
    return std::ldexp(result, scale);
}

/**
 * LUT-based sqrt: 256-entry table for sqrt(x) where x in [1, 4),
 * with linear interpolation. Simulates ROM-based sqrt units.
 */
inline float sqrt_lut(float a) {
    if (a <= 0.0f) return 0.0f;

    int exp_a;
    float frac_a = std::frexp(a, &exp_a);
    // Adjust so frac is in [1, 4) and exponent is even
    if (exp_a % 2 != 0) {
        frac_a *= 2.0f;
        exp_a -= 1;
    } else {
        frac_a *= 4.0f;
        exp_a -= 2;
    }
    // frac_a is now in [1, 4), exp_a is even

    static float lut[256];
    static bool lut_init = false;
    if (!lut_init) {
        for (int i = 0; i < 256; i++) {
            float x = 1.0f + 3.0f * static_cast<float>(i) / 255.0f;
            lut[i] = std::sqrt(x);
        }
        lut_init = true;
    }

    float idx_f = (frac_a - 1.0f) * 255.0f / 3.0f;
    int idx = static_cast<int>(idx_f);
    idx = std::clamp(idx, 0, 254);
    float frac = idx_f - idx;
    float result = lut[idx] * (1.0f - frac) + lut[idx + 1] * frac;

    return std::ldexp(result, exp_a / 2);
}

// === TRIGONOMETRY ===

/**
 * Taylor series sin(x): uses Maclaurin series up to x^11 term.
 * Reduces x to [-pi, pi] first. Accuracy degrades for large x.
 */
inline float trig_taylor_sin(float x) {
    // Range reduction to [-pi, pi]
    const float PI = 3.14159265358979f;
    x = std::fmod(x, 2.0f * PI);
    if (x > PI) x -= 2.0f * PI;
    if (x < -PI) x += 2.0f * PI;

    // sin(x) ≈ x - x^3/6 + x^5/120 - x^7/5040 + x^9/362880 - x^11/39916800
    float x2 = x * x;
    float result = x;
    float term = x;
    term *= -x2 / 6.0f;    result += term;   // x^3
    term *= -x2 / 20.0f;   result += term;   // x^5
    term *= -x2 / 42.0f;   result += term;   // x^7
    term *= -x2 / 72.0f;   result += term;   // x^9
    term *= -x2 / 110.0f;  result += term;   // x^11
    return result;
}

inline float trig_taylor_cos(float x) {
    const float PI = 3.14159265358979f;
    // cos(x) = sin(x + pi/2)
    return trig_taylor_sin(x + PI / 2.0f);
}

/**
 * CORDIC trigonometry: rotational mode computes sin and cos simultaneously.
 * 20 iterations using precomputed arctan(2^-i) angles.
 * Returns sin or cos based on the which parameter.
 */
inline void trig_cordic_sincos(float angle, float& out_sin, float& out_cos) {
    // Precomputed arctan(2^-i) for i = 0..19
    static const float atan_table[] = {
        0.78539816f, 0.46364761f, 0.24497866f, 0.12435499f,
        0.06241881f, 0.03123983f, 0.01562373f, 0.00781234f,
        0.00390623f, 0.00195312f, 0.00097656f, 0.00048828f,
        0.00024414f, 0.00012207f, 0.00006104f, 0.00003052f,
        0.00001526f, 0.00000763f, 0.00000381f, 0.00000191f
    };
    // CORDIC gain K_20 ≈ 0.60725294
    static const float K = 0.60725294f;

    // Reduce angle to [-pi, pi]
    const float PI = 3.14159265358979f;
    float a = std::fmod(angle, 2.0f * PI);
    if (a > PI) a -= 2.0f * PI;
    if (a < -PI) a += 2.0f * PI;

    // Handle quadrants: reduce to [-pi/2, pi/2]
    bool flip = false;
    if (a > PI / 2.0f) { a = PI - a; flip = true; }
    else if (a < -PI / 2.0f) { a = -PI - a; flip = true; }

    float x = K;  // Will converge to cos(angle)
    float y = 0.0f;  // Will converge to sin(angle)
    float z = a;     // Remaining angle to rotate

    for (int i = 0; i < 20; i++) {
        float d = (z >= 0) ? 1.0f : -1.0f;
        float x_new = x - d * y * std::ldexp(1.0f, -i);
        float y_new = y + d * x * std::ldexp(1.0f, -i);
        z -= d * atan_table[i];
        x = x_new;
        y = y_new;
    }

    out_cos = flip ? -x : x;
    out_sin = y;
}

inline float trig_cordic_sin(float x) {
    float s, c;
    trig_cordic_sincos(x, s, c);
    return s;
}

inline float trig_cordic_cos(float x) {
    float s, c;
    trig_cordic_sincos(x, s, c);
    return c;
}

/**
 * LUT-based trig: 1024-entry sine table covering [0, pi/2],
 * with linear interpolation and symmetry for full range.
 */
inline float trig_lut_sin(float x) {
    static float lut[1025];
    static bool init = false;
    if (!init) {
        const float PI_2 = 1.57079632679f;
        for (int i = 0; i <= 1024; i++) {
            lut[i] = std::sin(PI_2 * i / 1024.0f);
        }
        init = true;
    }

    const float PI = 3.14159265358979f;
    const float TWO_PI = 6.28318530718f;

    // Reduce to [0, 2*pi)
    x = std::fmod(x, TWO_PI);
    if (x < 0) x += TWO_PI;

    // Use symmetry to map to [0, pi/2]
    float sign = 1.0f;
    if (x > PI) { x -= PI; sign = -1.0f; }
    if (x > PI / 2.0f) { x = PI - x; }

    float idx_f = x / (PI / 2.0f) * 1024.0f;
    int idx = static_cast<int>(idx_f);
    idx = std::clamp(idx, 0, 1023);
    float frac = idx_f - idx;

    return sign * (lut[idx] * (1.0f - frac) + lut[idx + 1] * frac);
}

inline float trig_lut_cos(float x) {
    const float PI_2 = 1.57079632679f;
    return trig_lut_sin(x + PI_2);
}

/**
 * Chebyshev polynomial approximation for sin(x) on [-pi, pi].
 * Uses precomputed Chebyshev coefficients for a degree-7 polynomial.
 * More uniform error distribution than Taylor series across the interval.
 */
inline float trig_cheb_sin(float x) {
    const float PI = 3.14159265358979f;
    x = std::fmod(x, 2.0f * PI);
    if (x > PI) x -= 2.0f * PI;
    if (x < -PI) x += 2.0f * PI;

    // Chebyshev approximation for sin(x) on [-pi, pi]
    // Normalized to [-1, 1]: t = x / pi
    float t = x / PI;

    // Precomputed coefficients for the Chebyshev expansion of sin(pi*t)
    // sin(pi*t) ≈ c1*t + c3*t^3 + c5*t^5 + c7*t^7  (odd function)
    float t2 = t * t;
    float result = t * (3.14159265f - 5.16771278f * t2
                       + 2.55016403f * t2 * t2
                       - 0.59926453f * t2 * t2 * t2);
    return result;
}

inline float trig_cheb_cos(float x) {
    const float PI_2 = 1.57079632679f;
    return trig_cheb_sin(x + PI_2);
}

/**
 * CORDIC atan2: vectoring mode drives y to zero, accumulating the angle.
 */
inline float trig_cordic_atan2(float y, float x) {
    static const float atan_table[] = {
        0.78539816f, 0.46364761f, 0.24497866f, 0.12435499f,
        0.06241881f, 0.03123983f, 0.01562373f, 0.00781234f,
        0.00390623f, 0.00195312f, 0.00097656f, 0.00048828f,
        0.00024414f, 0.00012207f, 0.00006104f, 0.00003052f,
        0.00001526f, 0.00000763f, 0.00000381f, 0.00000191f
    };
    const float PI = 3.14159265358979f;

    if (x == 0.0f && y == 0.0f) return 0.0f;

    // Pre-rotate to first quadrant, track base angle
    float base_angle = 0.0f;
    float vx = x, vy = y;
    if (x < 0) {
        if (y >= 0) { vx = y;  vy = -x; base_angle = PI / 2.0f; }
        else         { vx = -x; vy = -y; base_angle = PI; }
    } else if (y < 0) {
        vx = -y; vy = x; base_angle = -PI / 2.0f;
    }

    // Vectoring mode: rotate (vx, vy) to drive vy → 0
    float z = 0.0f;
    for (int i = 0; i < 20; i++) {
        float d = (vy >= 0) ? 1.0f : -1.0f;
        float vx_new = vx + d * vy * std::ldexp(1.0f, -i);
        float vy_new = vy - d * vx * std::ldexp(1.0f, -i);
        z += d * atan_table[i];
        vx = vx_new;
        vy = vy_new;
    }

    float result = base_angle + z;
    // Normalize to [-pi, pi]
    if (result > PI) result -= 2.0f * PI;
    if (result < -PI) result += 2.0f * PI;
    return result;
}

inline float trig_lut_atan2(float y, float x) {
    // 512-entry atan LUT covering [0, 1] mapped from atan(t)
    static float lut[513];
    static bool init = false;
    if (!init) {
        for (int i = 0; i <= 512; i++) {
            lut[i] = std::atan(static_cast<float>(i) / 512.0f);
        }
        init = true;
    }
    const float PI = 3.14159265358979f;

    if (x == 0.0f && y == 0.0f) return 0.0f;

    float abs_x = std::fabs(x), abs_y = std::fabs(y);
    bool swapped = false;
    if (abs_y > abs_x) { std::swap(abs_x, abs_y); swapped = true; }

    float ratio = (abs_x > 0.0f) ? abs_y / abs_x : 0.0f;
    float idx_f = ratio * 512.0f;
    int idx = std::clamp(static_cast<int>(idx_f), 0, 511);
    float frac = idx_f - idx;
    float angle = lut[idx] * (1.0f - frac) + lut[idx + 1] * frac;

    if (swapped) angle = PI / 2.0f - angle;
    if (x < 0) angle = PI - angle;
    if (y < 0) angle = -angle;
    return angle;
}

inline float trig_taylor_atan2(float y, float x) {
    // Use polynomial approximation for atan, then reconstruct atan2
    const float PI = 3.14159265358979f;
    if (x == 0.0f && y == 0.0f) return 0.0f;
    if (x == 0.0f) return (y > 0) ? PI / 2.0f : -PI / 2.0f;

    float ratio = y / x;
    float abs_r = std::fabs(ratio);

    // For |ratio| > 1, use atan(x) = pi/2 - atan(1/x)
    bool large = abs_r > 1.0f;
    float t = large ? 1.0f / abs_r : abs_r;

    // Polynomial: atan(t) ≈ t - t^3/3 + t^5/5 - t^7/7 + t^9/9
    float t2 = t * t;
    float result = t * (1.0f - t2 * (1.0f/3.0f - t2 * (1.0f/5.0f
                       - t2 * (1.0f/7.0f - t2 / 9.0f))));

    if (large) result = PI / 2.0f - result;
    if (ratio < 0) result = -result;
    if (x < 0) result += (y >= 0) ? PI : -PI;
    return result;
}

inline float trig_cheb_atan2(float y, float x) {
    // Chebyshev-like polynomial for atan on [0, 1]
    const float PI = 3.14159265358979f;
    if (x == 0.0f && y == 0.0f) return 0.0f;
    if (x == 0.0f) return (y > 0) ? PI / 2.0f : -PI / 2.0f;

    float ratio = y / x;
    float abs_r = std::fabs(ratio);
    bool large = abs_r > 1.0f;
    float t = large ? 1.0f / abs_r : abs_r;

    // Better polynomial: atan(t) ≈ t*(a + b*t^2 + c*t^4) for t in [0,1]
    float t2 = t * t;
    float result = t * (0.99997726f + t2 * (-0.33262347f + t2 * (0.19354346f
                       + t2 * (-0.11643287f + t2 * 0.05265332f))));

    if (large) result = PI / 2.0f - result;
    if (ratio < 0) result = -result;
    if (x < 0) result += (y >= 0) ? PI : -PI;
    return result;
}

// === LOG / EXP ===

inline float log_standard(float x) {
    return (x > 0.0f) ? std::log(x) : -1e30f;
}

/**
 * Taylor series ln(x): first reduces to ln(1+t) where t = (x-1)/(x+1),
 * then uses the series 2*(t + t^3/3 + t^5/5 + ... + t^13/13).
 * Converges well for x > 0 after range reduction via frexp.
 */
inline float log_taylor(float x) {
    if (x <= 0.0f) return -1e30f;

    int exp_x;
    float frac = std::frexp(x, &exp_x);  // x = frac * 2^exp, frac in [0.5, 1)
    frac *= 2.0f;
    exp_x -= 1;
    // Now x = frac * 2^exp_x, frac in [1, 2)

    // ln(x) = exp_x * ln(2) + ln(frac)
    // ln(frac) using series: t = (frac-1)/(frac+1)
    float t = (frac - 1.0f) / (frac + 1.0f);
    float t2 = t * t;
    // 2*(t + t^3/3 + t^5/5 + t^7/7 + t^9/9 + t^11/11 + t^13/13)
    float sum = t;
    float power = t;
    power *= t2; sum += power / 3.0f;
    power *= t2; sum += power / 5.0f;
    power *= t2; sum += power / 7.0f;
    power *= t2; sum += power / 9.0f;
    power *= t2; sum += power / 11.0f;
    power *= t2; sum += power / 13.0f;
    sum *= 2.0f;

    return exp_x * 0.69314718f + sum;  // ln(2) ≈ 0.693147
}

inline float log_lut(float x) {
    if (x <= 0.0f) return -1e30f;

    int exp_x;
    float frac = std::frexp(x, &exp_x);
    frac *= 2.0f;
    exp_x -= 1;

    // LUT for ln(x) where x in [1, 2)
    static float lut[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; i++) {
            float v = 1.0f + static_cast<float>(i) / 256.0f;
            lut[i] = std::log(v);
        }
        init = true;
    }

    float idx_f = (frac - 1.0f) * 256.0f;
    int idx = std::clamp(static_cast<int>(idx_f), 0, 254);
    float f = idx_f - idx;
    float ln_frac = lut[idx] * (1.0f - f) + lut[idx + 1] * f;

    return exp_x * 0.69314718f + ln_frac;
}

inline float exp_standard(float x) {
    return std::exp(x);
}

/**
 * Taylor series exp(x): range-reduce using exp(x) = 2^k * exp(r)
 * where x = k*ln(2) + r, then compute exp(r) by Taylor series.
 */
inline float exp_taylor(float x) {
    if (x > 88.0f) return 1e30f;
    if (x < -88.0f) return 0.0f;

    // Range reduction: x = k*ln2 + r, |r| < ln2/2
    float k_f = std::round(x / 0.69314718f);
    int k = static_cast<int>(k_f);
    float r = x - k_f * 0.69314718f;

    // exp(r) ≈ 1 + r + r^2/2 + r^3/6 + r^4/24 + r^5/120 + r^6/720 + r^7/5040
    float result = 1.0f + r * (1.0f + r * (0.5f + r * (1.0f/6.0f
                   + r * (1.0f/24.0f + r * (1.0f/120.0f
                   + r * (1.0f/720.0f + r / 5040.0f))))));

    return std::ldexp(result, k);
}

inline float exp_lut(float x) {
    if (x > 88.0f) return 1e30f;
    if (x < -88.0f) return 0.0f;

    float k_f = std::round(x / 0.69314718f);
    int k = static_cast<int>(k_f);
    float r = x - k_f * 0.69314718f;

    // LUT for exp(r) where r in [-ln2/2, ln2/2] ≈ [-0.347, 0.347]
    static float lut[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; i++) {
            float v = -0.35f + 0.70f * static_cast<float>(i) / 255.0f;
            lut[i] = std::exp(v);
        }
        init = true;
    }

    float idx_f = (r + 0.35f) / 0.70f * 255.0f;
    int idx = std::clamp(static_cast<int>(idx_f), 0, 254);
    float frac = idx_f - idx;
    float exp_r = lut[idx] * (1.0f - frac) + lut[idx + 1] * frac;

    return std::ldexp(exp_r, k);
}

// === RANDOM NUMBER GENERATION ===

/**
 * LFSR (Linear Feedback Shift Register): 32-bit Galois LFSR.
 * Fast, minimal hardware, but low statistical quality.
 * Polynomial: x^32 + x^22 + x^2 + x + 1
 */
struct RNG_LFSR {
    uint32_t state;
    RNG_LFSR(uint32_t seed = 0xACE1u) : state(seed ? seed : 1u) {}
    float next() {
        // Galois LFSR (taps at bits 32, 22, 2, 1)
        uint32_t bit = state & 1u;
        state >>= 1;
        if (bit) state ^= 0xD0000001u;
        return static_cast<float>(state) / static_cast<float>(0xFFFFFFFFu);
    }
};

/**
 * LCG (Linear Congruential Generator): classic MINSTD variant.
 * Very simple hardware: one multiply + one add + one modulo.
 * state = (a * state + c) mod m, with Numerical Recipes constants.
 */
struct RNG_LCG {
    uint32_t state;
    RNG_LCG(uint32_t seed = 12345u) : state(seed) {}
    float next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state) / static_cast<float>(0xFFFFFFFFu);
    }
};

/**
 * Xorshift32: George Marsaglia's fast PRNG. Three shift-xor ops.
 * Good balance of speed, quality, and hardware simplicity.
 */
struct RNG_Xorshift {
    uint32_t state;
    RNG_Xorshift(uint32_t seed = 42u) : state(seed ? seed : 1u) {}
    float next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state) / static_cast<float>(0xFFFFFFFFu);
    }
};

/**
 * Mersenne Twister (MT19937): gold standard PRNG.
 * High-quality randomness but large state (624 × 32-bit words).
 * Uses the C++ standard library implementation.
 */
struct RNG_MT {
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist{0.0f, 1.0f};
    RNG_MT(uint32_t seed = 42u) : gen(seed) {}
    float next() {
        return dist(gen);
    }
};

} // namespace math_impl

// ---------------------------------------------------------------------------
// MathEngine singleton — routes all operations through selected algorithms
// ---------------------------------------------------------------------------
class MathEngine {
public:
    // --- Method selections (public for easy configuration) ---
    AddMethod    add_method    = AddMethod::STANDARD;
    SubMethod    sub_method    = SubMethod::STANDARD;
    MulMethod    mul_method    = MulMethod::STANDARD;
    DivMethod    div_method    = DivMethod::STANDARD;
    SqrtMethod   sqrt_method   = SqrtMethod::STANDARD;
    TrigMethod   trig_method   = TrigMethod::STANDARD;
    LogExpMethod logexp_method = LogExpMethod::STANDARD;
    RngMethod    rng_method    = RngMethod::MERSENNE_TWISTER;

    // --- RNG state (one instance per engine) ---
    math_impl::RNG_LFSR     rng_lfsr;
    math_impl::RNG_LCG      rng_lcg;
    math_impl::RNG_Xorshift rng_xorshift;
    math_impl::RNG_MT       rng_mt;

    static MathEngine& instance() {
        static MathEngine engine;
        return engine;
    }

    void set_rng_seed(uint32_t seed) {
        rng_lfsr     = math_impl::RNG_LFSR(seed);
        rng_lcg      = math_impl::RNG_LCG(seed);
        rng_xorshift = math_impl::RNG_Xorshift(seed);
        rng_mt       = math_impl::RNG_MT(seed);
    }

    // --- Dispatch functions ---

    float add(float a, float b) const {
        switch (add_method) {
            case AddMethod::STANDARD:   return math_impl::add_standard(a, b);
            case AddMethod::SATURATING: return math_impl::add_saturating(a, b);
        }
        return a + b;
    }

    float sub(float a, float b) const {
        switch (sub_method) {
            case SubMethod::STANDARD:   return math_impl::sub_standard(a, b);
            case SubMethod::SATURATING: return math_impl::sub_saturating(a, b);
        }
        return a - b;
    }

    float mul(float a, float b) const {
        switch (mul_method) {
            case MulMethod::STANDARD:   return math_impl::mul_standard(a, b);
            case MulMethod::SHIFT_ADD:  return math_impl::mul_shift_add(a, b);
            case MulMethod::LOG_DOMAIN: return math_impl::mul_log_domain(a, b);
        }
        return a * b;
    }

    float div(float a, float b) const {
        switch (div_method) {
            case DivMethod::STANDARD:       return math_impl::div_standard(a, b);
            case DivMethod::NEWTON_RAPHSON: return math_impl::div_newton_raphson(a, b);
            case DivMethod::GOLDSCHMIDT:    return math_impl::div_goldschmidt(a, b);
            case DivMethod::SRT:            return math_impl::div_srt(a, b);
            case DivMethod::LUT:            return math_impl::div_lut(a, b);
        }
        return a / b;
    }

    float sqrt(float a) const {
        switch (sqrt_method) {
            case SqrtMethod::STANDARD:      return math_impl::sqrt_standard(a);
            case SqrtMethod::NEWTON:        return math_impl::sqrt_newton(a);
            case SqrtMethod::FAST_INV_SQRT: return math_impl::sqrt_fast_inv(a);
            case SqrtMethod::CORDIC:        return math_impl::sqrt_cordic(a);
            case SqrtMethod::LUT:           return math_impl::sqrt_lut(a);
        }
        return std::sqrt(a);
    }

    float sin(float x) const {
        switch (trig_method) {
            case TrigMethod::STANDARD:  return std::sin(x);
            case TrigMethod::TAYLOR:    return math_impl::trig_taylor_sin(x);
            case TrigMethod::CORDIC:    return math_impl::trig_cordic_sin(x);
            case TrigMethod::LUT:       return math_impl::trig_lut_sin(x);
            case TrigMethod::CHEBYSHEV: return math_impl::trig_cheb_sin(x);
        }
        return std::sin(x);
    }

    float cos(float x) const {
        switch (trig_method) {
            case TrigMethod::STANDARD:  return std::cos(x);
            case TrigMethod::TAYLOR:    return math_impl::trig_taylor_cos(x);
            case TrigMethod::CORDIC:    return math_impl::trig_cordic_cos(x);
            case TrigMethod::LUT:       return math_impl::trig_lut_cos(x);
            case TrigMethod::CHEBYSHEV: return math_impl::trig_cheb_cos(x);
        }
        return std::cos(x);
    }

    float atan2(float y, float x) const {
        switch (trig_method) {
            case TrigMethod::STANDARD:  return std::atan2(y, x);
            case TrigMethod::TAYLOR:    return math_impl::trig_taylor_atan2(y, x);
            case TrigMethod::CORDIC:    return math_impl::trig_cordic_atan2(y, x);
            case TrigMethod::LUT:       return math_impl::trig_lut_atan2(y, x);
            case TrigMethod::CHEBYSHEV: return math_impl::trig_cheb_atan2(y, x);
        }
        return std::atan2(y, x);
    }

    float log(float x) const {
        switch (logexp_method) {
            case LogExpMethod::STANDARD: return math_impl::log_standard(x);
            case LogExpMethod::TAYLOR:   return math_impl::log_taylor(x);
            case LogExpMethod::LUT:      return math_impl::log_lut(x);
        }
        return std::log(x);
    }

    float exp(float x) const {
        switch (logexp_method) {
            case LogExpMethod::STANDARD: return math_impl::exp_standard(x);
            case LogExpMethod::TAYLOR:   return math_impl::exp_taylor(x);
            case LogExpMethod::LUT:      return math_impl::exp_lut(x);
        }
        return std::exp(x);
    }

    /**
     * pow(base, exponent): implemented as exp(exponent * log(base)).
     * This means the pow accuracy depends on both logexp_method and
     * mul_method, compounding errors from both units.
     */
    float pow(float base, float exponent) const {
        if (base <= 0.0f) return 0.0f;
        return exp(mul(exponent, log(base)));
    }

    /**
     * Random float in [0, 1): dispatches to the selected RNG.
     * Non-const because RNG state is mutated.
     */
    float random_float() {
        switch (rng_method) {
            case RngMethod::LFSR:             return rng_lfsr.next();
            case RngMethod::LCG:              return rng_lcg.next();
            case RngMethod::XORSHIFT:         return rng_xorshift.next();
            case RngMethod::MERSENNE_TWISTER: return rng_mt.next();
        }
        return rng_mt.next();
    }

    /** Random float in [min, max) */
    float random_float(float min, float max) {
        return add(min, mul(sub(max, min), random_float()));
    }

    // --- String helpers for display ---
    static std::string add_method_name(AddMethod m) {
        switch (m) {
            case AddMethod::STANDARD:   return "Standard";
            case AddMethod::SATURATING: return "Saturating";
        } return "?";
    }
    static std::string sub_method_name(SubMethod m) {
        switch (m) {
            case SubMethod::STANDARD:   return "Standard";
            case SubMethod::SATURATING: return "Saturating";
        } return "?";
    }
    static std::string mul_method_name(MulMethod m) {
        switch (m) {
            case MulMethod::STANDARD:   return "Standard";
            case MulMethod::SHIFT_ADD:  return "Shift-and-Add";
            case MulMethod::LOG_DOMAIN: return "Log-Domain";
        } return "?";
    }
    static std::string div_method_name(DivMethod m) {
        switch (m) {
            case DivMethod::STANDARD:       return "Standard";
            case DivMethod::NEWTON_RAPHSON: return "Newton-Raphson";
            case DivMethod::GOLDSCHMIDT:    return "Goldschmidt";
            case DivMethod::SRT:            return "SRT (digit-recurrence)";
            case DivMethod::LUT:            return "LUT-interpolated";
        } return "?";
    }
    static std::string sqrt_method_name(SqrtMethod m) {
        switch (m) {
            case SqrtMethod::STANDARD:      return "Standard (hw)";
            case SqrtMethod::NEWTON:        return "Newton (Heron)";
            case SqrtMethod::FAST_INV_SQRT: return "Fast Inv Sqrt (Quake)";
            case SqrtMethod::CORDIC:        return "CORDIC";
            case SqrtMethod::LUT:           return "LUT-interpolated";
        } return "?";
    }
    static std::string trig_method_name(TrigMethod m) {
        switch (m) {
            case TrigMethod::STANDARD:  return "Standard (libm)";
            case TrigMethod::TAYLOR:    return "Taylor series";
            case TrigMethod::CORDIC:    return "CORDIC";
            case TrigMethod::LUT:       return "LUT-interpolated";
            case TrigMethod::CHEBYSHEV: return "Chebyshev polynomial";
        } return "?";
    }
    static std::string logexp_method_name(LogExpMethod m) {
        switch (m) {
            case LogExpMethod::STANDARD: return "Standard (libm)";
            case LogExpMethod::TAYLOR:   return "Taylor series";
            case LogExpMethod::LUT:      return "LUT-interpolated";
        } return "?";
    }
    static std::string rng_method_name(RngMethod m) {
        switch (m) {
            case RngMethod::LFSR:             return "LFSR (32-bit Galois)";
            case RngMethod::LCG:              return "LCG (Numerical Recipes)";
            case RngMethod::XORSHIFT:         return "Xorshift32";
            case RngMethod::MERSENNE_TWISTER: return "Mersenne Twister (MT19937)";
        } return "?";
    }

    /** Print the full configuration to stdout */
    void print_config() const {
        std::cout << "  Addition       : " << add_method_name(add_method) << "\n"
                  << "  Subtraction    : " << sub_method_name(sub_method) << "\n"
                  << "  Multiplication : " << mul_method_name(mul_method) << "\n"
                  << "  Division       : " << div_method_name(div_method) << "\n"
                  << "  Square Root    : " << sqrt_method_name(sqrt_method) << "\n"
                  << "  Trigonometry   : " << trig_method_name(trig_method) << "\n"
                  << "  Log / Exp      : " << logexp_method_name(logexp_method) << "\n"
                  << "  RNG            : " << rng_method_name(rng_method) << "\n";
    }

private:
    MathEngine() = default;
};

#endif // MATH_ENGINE_H
