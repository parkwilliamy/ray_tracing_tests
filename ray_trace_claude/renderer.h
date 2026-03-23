/**
 * renderer.h — Ray Tracing Renderer
 * ====================================
 * Contains the core rendering loop and the recursive ray_color() function.
 *
 * The rendering pipeline for each pixel:
 *   1. For each sample (controlled by samples_per_pixel):
 *      a. Generate a ray through the pixel with sub-pixel jitter
 *      b. Trace the ray through the scene (ray_color)
 *      c. Accumulate the returned color
 *   2. Divide by number of samples to average
 *   3. Apply gamma correction (sqrt for gamma 2.0)
 *   4. Write the pixel to the output buffer
 *
 * ray_color() is the recursive heart of the ray tracer:
 *   - If the ray hits an object, ask its material to scatter
 *   - If scattered, recursively trace the scattered ray (up to max_depth)
 *   - If the ray hits nothing, return the sky gradient (background)
 *   - At max_depth, return black (light is fully absorbed)
 *
 * Output format: PPM (Portable Pixmap) — the simplest image format.
 * Each pixel is written as three integers [0, 255] for R, G, B.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "geometry.h"
#include "material.h"
#include <fstream>
#include <iostream>
#include <string>
#include <cmath>

class Renderer {
public:
    int samples_per_pixel;  // Number of antialiasing samples per pixel
    int max_depth;          // Maximum ray bounce depth

    Renderer(int spp, int depth) : samples_per_pixel(spp), max_depth(depth) {}

    /**
     * Recursively determine the color seen along a ray.
     *
     * This function is the core of the path tracer. At each bounce:
     *   1. Test intersection with the scene
     *   2. If hit: ask the material to scatter, then recurse
     *   3. If miss: return background (sky gradient)
     *   4. If depth exceeded: return black (energy absorbed)
     *
     * The color returned is the product of all attenuation factors along
     * the path, multiplied by the sky color at the terminal ray.
     * Every operation in this function (dot, sqrt, division, RNG) uses
     * the configured MathEngine, making this the primary testbed for
     * arithmetic accuracy analysis.
     */
    Color ray_color(const Ray& r, const HittableList& world, int depth) const {
        // Base case: if we've exceeded the ray bounce limit, no more light
        // is gathered. This prevents infinite recursion (e.g., between
        // two parallel mirrors) and represents total energy absorption.
        if (depth <= 0)
            return Color(0.0f, 0.0f, 0.0f);

        HitRecord rec;

        // t_min = 0.001 (not 0) to avoid "shadow acne": numerical error
        // can cause the scattered ray to start slightly inside the surface,
        // creating self-intersection artifacts. The small offset avoids this.
        if (world.hit(r, Scalar(0.001f), Scalar(1e30f), rec)) {
            Ray scattered;
            Color attenuation;

            if (rec.mat->scatter(r, rec, attenuation, scattered)) {
                // Recursive step: trace the scattered ray and multiply
                // by the material's attenuation (color filter).
                Color child_color = ray_color(scattered, world, depth - 1);
                return attenuation * child_color;
            }
            // Material absorbed the ray (no scattering)
            return Color(0.0f, 0.0f, 0.0f);
        }

        // No hit: render the sky gradient.
        // Blend between white (bottom) and blue (top) based on ray Y direction.
        // This provides ambient illumination for the scene.
        Vec3 unit_dir = unit_vector(r.direction);
        Scalar t = Scalar(0.5f) * (unit_dir.y + Scalar(1.0f));  // Map y from [-1,1] to [0,1]
        Color white(1.0f, 1.0f, 1.0f);
        Color blue(0.5f, 0.7f, 1.0f);
        // Linear interpolation: (1-t)*white + t*blue
        return (Scalar(1.0f) - t) * white + t * blue;
    }

    /**
     * Render the entire scene to a PPM file.
     *
     * For each pixel, we cast samples_per_pixel rays with random sub-pixel
     * offsets to achieve antialiasing. The final color is the average of
     * all samples, gamma-corrected with gamma = 2.0 (i.e., sqrt).
     *
     * Progress is reported to stderr so it doesn't interfere with the
     * PPM output (which can go to stdout or a file).
     */
    void render(const Camera& cam, const HittableList& world, const std::string& filename) const {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: could not open " << filename << " for writing.\n";
            return;
        }

        // PPM header: P3 format (ASCII), width, height, max color value
        out << "P3\n" << cam.image_width << " " << cam.image_height << "\n255\n";

        // Render pixels top-to-bottom (PPM convention: first row is top)
        for (int j = cam.image_height - 1; j >= 0; j--) {
            // Progress indicator (every 10 rows or on last row)
            if (j % 10 == 0 || j == cam.image_height - 1) {
                std::cerr << "\rScanlines remaining: " << j << "   " << std::flush;
            }

            for (int i = 0; i < cam.image_width; i++) {
                Color pixel_color(0.0f, 0.0f, 0.0f);

                // Multi-sample antialiasing: jitter the ray within the pixel
                for (int s = 0; s < samples_per_pixel; s++) {
                    // u, v are the viewport coordinates with random sub-pixel offset
                    Scalar u = (Scalar(static_cast<float>(i)) + s_random())
                             / Scalar(static_cast<float>(cam.image_width - 1));
                    Scalar v = (Scalar(static_cast<float>(j)) + s_random())
                             / Scalar(static_cast<float>(cam.image_height - 1));

                    Ray r = cam.get_ray(u, v);
                    pixel_color += ray_color(r, world, max_depth);
                }

                // Average the samples and apply gamma correction
                write_color(out, pixel_color, samples_per_pixel);
            }
        }

        out.close();
        std::cerr << "\rDone.                           \n";
        std::cout << "Output written to: " << filename << "\n";
    }

private:
    /**
     * Write a single pixel color to the output stream.
     *
     * Steps:
     *   1. Divide by number of samples (averaging)
     *   2. Gamma correction: sqrt (gamma = 2.0) — this is the most common
     *      gamma value and approximates sRGB encoding. The sqrt uses the
     *      configured sqrt method from MathEngine.
     *   3. Clamp to [0, 1] and map to [0, 255] integer range
     */
    void write_color(std::ostream& out, Color pixel_color, int samples) const {
        Scalar scale = Scalar(1.0f) / Scalar(static_cast<float>(samples));
        Scalar r = pixel_color.x * scale;
        Scalar g = pixel_color.y * scale;
        Scalar b = pixel_color.z * scale;

        // Gamma correction (gamma = 2.0 → take sqrt)
        // This uses the configured sqrt method, adding another point
        // where arithmetic approximation affects the final image.
        r = s_sqrt(s_clamp(r, Scalar(0.0f), Scalar(0.999f)));
        g = s_sqrt(s_clamp(g, Scalar(0.0f), Scalar(0.999f)));
        b = s_sqrt(s_clamp(b, Scalar(0.0f), Scalar(0.999f)));

        // Map [0, 1) to [0, 255] and write as ASCII integers
        int ir = static_cast<int>(Scalar(256.0f) * r);
        int ig = static_cast<int>(Scalar(256.0f) * g);
        int ib = static_cast<int>(Scalar(256.0f) * b);

        ir = std::clamp(ir, 0, 255);
        ig = std::clamp(ig, 0, 255);
        ib = std::clamp(ib, 0, 255);

        out << ir << " " << ig << " " << ib << "\n";
    }
};

#endif // RENDERER_H
