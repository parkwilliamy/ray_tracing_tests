/**
 * vec3.h — 3D Vector Using Quantized Scalars
 * ============================================
 * Standard 3D vector class, but every component is a Scalar. This means all
 * vector math (dot products, cross products, normalization) flows through the
 * configurable MathEngine and gets quantized to the selected number format.
 *
 * Also serves as Color (RGB) and Point3 via type aliases at the bottom.
 */

#ifndef VEC3_H
#define VEC3_H

#include "scalar.h"
#include <iostream>
#include <cmath>

class Vec3 {
public:
    Scalar x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(Scalar x, Scalar y, Scalar z) : x(x), y(y), z(z) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    // --- Element access ---
    Scalar  operator[](int i) const { return (i == 0) ? x : (i == 1) ? y : z; }
    Scalar& operator[](int i)       { return (i == 0) ? x : (i == 1) ? y : z; }

    // --- Arithmetic ---
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }  // component-wise
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(Scalar t) { x *= t; y *= t; z *= t; return *this; }
    Vec3& operator/=(Scalar t) { x /= t; y /= t; z /= t; return *this; }

    // --- Scalar-vector ops (defined as friends for commutativity) ---
    friend Vec3 operator*(Scalar t, const Vec3& v) { return {t * v.x, t * v.y, t * v.z}; }
    friend Vec3 operator*(const Vec3& v, Scalar t) { return {v.x * t, v.y * t, v.z * t}; }
    friend Vec3 operator/(const Vec3& v, Scalar t) { return {v.x / t, v.y / t, v.z / t}; }

    // --- Geometric operations ---

    /** Squared length — avoids a sqrt, used heavily in ray-sphere intersection */
    Scalar length_squared() const {
        return x * x + y * y + z * z;
    }

    /** Euclidean length — uses the configured sqrt method */
    Scalar length() const {
        return s_sqrt(length_squared());
    }

    /** Returns true if the vector is close to zero in all dimensions */
    bool near_zero() const {
        Scalar eps(1e-8f);
        return (s_abs(x) < eps) && (s_abs(y) < eps) && (s_abs(z) < eps);
    }
};

// ---------------------------------------------------------------------------
// Free-function vector utilities
// ---------------------------------------------------------------------------

/** Dot product: a·b = ax*bx + ay*by + az*bz */
inline Scalar dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** Cross product: a × b */
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

/** Unit vector (normalized) — uses configured div and sqrt */
inline Vec3 unit_vector(const Vec3& v) {
    Scalar len = v.length();
    if (len.v == 0.0f) return Vec3(0, 0, 0);
    return v / len;
}

/**
 * Reflect vector v about surface normal n.
 * Formula: v - 2*dot(v,n)*n
 * Used by metal material for mirror-like reflection.
 */
inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - Scalar(2.0f) * dot(v, n) * n;
}

/**
 * Refract vector uv through surface with normal n at ratio etai_over_etat.
 * Uses Snell's law decomposition into perpendicular and parallel components.
 * This is critical for dielectric (glass) materials.
 */
inline Vec3 refract(const Vec3& uv, const Vec3& n, Scalar etai_over_etat) {
    Scalar cos_theta = s_min(dot(-uv, n), Scalar(1.0f));
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Scalar perp_sq = r_out_perp.length_squared();
    Vec3 r_out_parallel = -s_sqrt(s_abs(Scalar(1.0f) - perp_sq)) * n;
    return r_out_perp + r_out_parallel;
}

// ---------------------------------------------------------------------------
// Random vector generation — uses configured RNG
// ---------------------------------------------------------------------------

/** Random vector with each component in [0, 1) */
inline Vec3 random_vec3() {
    return Vec3(s_random(), s_random(), s_random());
}

/** Random vector with each component in [min, max) */
inline Vec3 random_vec3(Scalar min, Scalar max) {
    return Vec3(s_random(min, max), s_random(min, max), s_random(min, max));
}

/**
 * Random point inside unit sphere using rejection sampling.
 * Keep generating random points in the [-1,1] cube until one falls
 * inside the unit sphere. This is the standard approach from RTiOW.
 */
inline Vec3 random_in_unit_sphere() {
    while (true) {
        Vec3 p = random_vec3(Scalar(-1.0f), Scalar(1.0f));
        if (p.length_squared() < Scalar(1.0f))
            return p;
    }
}

/** Random unit vector: normalize a random point on the sphere surface */
inline Vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

/**
 * Random unit vector in the hemisphere defined by normal.
 * If the random vector points away from the normal, flip it.
 * Used by Lambertian scattering.
 */
inline Vec3 random_in_hemisphere(const Vec3& normal) {
    Vec3 in_sphere = random_in_unit_sphere();
    if (dot(in_sphere, normal) > Scalar(0.0f))
        return in_sphere;
    else
        return -in_sphere;
}

/** Random point in unit disk (for depth of field, not currently used) */
inline Vec3 random_in_unit_disk() {
    while (true) {
        Vec3 p(s_random(Scalar(-1), Scalar(1)), s_random(Scalar(-1), Scalar(1)), Scalar(0));
        if (p.length_squared() < Scalar(1.0f))
            return p;
    }
}

// Type aliases for semantic clarity
using Point3 = Vec3;
using Color  = Vec3;

#endif // VEC3_H
