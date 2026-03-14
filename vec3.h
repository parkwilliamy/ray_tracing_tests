#ifndef VEC3_H
#define VEC3_H

#include "rtweekend.h"

class vec3 {
  public:
    fixed8 e[3];

    vec3() : e{fixed8(0), fixed8(0), fixed8(0)} {}
    vec3(fixed8 e0, fixed8 e1, fixed8 e2) : e{e0, e1, e2} {}
    vec3(double e0, double e1, double e2) : e{fixed8(e0), fixed8(e1), fixed8(e2)} {}

    fixed8 x() const { return e[0]; }
    fixed8 y() const { return e[1]; }
    fixed8 z() const { return e[2]; }

    vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }
    fixed8 operator[](int i) const { return e[i]; }
    fixed8& operator[](int i) { return e[i]; }

    vec3& operator+=(const vec3& v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    vec3& operator*=(fixed8 t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    vec3& operator/=(fixed8 t) {
        fixed8 inv = fixed8(1.0) / t;
        return *this *= inv;
    }

    fixed8 length() const {
        return fp_sqrt(length_squared());
    }

    fixed8 length_squared() const {
        return e[0]*e[0] + e[1]*e[1] + e[2]*e[2];
    }

    bool near_zero() const {
        fixed8 s = fixed8(0.0625);  // smallest representable positive Q3.4 value (1/16)
        return (fp_abs(e[0]) < s) && (fp_abs(e[1]) < s) && (fp_abs(e[2]) < s);
    }

    static vec3 random() {
        return vec3(random_fixed8(), random_fixed8(), random_fixed8());
    }

    static vec3 random(fixed8 mn, fixed8 mx) {
        return vec3(random_fixed8(mn, mx), random_fixed8(mn, mx), random_fixed8(mn, mx));
    }
};

using point3 = vec3;

// Vector Utility Functions

inline std::ostream& operator<<(std::ostream& out, const vec3& v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline vec3 operator+(const vec3& u, const vec3& v) {
    return vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline vec3 operator-(const vec3& u, const vec3& v) {
    return vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline vec3 operator*(const vec3& u, const vec3& v) {
    return vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline vec3 operator*(fixed8 t, const vec3& v) {
    return vec3(t*v.e[0], t*v.e[1], t*v.e[2]);
}

inline vec3 operator*(const vec3& v, fixed8 t) {
    return t * v;
}

inline vec3 operator/(const vec3& v, fixed8 t) {
    fixed8 inv = fixed8(1.0) / t;
    return inv * v;
}

inline fixed8 dot(const vec3& u, const vec3& v) {
    return u.e[0] * v.e[0]
         + u.e[1] * v.e[1]
         + u.e[2] * v.e[2];
}

inline vec3 cross(const vec3& u, const vec3& v) {
    return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline vec3 unit_vector(const vec3& v) {
    return v / v.length();
}

inline vec3 random_unit_vector() {
    // Rejection sampling in the fixed8 domain
    while (true) {
        auto p = vec3::random(fixed8(-1.0), fixed8(1.0));
        auto lensq = p.length_squared();
        if (lensq > fixed8(0.0625) && lensq <= fixed8(1.0))
            return p / fp_sqrt(lensq);
    }
}

inline vec3 random_on_hemisphere(const vec3& normal) {
    vec3 on_unit_sphere = random_unit_vector();
    if (dot(on_unit_sphere, normal) > fixed8(0.0))
        return on_unit_sphere;
    else
        return -on_unit_sphere;
}

inline vec3 reflect(const vec3& v, const vec3& n) {
    return v - fixed8(2.0)*dot(v,n)*n;
}

inline vec3 refract(const vec3& uv, const vec3& n, fixed8 etai_over_etat) {
    auto cos_theta = fp_fmin(-dot(uv, n), fixed8(1.0));
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta*n);
    auto perp_lensq = r_out_perp.length_squared();
    fixed8 one = fixed8(1.0);
    fixed8 diff = one - perp_lensq;
    fixed8 par_scale = -fp_sqrt(fp_abs(diff));
    vec3 r_out_parallel = par_scale * n;
    return r_out_perp + r_out_parallel;
}

#endif