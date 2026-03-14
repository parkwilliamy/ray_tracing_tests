#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"

class material {
  public:
    virtual ~material() = default;
    virtual bool scatter(
        const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered
    ) const {
        return false;
    }
};

class lambertian : public material {
  public:
    lambertian(const color& albedo) : albedo(albedo) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        auto scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;
        scattered = ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

  private:
    color albedo;
};

class metal : public material {
  public:
    metal(const color& albedo, double fuzz)
      : albedo(albedo), fuzz(fixed8(fuzz < 1.0 ? fuzz : 1.0)) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected = unit_vector(reflected) + fuzz * random_unit_vector();
        scattered = ray(rec.p, reflected);
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > fixed8(0.0));
    }

  private:
    color albedo;
    fixed8 fuzz;
};

class dielectric : public material {
  public:
    dielectric(double refraction_index) : refraction_index(fixed8(refraction_index)) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        attenuation = color(1.0, 1.0, 1.0);
        fixed8 ri = rec.front_face ? (fixed8(1.0) / refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(r_in.direction());
        fixed8 cos_theta = fp_fmin(-dot(unit_direction, rec.normal), fixed8(1.0));
        fixed8 sin_theta = fp_sqrt(fixed8(1.0) - cos_theta * cos_theta);

        bool cannot_refract = (ri * sin_theta) > fixed8(1.0);
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_fixed8())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        scattered = ray(rec.p, direction);
        return true;
    }

  private:
    fixed8 refraction_index;

    static fixed8 reflectance(fixed8 cosine, fixed8 refraction_index) {
        // Schlick's approximation
        auto r0 = (fixed8(1.0) - refraction_index) / (fixed8(1.0) + refraction_index);
        r0 = r0 * r0;
        fixed8 one_minus_cos = fixed8(1.0) - cosine;
        // (1-cos)^5 — do it iteratively to stay in fixed8
        fixed8 p = one_minus_cos;
        p = p * one_minus_cos;  // ^2
        p = p * one_minus_cos;  // ^3
        p = p * one_minus_cos;  // ^4
        p = p * one_minus_cos;  // ^5
        return r0 + (fixed8(1.0) - r0) * p;
    }
};

#endif