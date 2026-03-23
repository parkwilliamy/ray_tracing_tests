/**
 * material.h — Surface Materials
 * ================================
 * Defines the three material types from RTiOW:
 *
 *   Lambertian (matte): scatters rays in random hemisphere directions,
 *     tinting them by the material's albedo. Uses the configured RNG.
 *
 *   Metal (reflective): reflects rays about the surface normal with
 *     optional fuzz. Uses the reflect() function from vec3.h.
 *
 *   Dielectric (glass): refracts or reflects rays based on Snell's law
 *     and Schlick's approximation for the Fresnel effect. This material
 *     is the most arithmetic-intensive, using sqrt, pow, and trig-adjacent
 *     operations, making it a good stress test for different math engines.
 *
 * Each material implements scatter(), which decides:
 *   1. Whether the incoming ray is absorbed (return false) or scattered
 *   2. The direction of the scattered ray
 *   3. The attenuation (color filter) applied to the scattered light
 */

#ifndef MATERIAL_H
#define MATERIAL_H

#include "geometry.h"

// ---------------------------------------------------------------------------
// Material base class
// ---------------------------------------------------------------------------
class Material {
public:
    virtual ~Material() = default;

    /**
     * Given an incoming ray and a hit record, determine:
     *   - scattered: the outgoing ray (direction + origin)
     *   - attenuation: color multiplier (how much light passes through)
     * Returns true if the ray scatters, false if it's absorbed.
     */
    virtual bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const = 0;
};

// ---------------------------------------------------------------------------
// Lambertian (diffuse/matte) material
// ---------------------------------------------------------------------------
/**
 * Lambertian scattering: the reflected ray goes in a random direction
 * biased toward the surface normal. Specifically, we add a random unit
 * vector to the surface normal — this produces the cos(θ) distribution
 * that Lambert's cosine law predicts for ideal diffuse surfaces.
 *
 * The random unit vector generation uses the configured RNG and trig
 * methods, so different RNG/trig selections will produce visibly
 * different noise patterns in the render.
 */
class Lambertian : public Material {
public:
    Color albedo;  // Surface color (each channel in [0, 1])

    Lambertian(const Color& albedo) : albedo(albedo) {}

    bool scatter(
        const Ray& /*r_in*/,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override {
        // Scatter direction: normal + random unit vector
        Vec3 scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction (random vector nearly opposite to normal)
        // This avoids NaN/inf propagation that would corrupt the image.
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scattered = Ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Metal (reflective) material
// ---------------------------------------------------------------------------
/**
 * Metal surfaces reflect rays about the surface normal. The optional
 * "fuzz" parameter adds a random perturbation to the reflected direction,
 * simulating brushed or rough metal surfaces.
 *
 * fuzz = 0: perfect mirror
 * fuzz = 1: almost diffuse (rays scattered widely)
 *
 * The reflect() function uses dot product and scalar multiply, so
 * the configured add/mul methods affect reflection accuracy.
 */
class Metal : public Material {
public:
    Color  albedo;  // Metal tint color
    Scalar fuzz;    // Roughness parameter [0, 1]

    Metal(const Color& albedo, Scalar fuzz)
        : albedo(albedo), fuzz(s_min(fuzz, Scalar(1.0f))) {}

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override {
        // Perfect reflection direction
        Vec3 reflected = reflect(unit_vector(r_in.direction), rec.normal);

        // Add fuzz: perturb the reflection with a random vector scaled by fuzz
        scattered = Ray(rec.p, reflected + fuzz * random_in_unit_sphere());
        attenuation = albedo;

        // Only scatter if the reflected ray goes outward (not into the surface)
        return dot(scattered.direction, rec.normal) > Scalar(0.0f);
    }
};

// ---------------------------------------------------------------------------
// Dielectric (glass/transparent) material
// ---------------------------------------------------------------------------
/**
 * Dielectric materials both refract AND reflect light. The proportion
 * depends on:
 *   1. The angle of incidence (Snell's law)
 *   2. The index of refraction (glass ≈ 1.5, water ≈ 1.33, diamond ≈ 2.42)
 *   3. The Fresnel effect (approximated by Schlick's formula)
 *
 * When total internal reflection occurs (angle too steep for refraction),
 * the material acts as a perfect mirror.
 *
 * Schlick's approximation: R(θ) = R0 + (1-R0)(1-cosθ)^5
 * where R0 = ((n1-n2)/(n1+n2))^2
 *
 * This is the most mathematically rich material, exercising sqrt, pow,
 * division, and RNG — ideal for testing arithmetic method impacts.
 */
class Dielectric : public Material {
public:
    Scalar ir;  // Index of refraction (relative to air = 1.0)

    Dielectric(Scalar index_of_refraction) : ir(index_of_refraction) {}

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override {
        // Glass doesn't absorb light — attenuation is white (1, 1, 1)
        attenuation = Color(1.0f, 1.0f, 1.0f);

        // Determine the ratio of refractive indices based on which side we hit.
        // If we're entering the surface: air→glass = 1/ir
        // If we're exiting the surface: glass→air = ir/1 = ir
        Scalar refraction_ratio = rec.front_face
            ? (Scalar(1.0f) / ir)
            : ir;

        Vec3 unit_direction = unit_vector(r_in.direction);
        Scalar cos_theta = s_min(dot(-unit_direction, rec.normal), Scalar(1.0f));
        Scalar sin_theta = s_sqrt(Scalar(1.0f) - cos_theta * cos_theta);

        // Total internal reflection check: Snell's law says refraction is
        // impossible when sin(θ_t) > 1, which happens when n1/n2 * sin(θ_i) > 1.
        bool cannot_refract = (refraction_ratio * sin_theta > Scalar(1.0f));

        Vec3 direction;
        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > s_random()) {
            // Reflect (either total internal reflection or Fresnel probability)
            direction = reflect(unit_direction, rec.normal);
        } else {
            // Refract through the surface
            direction = refract(unit_direction, rec.normal, refraction_ratio);
        }

        scattered = Ray(rec.p, direction);
        return true;
    }

private:
    /**
     * Schlick's approximation for Fresnel reflectance.
     * At grazing angles (cos ≈ 0), almost all light reflects.
     * At perpendicular incidence (cos ≈ 1), minimal reflection.
     *
     * Uses the configured pow() which itself chains exp(5*log(...)),
     * compounding approximation errors from multiple operation types.
     */
    static Scalar reflectance(Scalar cosine, Scalar ref_idx) {
        Scalar r0 = (Scalar(1.0f) - ref_idx) / (Scalar(1.0f) + ref_idx);
        r0 = r0 * r0;
        // (1 - cosine)^5 via MathEngine::pow
        Scalar one_minus_cos = Scalar(1.0f) - cosine;
        // Manual pow for integer exponent (more accurate than exp(5*log(...)))
        Scalar omc2 = one_minus_cos * one_minus_cos;
        Scalar omc5 = omc2 * omc2 * one_minus_cos;
        return r0 + (Scalar(1.0f) - r0) * omc5;
    }
};

#endif // MATERIAL_H
