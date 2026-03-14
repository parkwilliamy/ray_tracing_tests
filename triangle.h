#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "quad.h"

class triangle : public quad {
  public:
    triangle(const point3& Q, const vec3& u, const vec3& v, shared_ptr<material> mat)
      : quad(Q, u, v, mat) {}

    bool is_interior(fixed8 a, fixed8 b, hit_record& rec) const override {
        if (a < fixed8(0.0) || b < fixed8(0.0) || (a + b) > fixed8(1.0))
            return false;
        rec.u = a;
        rec.v = b;
        return true;
    }
};

#endif