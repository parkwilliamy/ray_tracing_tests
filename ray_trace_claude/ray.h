/**
 * ray.h — Ray Representation
 * ===========================
 * A ray is defined by P(t) = origin + t * direction, where t > 0 gives
 * points in front of the ray origin. This is the fundamental geometric
 * primitive traced through the scene for every pixel sample.
 */

#ifndef RAY_H
#define RAY_H

#include "vec3.h"

class Ray {
public:
    Point3 origin;
    Vec3   direction;

    Ray() {}
    Ray(const Point3& origin, const Vec3& direction)
        : origin(origin), direction(direction) {}

    /** Evaluate the ray equation: P(t) = origin + t * direction */
    Point3 at(Scalar t) const {
        return origin + t * direction;
    }
};

#endif // RAY_H
