#ifndef CAMERA_H
#define CAMERA_H

#include "hittable.h"
#include "material.h"

class camera {
  public:
    double aspect_ratio      = 1.0;
    int    image_width       = 100;
    int    samples_per_pixel = 10;
    int    max_depth         = 10;

    double vfov     = 90;
    double lookfrom_d[3] = {0, 0, 0};
    double lookat_d[3]   = {0, 0, -1};
    double vup_d[3]      = {0, 1, 0};

    void set_lookfrom(double x, double y, double z) { lookfrom_d[0]=x; lookfrom_d[1]=y; lookfrom_d[2]=z; }
    void set_lookat(double x, double y, double z)   { lookat_d[0]=x;   lookat_d[1]=y;   lookat_d[2]=z; }
    void set_vup(double x, double y, double z)      { vup_d[0]=x;      vup_d[1]=y;      vup_d[2]=z; }

    void render(const hittable& world) {
        initialize();

        std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++) {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
            for (int i = 0; i < image_width; i++) {
                // Accumulate in 16-bit to avoid overflow, then average.
                // In hardware this would be a wider accumulator register.
                int16_t acc_r = 0, acc_g = 0, acc_b = 0;
                for (int sample = 0; sample < samples_per_pixel; sample++) {
                    ray r = get_ray(i, j);
                    color c = ray_color(r, max_depth, world);
                    acc_r += c.x().raw;
                    acc_g += c.y().raw;
                    acc_b += c.z().raw;
                }
                // Divide by sample count in 16-bit, then truncate back to fixed8
                color pixel_color(
                    fixed8::from_raw(static_cast<int8_t>((acc_r / samples_per_pixel) & 0xFF)),
                    fixed8::from_raw(static_cast<int8_t>((acc_g / samples_per_pixel) & 0xFF)),
                    fixed8::from_raw(static_cast<int8_t>((acc_b / samples_per_pixel) & 0xFF))
                );
                write_color(std::cout, pixel_color);
            }
        }

        std::clog << "\rDone.                 \n";
    }

  private:
    int    image_height;
    fixed8 pixel_samples_scale;

    // Camera setup stored in double — ray generation is the "host side"
    // that feeds normalized direction vectors into the fixed8 pipeline.
    double d_center[3];
    double d_pixel00[3];
    double d_delta_u[3];
    double d_delta_v[3];

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = fixed8(1.0 / samples_per_pixel);

        double cx = lookfrom_d[0], cy = lookfrom_d[1], cz = lookfrom_d[2];
        double lx = lookat_d[0],  ly = lookat_d[1],  lz = lookat_d[2];
        double ux = vup_d[0],     uy = vup_d[1],     uz = vup_d[2];

        d_center[0] = cx; d_center[1] = cy; d_center[2] = cz;

        double dx = cx-lx, dy = cy-ly, dz = cz-lz;
        double focal_length = std::sqrt(dx*dx + dy*dy + dz*dz);
        double theta = degrees_to_radians(vfov);
        double h = std::tan(theta / 2);
        double viewport_height = 2.0 * h * focal_length;
        double viewport_width = viewport_height * (double(image_width) / image_height);

        double wlen = std::sqrt(dx*dx + dy*dy + dz*dz);
        double wx = dx/wlen, wy = dy/wlen, wz = dz/wlen;

        double ucx = uy*wz - uz*wy, ucy = uz*wx - ux*wz, ucz = ux*wy - uy*wx;
        double ulen = std::sqrt(ucx*ucx + ucy*ucy + ucz*ucz);
        double uux = ucx/ulen, uuy = ucy/ulen, uuz = ucz/ulen;

        double vvx = wy*uuz - wz*uuy, vvy = wz*uux - wx*uuz, vvz = wx*uuy - wy*uux;

        double vp_ux = viewport_width * uux, vp_uy = viewport_width * uuy, vp_uz = viewport_width * uuz;
        double vp_vx = viewport_height * -vvx, vp_vy = viewport_height * -vvy, vp_vz = viewport_height * -vvz;

        d_delta_u[0] = vp_ux/image_width;  d_delta_u[1] = vp_uy/image_width;  d_delta_u[2] = vp_uz/image_width;
        d_delta_v[0] = vp_vx/image_height; d_delta_v[1] = vp_vy/image_height; d_delta_v[2] = vp_vz/image_height;

        double ul_x = cx - focal_length*wx - vp_ux/2 - vp_vx/2;
        double ul_y = cy - focal_length*wy - vp_uy/2 - vp_vy/2;
        double ul_z = cz - focal_length*wz - vp_uz/2 - vp_vz/2;

        d_pixel00[0] = ul_x + 0.5*(d_delta_u[0] + d_delta_v[0]);
        d_pixel00[1] = ul_y + 0.5*(d_delta_u[1] + d_delta_v[1]);
        d_pixel00[2] = ul_z + 0.5*(d_delta_u[2] + d_delta_v[2]);
    }

    // Ray generation: compute direction in double, normalize, then convert to fixed8.
    // This models a hardware design where the host precomputes or a dedicated unit
    // generates ray directions at higher precision, feeding the fixed8 intersection pipeline.
    ray get_ray(int i, int j) const {
        double ox = random_double() - 0.5;
        double oy = random_double() - 0.5;

        double px = d_pixel00[0] + (i + ox)*d_delta_u[0] + (j + oy)*d_delta_v[0];
        double py = d_pixel00[1] + (i + ox)*d_delta_u[1] + (j + oy)*d_delta_v[1];
        double pz = d_pixel00[2] + (i + ox)*d_delta_u[2] + (j + oy)*d_delta_v[2];

        double rdx = px - d_center[0];
        double rdy = py - d_center[1];
        double rdz = pz - d_center[2];

        // Normalize direction in double, then quantize to fixed8
        double rlen = std::sqrt(rdx*rdx + rdy*rdy + rdz*rdz);
        rdx /= rlen; rdy /= rlen; rdz /= rlen;

        // Origin and direction converted to fixed8 — from here, all math is 8-bit
        point3 ray_origin(d_center[0], d_center[1], d_center[2]);
        vec3 ray_direction(rdx, rdy, rdz);

        return ray(ray_origin, ray_direction);
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        if (depth <= 0)
            return color(0, 0, 0);

        hit_record rec;

        if (world.hit(r, interval(0.0625, 7.9375), rec)) {
            ray scattered;
            color attenuation;
            if (rec.mat->scatter(r, rec, attenuation, scattered))
                return attenuation * ray_color(scattered, depth-1, world);
            return color(0, 0, 0);
        }

        // Sky gradient
        vec3 unit_direction = unit_vector(r.direction());
        auto a = fixed8(0.5) * (unit_direction.y() + fixed8(1.0));
        auto one_minus_a = fixed8(1.0) - a;
        return one_minus_a * color(1.0, 1.0, 1.0) + a * color(0.5, 0.6875, 1.0);
    }
};

#endif