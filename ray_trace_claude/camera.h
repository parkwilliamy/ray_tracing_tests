/**
 * camera.h — Fixed-Position Camera
 * ==================================
 * Generates rays through pixel locations on a virtual viewport. The camera
 * has a fixed position, orientation, and field of view as specified by the
 * project requirements.
 *
 * Camera parameters (hardcoded per spec):
 *   - Position: (0, 0, 0) — at the origin
 *   - Look direction: (0, 0, -1) — looking down -Z
 *   - Up vector: (0, 1, 0) — Y is up
 *   - FOV: 90 degrees vertical
 *   - Aspect ratio: configurable (default 16:9)
 *
 * The viewport is placed at z = -focal_length in front of the camera.
 * Rays are cast from the camera origin through each pixel's position on
 * the viewport, with sub-pixel jitter for antialiasing (controlled by
 * the samples-per-pixel setting).
 */

#ifndef CAMERA_H
#define CAMERA_H

#include "ray.h"

class Camera {
public:
    Point3 origin;          // Camera position (fixed at origin)
    Vec3   horizontal;      // Viewport width vector
    Vec3   vertical;        // Viewport height vector
    Point3 lower_left;      // Bottom-left corner of viewport

    int image_width;
    int image_height;

    /**
     * Construct camera with the given image dimensions.
     *
     * The virtual viewport dimensions are derived from the 90° FOV and
     * the aspect ratio. A 90° vertical FOV means the viewport half-height
     * equals the focal length (tan(45°) = 1), so the viewport height = 2
     * at focal_length = 1.
     */
    Camera(int width, int height)
        : image_width(width), image_height(height)
    {
        // Fixed parameters
        Scalar aspect_ratio = Scalar(static_cast<float>(width)) / Scalar(static_cast<float>(height));
        Scalar focal_length(1.0f);

        // 90° vertical FOV: viewport_height = 2 * tan(45°) * focal_length = 2.0
        Scalar viewport_height(2.0f);
        Scalar viewport_width = aspect_ratio * viewport_height;

        origin     = Point3(0.0f, 0.0f, 0.0f);
        horizontal = Vec3(viewport_width, Scalar(0.0f), Scalar(0.0f));
        vertical   = Vec3(Scalar(0.0f), viewport_height, Scalar(0.0f));

        // Lower-left corner of the viewport in world space:
        // origin - horizontal/2 - vertical/2 - (0, 0, focal_length)
        lower_left = origin
            - horizontal / Scalar(2.0f)
            - vertical / Scalar(2.0f)
            - Vec3(Scalar(0.0f), Scalar(0.0f), focal_length);
    }

    /**
     * Generate a ray through normalized viewport coordinates (u, v).
     *   u ∈ [0, 1]: horizontal position (0 = left edge, 1 = right edge)
     *   v ∈ [0, 1]: vertical position  (0 = bottom edge, 1 = top edge)
     *
     * The caller adds sub-pixel jitter to (u, v) for antialiasing.
     * All the vector math here goes through the configured MathEngine.
     */
    Ray get_ray(Scalar u, Scalar v) const {
        Vec3 direction = lower_left + u * horizontal + v * vertical - origin;
        return Ray(origin, direction);
    }
};

#endif // CAMERA_H
