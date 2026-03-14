// ============================================================================
// 8-bit fixed-point (Q3.4) ray tracer — exploring precision degradation
//
// Scene is scaled to fit within Q3.4 range [-8, +7.9375].
// Camera setup computed in double, per-pixel math in fixed8.
// ============================================================================

#include "rtweekend.h"
#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"

int main() {
    hittable_list world;

    // Ground — large sphere below the scene
    // Radius 5.0 fits in Q3.4, center at y=-5 keeps the surface near y=0
    /*
    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -5, 0), 5.0, ground_material));

    // A few small spheres within the representable range
    auto red_mat   = make_shared<lambertian>(color(0.8125, 0.125, 0.125));
    auto green_mat = make_shared<lambertian>(color(0.125, 0.8125, 0.125));
    auto glass     = make_shared<dielectric>(1.5);
    auto metal_mat = make_shared<metal>(color(0.6875, 0.625, 0.5), 0.0);

    world.add(make_shared<sphere>(point3(-1.5, 0.5, -2.0), 0.5, red_mat));
    world.add(make_shared<sphere>(point3( 0.0, 0.5, -2.0), 0.5, glass));
    world.add(make_shared<sphere>(point3( 1.5, 0.5, -2.0), 0.5, metal_mat));
    world.add(make_shared<sphere>(point3( 0.0, 1.5, -2.0), 0.5, green_mat));
*/
    // Camera — setup in double, pixel pipeline in fixed8
    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 8;
    cam.max_depth         = 5;

    cam.vfov = 60;

    cam.render(world);
}