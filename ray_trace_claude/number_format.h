/**
 * number_format.h — Number Format Quantization Layer
 * ===================================================
 * This header defines the configurable number storage formats used throughout
 * the ray tracer. Every scalar value passes through a quantize() function after
 * each arithmetic operation, simulating the precision loss of hardware with
 * limited-width datapaths.
 *
 * Supported formats:
 *   FP32   — IEEE 754 single-precision (1-5-10 → effectively no quantization)
 *   FP16   — IEEE 754 half-precision  (1-5-10)
 *   FP8    — E4M3 minifloat           (1-4-3), popular in ML accelerators
 *   Q8n.m  — 8-bit signed fixed point  (1 sign + n-1 int + m frac, n+m = 8)
 *   Q16n.m — 16-bit signed fixed point (1 sign + n-1 int + m frac, n+m = 16)
 *
 * Design rationale: We store everything internally as C++ float (fp32) and
 * apply quantization after every operation. This isolates the precision-loss
 * simulation from the algorithmic-error simulation in math_engine.h, giving
 * the user two independent knobs to study render quality degradation.
 */

#ifndef NUMBER_FORMAT_H
#define NUMBER_FORMAT_H

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <algorithm>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Format enumeration
// ---------------------------------------------------------------------------
enum class NumberFormatType {
    FP32,    // IEEE 754 single precision — identity quantization
    FP16,    // IEEE 754 half precision
    FP8,     // E4M3 minifloat
    Q8,      // 8-bit signed fixed point
    Q16      // 16-bit signed fixed point
};

// ---------------------------------------------------------------------------
// Global number format configuration (singleton-style)
// ---------------------------------------------------------------------------
struct NumberFormatConfig {
    NumberFormatType type = NumberFormatType::FP32;

    // Fixed-point parameters: integer bits and fractional bits.
    // For Q8: int_bits + frac_bits = 8.  For Q16: int_bits + frac_bits = 16.
    // The MSB of int_bits is the sign bit.
    int int_bits = 4;   // default: Q8 4.4 or Q16 8.8
    int frac_bits = 4;

    static NumberFormatConfig& instance() {
        static NumberFormatConfig cfg;
        return cfg;
    }

    /**
     * Parse a format string like "fp32", "fp16", "fp8", "Q8n.m", "Q16n.m"
     * where n and m are single digits.
     */
    void parse(const std::string& fmt) {
        if (fmt == "fp32") {
            type = NumberFormatType::FP32;
        } else if (fmt == "fp16") {
            type = NumberFormatType::FP16;
        } else if (fmt == "fp8") {
            type = NumberFormatType::FP8;
        } else if (fmt.substr(0, 2) == "Q8" || fmt.substr(0, 3) == "Q16") {
            // Parse "Q8n.m" or "Q16n.m"
            bool is16 = (fmt[1] == '1' && fmt[2] == '6');
            type = is16 ? NumberFormatType::Q16 : NumberFormatType::Q8;
            int total = is16 ? 16 : 8;

            // Find the digits after "Q8" or "Q16"
            size_t start = is16 ? 3 : 2;
            size_t dot = fmt.find('.', start);
            if (dot == std::string::npos)
                throw std::runtime_error("Fixed-point format must be QXn.m");
            int_bits = std::stoi(fmt.substr(start, dot - start));
            frac_bits = std::stoi(fmt.substr(dot + 1));
            if (int_bits + frac_bits != total)
                throw std::runtime_error("n + m must equal " + std::to_string(total));
        } else {
            throw std::runtime_error("Unknown number format: " + fmt);
        }
    }

    std::string to_string() const {
        switch (type) {
            case NumberFormatType::FP32: return "fp32";
            case NumberFormatType::FP16: return "fp16";
            case NumberFormatType::FP8:  return "fp8";
            case NumberFormatType::Q8:
                return "Q8" + std::to_string(int_bits) + "." + std::to_string(frac_bits);
            case NumberFormatType::Q16:
                return "Q16" + std::to_string(int_bits) + "." + std::to_string(frac_bits);
        }
        return "unknown";
    }
};

// ---------------------------------------------------------------------------
// Quantization functions for each format
// ---------------------------------------------------------------------------
namespace quantize_impl {

/**
 * FP16 quantization: Convert float to IEEE 754 half-precision and back.
 * Layout: 1 sign | 5 exponent (bias 15) | 10 mantissa
 * This is a faithful bit-level conversion, not just rounding.
 */
inline float quantize_fp16(float val) {
    // Handle special cases
    if (std::isnan(val)) return val;
    if (std::isinf(val)) return val;
    if (val == 0.0f) return val;

    // Extract float32 bits
    uint32_t fbits;
    std::memcpy(&fbits, &val, sizeof(fbits));
    uint32_t sign = (fbits >> 31) & 1;
    int32_t  exp  = ((fbits >> 23) & 0xFF) - 127;  // unbiased exponent
    uint32_t mant = fbits & 0x7FFFFF;               // 23-bit mantissa

    uint16_t hbits = sign << 15;

    if (exp > 15) {
        // Overflow → infinity in fp16
        hbits |= 0x7C00;
    } else if (exp < -14) {
        // Underflow → denorm or zero in fp16
        // Denorm: shift mantissa right and add implicit 1
        int shift = -14 - exp;
        uint32_t denorm_mant = (mant | 0x800000) >> (shift + 13);
        hbits |= (denorm_mant & 0x3FF);
    } else {
        // Normal number: rebiased exponent + truncated mantissa
        hbits |= ((exp + 15) & 0x1F) << 10;
        hbits |= (mant >> 13) & 0x3FF;
    }

    // Convert fp16 bits back to float for internal use
    uint32_t h_sign = (hbits >> 15) & 1;
    uint32_t h_exp  = (hbits >> 10) & 0x1F;
    uint32_t h_mant = hbits & 0x3FF;

    float result;
    if (h_exp == 0) {
        // Denorm or zero
        result = std::ldexp(static_cast<float>(h_mant), -24);  // 2^(-14-10)
    } else if (h_exp == 0x1F) {
        result = (h_mant == 0) ? INFINITY : NAN;
    } else {
        result = std::ldexp(static_cast<float>(h_mant | 0x400), h_exp - 25);
    }
    return h_sign ? -result : result;
}

/**
 * FP8 E4M3 quantization: 1 sign | 4 exponent (bias 7) | 3 mantissa
 * Range: roughly ±448, minimum positive ≈ 2^-9
 * No infinity representation; NaN is 0x7F / 0xFF only.
 */
inline float quantize_fp8(float val) {
    if (std::isnan(val)) return val;
    if (val == 0.0f) return val;

    float sign_f = (val < 0) ? -1.0f : 1.0f;
    float abs_val = std::fabs(val);

    // Max representable value in E4M3: (1 + 7/8) * 2^8 = 448
    if (abs_val > 448.0f) abs_val = 448.0f;

    uint32_t fbits;
    std::memcpy(&fbits, &abs_val, sizeof(fbits));
    int32_t exp = ((fbits >> 23) & 0xFF) - 127;  // unbiased
    uint32_t mant = fbits & 0x7FFFFF;

    uint8_t e8bits = 0;

    if (exp > 8) {
        // Clamp to max
        e8bits = 0x7E;  // max normal: exp=15-7=8, mant=110 → (1+6/8)*256=448
    } else if (exp < -6) {
        // Denorm region (exp field = 0)
        int shift = -6 - exp;
        uint32_t denorm_mant = (mant | 0x800000) >> (shift + 20);
        e8bits = denorm_mant & 0x07;
    } else {
        // Normal: biased exp in [1..14], 3-bit mantissa
        e8bits = ((exp + 7) & 0x0F) << 3;
        e8bits |= (mant >> 20) & 0x07;
    }

    // Decode back to float
    uint8_t d_exp  = (e8bits >> 3) & 0x0F;
    uint8_t d_mant = e8bits & 0x07;

    float result;
    if (d_exp == 0) {
        result = std::ldexp(static_cast<float>(d_mant), -9);  // 2^(-6-3)
    } else {
        result = std::ldexp(static_cast<float>(d_mant | 0x08), d_exp - 10);
    }
    return sign_f * result;
}

/**
 * Fixed-point quantization: simulate Qn.m signed fixed-point.
 * The value is clamped to the representable range, then rounded to the
 * nearest representable step (1 / 2^frac_bits).
 */
inline float quantize_fixed(float val, int /*total_bits*/, int int_bits, int frac_bits) {
    // Representable range for signed fixed point:
    // min = -(2^(int_bits-1)),  max = 2^(int_bits-1) - 2^(-frac_bits)
    float scale = static_cast<float>(1 << frac_bits);
    float max_val = static_cast<float>((1 << (int_bits - 1))) - 1.0f / scale;
    float min_val = -static_cast<float>((1 << (int_bits - 1)));

    val = std::clamp(val, min_val, max_val);

    // Quantize: round to nearest step
    float quantized = std::round(val * scale) / scale;
    return quantized;
}

} // namespace quantize_impl

// ---------------------------------------------------------------------------
// Master quantize function — dispatches based on global config
// ---------------------------------------------------------------------------
inline float quantize(float val) {
    const auto& cfg = NumberFormatConfig::instance();
    switch (cfg.type) {
        case NumberFormatType::FP32:
            return val;  // No quantization loss
        case NumberFormatType::FP16:
            return quantize_impl::quantize_fp16(val);
        case NumberFormatType::FP8:
            return quantize_impl::quantize_fp8(val);
        case NumberFormatType::Q8:
            return quantize_impl::quantize_fixed(val, 8, cfg.int_bits, cfg.frac_bits);
        case NumberFormatType::Q16:
            return quantize_impl::quantize_fixed(val, 16, cfg.int_bits, cfg.frac_bits);
    }
    return val;
}

#endif // NUMBER_FORMAT_H
