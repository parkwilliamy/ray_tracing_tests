/**
 * scalar.h — Quantized Scalar Type
 * =================================
 * The Scalar class wraps a C++ float and enforces two constraints on every
 * arithmetic operation:
 *
 *   1. The operation is performed using the algorithm selected in MathEngine
 *   2. The result is quantized to the format selected in NumberFormatConfig
 *
 * This creates a transparent simulation of limited-precision hardware: the
 * user writes natural-looking math (a + b, a * b, etc.) and the operator
 * overloads automatically route through the configurable pipeline.
 *
 * Design note: We store the value as a C++ float internally because the
 * quantization merely rounds/clamps it. The "storage format" is simulated
 * by the quantize() call, not by actually packing bits into an int8/int16.
 * This keeps the code simple while faithfully reproducing the numerical
 * behavior of each format.
 */

#ifndef SCALAR_H
#define SCALAR_H

#include "number_format.h"
#include "math_engine.h"
#include <cmath>

class Scalar {
public:
    float v;  // The quantized value

    // --- Constructors ---

    /** Default: zero */
    Scalar() : v(0.0f) {}

    /**
     * Construct from float: immediately quantize to the configured format.
     * This means even literal constants lose precision if using fp8/Q8/etc.
     */
    Scalar(float val) : v(quantize(val)) {}

    /** Construct from double (implicit narrowing + quantize) */
    Scalar(double val) : v(quantize(static_cast<float>(val))) {}

    /** Construct from int */
    Scalar(int val) : v(quantize(static_cast<float>(val))) {}

    // --- Implicit conversion back to float (for interop) ---
    operator float() const { return v; }

    // --- Arithmetic operators ---
    // Each operator: (1) uses MathEngine for the operation, (2) quantizes result

    friend Scalar operator+(Scalar a, Scalar b) {
        return Scalar(MathEngine::instance().add(a.v, b.v));
    }

    friend Scalar operator-(Scalar a, Scalar b) {
        return Scalar(MathEngine::instance().sub(a.v, b.v));
    }

    friend Scalar operator*(Scalar a, Scalar b) {
        return Scalar(MathEngine::instance().mul(a.v, b.v));
    }

    friend Scalar operator/(Scalar a, Scalar b) {
        return Scalar(MathEngine::instance().div(a.v, b.v));
    }

    Scalar operator-() const {
        return Scalar(-v);  // Negation is just sign flip, no MathEngine needed
    }

    // --- Compound assignment ---
    Scalar& operator+=(Scalar other) { *this = *this + other; return *this; }
    Scalar& operator-=(Scalar other) { *this = *this - other; return *this; }
    Scalar& operator*=(Scalar other) { *this = *this * other; return *this; }
    Scalar& operator/=(Scalar other) { *this = *this / other; return *this; }

    // --- Comparison (uses raw float comparison — no quantization needed) ---
    friend bool operator<(Scalar a, Scalar b)  { return a.v < b.v; }
    friend bool operator>(Scalar a, Scalar b)  { return a.v > b.v; }
    friend bool operator<=(Scalar a, Scalar b) { return a.v <= b.v; }
    friend bool operator>=(Scalar a, Scalar b) { return a.v >= b.v; }
    friend bool operator==(Scalar a, Scalar b) { return a.v == b.v; }
    friend bool operator!=(Scalar a, Scalar b) { return a.v != b.v; }
};

// ---------------------------------------------------------------------------
// Math functions operating on Scalar — these all route through MathEngine
// ---------------------------------------------------------------------------

inline Scalar s_sqrt(Scalar x) {
    return Scalar(MathEngine::instance().sqrt(x.v));
}

inline Scalar s_sin(Scalar x) {
    return Scalar(MathEngine::instance().sin(x.v));
}

inline Scalar s_cos(Scalar x) {
    return Scalar(MathEngine::instance().cos(x.v));
}

inline Scalar s_atan2(Scalar y, Scalar x) {
    return Scalar(MathEngine::instance().atan2(y.v, x.v));
}

inline Scalar s_log(Scalar x) {
    return Scalar(MathEngine::instance().log(x.v));
}

inline Scalar s_exp(Scalar x) {
    return Scalar(MathEngine::instance().exp(x.v));
}

inline Scalar s_pow(Scalar base, Scalar exponent) {
    return Scalar(MathEngine::instance().pow(base.v, exponent.v));
}

inline Scalar s_abs(Scalar x) {
    return Scalar(std::fabs(x.v));
}

inline Scalar s_min(Scalar a, Scalar b) {
    return (a < b) ? a : b;
}

inline Scalar s_max(Scalar a, Scalar b) {
    return (a > b) ? a : b;
}

inline Scalar s_clamp(Scalar x, Scalar lo, Scalar hi) {
    return s_max(lo, s_min(x, hi));
}

/** Random Scalar in [0, 1) */
inline Scalar s_random() {
    return Scalar(MathEngine::instance().random_float());
}

/** Random Scalar in [min, max) */
inline Scalar s_random(Scalar min, Scalar max) {
    return Scalar(MathEngine::instance().random_float(min.v, max.v));
}

#endif // SCALAR_H
