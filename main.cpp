// ============================================================================
// Example: Loading an STL mesh into your Ray Tracing in One Weekend scene
// ============================================================================
//
// Drop triangle.h and stl_loader.h alongside your existing RTIOW headers,
// then use them like any other hittable.
//
// This file shows a minimal scene setup — adapt to your own main.cpp.
// ============================================================================

#include "rtweekend.h"       // your common header (infinity, pi, random_double, etc.)
#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"

#include "triangle.h"        // <-- new
#include "stl_loader.h"      // <-- new

int main() {
    hittable_list world;

    // --- Ground plane (unchanged from typical RTIOW scene) ---
    auto ground_material = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

    // --- Load STL mesh ---
    auto mesh_material = make_shared<lambertian>(color(0.8, 0.2, 0.2));

    // Option A: Load raw (keeps original STL coordinates)
    //   auto mesh = stl::load_stl("model.stl", mesh_material);

    // Option B: Load centered and scaled (recommended)
    //   Centers the mesh at the given point, scales so largest dimension = target_size
    auto mesh = stl::load_stl_centered(
        "matt.stl",
        mesh_material,
        point3(0, 1, 0),   // center the mesh here
        2.0                 // scale so it's 2 units across
    );

    // Add all triangles to the world.
    // For performance with large meshes, wrap in a BVH:
    //   world.add(make_shared<bvh_node>(mesh));
    // For small meshes, adding directly is fine:
    //world.add(make_shared<hittable_list>(mesh));

    // --- A couple of reference spheres ---
    auto glass = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(6, 1, 0), 1.0, glass));

    //auto metal_mat = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    //world.add(make_shared<sphere>(point3(2, 1, 0), 1.0, metal_mat));

    // --- Camera (standard RTIOW setup) ---
    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 8;
    cam.max_depth         = 10;

    cam.vfov     = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat   = point3(0, 1, 0);
    cam.vup      = vec3(0, 1, 0);

    cam.render(world);
}