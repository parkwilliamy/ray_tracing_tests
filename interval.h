#ifndef INTERVAL_H
#define INTERVAL_H

class interval {
  public:
    fixed8 min, max;

    interval() : min(fp_infinity), max(-fp_infinity) {}
    interval(fixed8 mn, fixed8 mx) : min(mn), max(mx) {}
    interval(double mn, double mx) : min(fixed8(mn)), max(fixed8(mx)) {}

    fixed8 size() const { return max - min; }

    bool contains(fixed8 x) const { return min <= x && x <= max; }
    bool surrounds(fixed8 x) const { return min < x && x < max; }

    fixed8 clamp(fixed8 x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    static const interval empty, universe;
};

const interval interval::empty    = interval(fp_infinity, -fp_infinity);
const interval interval::universe = interval(-fp_infinity, fp_infinity);

#endif