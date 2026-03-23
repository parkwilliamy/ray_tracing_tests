/**
 * geometry.h — Geometric Primitives and Hit Testing
 * ===================================================
 * Defines the Hittable interface and concrete primitives (Sphere, Triangle).
 *
 * Hit testing is where the most arithmetic happens per ray: quadratic
 * solvers for spheres, dot/cross products for triangles. The choice of
 * division and sqrt algorithms in MathEngine directly impacts the accuracy
 * of intersection points and surface normals, which in turn affects how
 * materials scatter secondary rays.
 */

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "ray.h"
#include <memory>
#include <vector>

// Forward declaration — materials are defined in material.h
class Material;

// ---------------------------------------------------------------------------
// HitRecord: stores intersection data passed from geometry to material
// ---------------------------------------------------------------------------
struct HitRecord {
    Point3 p;           // Intersection point
    Vec3   normal;      // Surface normal (always pointing against the ray)
    Scalar t;           // Ray parameter at intersection
    bool   front_face;  // True if the ray hit the outside surface

    std::shared_ptr<Material> mat;  // Material at the hit point

    /**
     * Ensure the normal always points against the incoming ray.
     * This convention simplifies material scattering logic: the normal
     * always faces "outward" relative to the ray direction.
     */
    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction, outward_normal) < Scalar(0.0f);
        normal = front_face ? outward_normal : -outward_normal;
    }
};

// ---------------------------------------------------------------------------
// Hittable: abstract interface for anything a ray can intersect
// ---------------------------------------------------------------------------
class Hittable {
public:
    virtual ~Hittable() = default;

    /**
     * Test if ray r hits this object for t in [t_min, t_max].
     * If so, fill out rec with the intersection details.
     * t_min is typically a small epsilon to avoid self-intersection ("shadow acne").
     * t_max is the closest hit so far (ensures we find the nearest intersection).
     */
    virtual bool hit(const Ray& r, Scalar t_min, Scalar t_max, HitRecord& rec) const = 0;
};

// ---------------------------------------------------------------------------
// Sphere: the classic ray tracing primitive
// ---------------------------------------------------------------------------
/**
 * Ray-sphere intersection uses the quadratic formula:
 *   Given ray P(t) = O + t*D and sphere center C, radius R:
 *   |P(t) - C|^2 = R^2
 *   => (D·D)t^2 + 2(D·(O-C))t + ((O-C)·(O-C) - R^2) = 0
 *
 * The discriminant b^2 - 4ac determines hit/miss. We use the "half-b"
 * optimization from RTiOW to reduce multiplications.
 */
class Sphere : public Hittable {
public:
    Point3 center;
    Scalar radius;
    std::shared_ptr<Material> mat;

    Sphere() {}
    Sphere(Point3 c, Scalar r, std::shared_ptr<Material> m)
        : center(c), radius(r), mat(m) {}

    bool hit(const Ray& r, Scalar t_min, Scalar t_max, HitRecord& rec) const override {
        Vec3 oc = r.origin - center;

        // Quadratic coefficients (half-b optimization)
        Scalar a = dot(r.direction, r.direction);
        Scalar half_b = dot(oc, r.direction);
        Scalar c = dot(oc, oc) - radius * radius;

        // Discriminant determines if ray intersects the sphere
        Scalar discriminant = half_b * half_b - a * c;
        if (discriminant < Scalar(0.0f))
            return false;

        Scalar sqrtd = s_sqrt(discriminant);

        // Find the nearest root in the acceptable range [t_min, t_max]
        Scalar root = (-half_b - sqrtd) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || root > t_max)
                return false;
        }

        // Fill the hit record
        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.mat = mat;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Triangle: defined by three vertices, uses Möller–Trumbore algorithm
// ---------------------------------------------------------------------------
/**
 * The Möller–Trumbore algorithm is the standard fast ray-triangle
 * intersection test. It computes barycentric coordinates (u, v) and the
 * ray parameter t simultaneously using a series of cross and dot products.
 *
 * Vertex layout: v0, v1, v2 in counter-clockwise order (right-hand rule
 * gives the outward normal direction).
 */
class Triangle : public Hittable {
public:
    Point3 v0, v1, v2;
    std::shared_ptr<Material> mat;

    Triangle() {}
    Triangle(Point3 v0, Point3 v1, Point3 v2, std::shared_ptr<Material> m)
        : v0(v0), v1(v1), v2(v2), mat(m) {}

    bool hit(const Ray& r, Scalar t_min, Scalar t_max, HitRecord& rec) const override {
        // Edge vectors
        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;

        // Begin calculating determinant — also used to calculate u parameter
        Vec3 h = cross(r.direction, edge2);
        Scalar det = dot(edge1, h);

        // Epsilon for parallel ray detection.
        // If det is near zero, the ray is parallel to the triangle plane.
        Scalar eps(1e-7f);
        if (s_abs(det) < eps)
            return false;

        Scalar inv_det = Scalar(1.0f) / det;
        Vec3 s = r.origin - v0;

        // Calculate u barycentric coordinate and test bounds
        Scalar u = inv_det * dot(s, h);
        if (u < Scalar(0.0f) || u > Scalar(1.0f))
            return false;

        // Calculate v barycentric coordinate and test bounds
        Vec3 q = cross(s, edge1);
        Scalar v = inv_det * dot(r.direction, q);
        if (v < Scalar(0.0f) || u + v > Scalar(1.0f))
            return false;

        // Calculate t to find where the intersection point is on the ray
        Scalar t = inv_det * dot(edge2, q);
        if (t < t_min || t > t_max)
            return false;

        // Valid hit — fill the record
        rec.t = t;
        rec.p = r.at(t);

        // Geometric normal from the cross product of edges
        Vec3 outward_normal = unit_vector(cross(edge1, edge2));
        rec.set_face_normal(r, outward_normal);
        rec.mat = mat;
        return true;
    }
};

// ---------------------------------------------------------------------------
// HittableList: holds all scene objects and tests them in sequence
// ---------------------------------------------------------------------------
/**
 * This is a simple linear scan through all objects. A production renderer
 * would use a BVH (bounding volume hierarchy) for O(log n) intersection,
 * but for studying arithmetic quality a linear scan keeps the code focused
 * on the math operations under test.
 */
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;

    void clear() { objects.clear(); }
    void add(std::shared_ptr<Hittable> obj) { objects.push_back(obj); }

    bool hit(const Ray& r, Scalar t_min, Scalar t_max, HitRecord& rec) const override {
        HitRecord temp_rec;
        bool hit_anything = false;
        Scalar closest_so_far = t_max;

        for (const auto& obj : objects) {
            if (obj->hit(r, t_min, closest_so_far, temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }
        return hit_anything;
    }
};

#endif // GEOMETRY_H
