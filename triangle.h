#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "quad.h"

// A triangle defined by origin point Q and two edge vectors u and v.
// Identical to quad in all respects except the interior bounds check:
// instead of 0<=alpha<=1 and 0<=beta<=1 (a parallelogram),
// we require alpha>=0, beta>=0, and alpha+beta<=1 (the lower triangle).
class triangle : public quad {
  public:
    triangle(const point3& Q, const vec3& u, const vec3& v, shared_ptr<material> mat)
      : quad(Q, u, v, mat) {}

    bool is_interior(double a, double b, hit_record& rec) const override {
        // Hit point must be inside the triangle:
        // both barycentric coordinates non-negative, and their sum <= 1.
        if (a < 0 || b < 0 || (a + b) > 1)
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }
};

#endif