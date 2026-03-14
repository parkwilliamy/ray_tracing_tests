#ifndef FIXED8_H
#define FIXED8_H

#include <cstdint>
#include <cmath>
#include <iostream>
#include <limits>

// ============================================================================
// fixed8 — Signed Q3.4 fixed-point number in 8 bits
//
// Layout:  [S III . FFFF]
//   1 sign bit, 3 integer bits, 4 fractional bits
//   Range:  -8.0  to  +7.9375  (i.e. +7 + 15/16)
//   Resolution: 1/16 = 0.0625
//
// Overflow: truncate (mask to 8 bits), matching cheap hardware behavior.
// Multiply: 8x8 -> 16-bit intermediate, shift right by 4 (fractional bits),
//           then truncate to 8 bits.
// ============================================================================

class fixed8 {
  public:
    int8_t raw;  // The raw Q3.4 representation

    // -- Constructors --------------------------------------------------------

    fixed8() : raw(0) {}

    // Construct from a double — this is the main entry point for constants.
    fixed8(double val) {
        int rounded = static_cast<int>(std::round(val * 16.0));  // scale by 2^4
        raw = static_cast<int8_t>(rounded & 0xFF);               // truncate to 8 bits
    }

    // Construct from raw bits (private-ish, used internally)
    static fixed8 from_raw(int8_t r) {
        fixed8 f;
        f.raw = r;
        return f;
    }

    // -- Conversion back to double (for final PPM output only) ---------------

    double to_double() const {
        return static_cast<double>(raw) / 16.0;
    }

    // -- Arithmetic ----------------------------------------------------------

    // Addition: add raw values, truncate to 8 bits
    friend fixed8 operator+(fixed8 a, fixed8 b) {
        int sum = static_cast<int>(a.raw) + static_cast<int>(b.raw);
        return from_raw(static_cast<int8_t>(sum & 0xFF));
    }

    // Subtraction: subtract raw values, truncate to 8 bits
    friend fixed8 operator-(fixed8 a, fixed8 b) {
        int diff = static_cast<int>(a.raw) - static_cast<int>(b.raw);
        return from_raw(static_cast<int8_t>(diff & 0xFF));
    }

    // Unary negation
    fixed8 operator-() const {
        // Two's complement negation, truncated
        int neg = -static_cast<int>(raw);
        return from_raw(static_cast<int8_t>(neg & 0xFF));
    }

    // Multiplication: 8x8 -> 16-bit, shift right by FRAC_BITS, truncate to 8
    friend fixed8 operator*(fixed8 a, fixed8 b) {
        int product = static_cast<int>(a.raw) * static_cast<int>(b.raw);
        int shifted = product >> 4;  // shift right by fractional bits
        return from_raw(static_cast<int8_t>(shifted & 0xFF));
    }

    // Division: scale numerator up by FRAC_BITS, divide, truncate
    friend fixed8 operator/(fixed8 a, fixed8 b) {
        if (b.raw == 0) return from_raw(a.raw >= 0 ? 0x7F : static_cast<int8_t>(0x80));
        int scaled = (static_cast<int>(a.raw) << 4) / static_cast<int>(b.raw);
        return from_raw(static_cast<int8_t>(scaled & 0xFF));
    }

    // -- Compound assignment -------------------------------------------------

    fixed8& operator+=(fixed8 other) { *this = *this + other; return *this; }
    fixed8& operator-=(fixed8 other) { *this = *this - other; return *this; }
    fixed8& operator*=(fixed8 other) { *this = *this * other; return *this; }
    fixed8& operator/=(fixed8 other) { *this = *this / other; return *this; }

    // -- Comparisons ---------------------------------------------------------

    friend bool operator<(fixed8 a, fixed8 b)  { return a.raw < b.raw; }
    friend bool operator>(fixed8 a, fixed8 b)  { return a.raw > b.raw; }
    friend bool operator<=(fixed8 a, fixed8 b) { return a.raw <= b.raw; }
    friend bool operator>=(fixed8 a, fixed8 b) { return a.raw >= b.raw; }
    friend bool operator==(fixed8 a, fixed8 b) { return a.raw == b.raw; }
    friend bool operator!=(fixed8 a, fixed8 b) { return a.raw != b.raw; }

    // -- Stream output -------------------------------------------------------

    friend std::ostream& operator<<(std::ostream& out, fixed8 f) {
        out << f.to_double();
        return out;
    }
};

// ============================================================================
// Fixed-point versions of math functions
// These use double internally for transcendentals, then convert back —
// in real hardware these would be LUTs or CORDIC.
// ============================================================================

inline fixed8 fp_sqrt(fixed8 x) {
    double val = x.to_double();
    if (val <= 0.0) return fixed8(0.0);
    return fixed8(std::sqrt(val));
}

inline fixed8 fp_abs(fixed8 x) {
    return (x.raw < 0) ? -x : x;
}

inline fixed8 fp_fmin(fixed8 a, fixed8 b) {
    return (a.raw < b.raw) ? a : b;
}

inline fixed8 fp_fmax(fixed8 a, fixed8 b) {
    return (a.raw > b.raw) ? a : b;
}

inline fixed8 fp_tan(double angle_rad) {
    return fixed8(std::tan(angle_rad));
}

inline fixed8 fp_pow(fixed8 base, int exp) {
    return fixed8(std::pow(base.to_double(), exp));
}

#endif