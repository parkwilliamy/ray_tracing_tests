#ifndef STL_LOADER_H
#define STL_LOADER_H

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <cctype>

#include "hittable_list.h"
#include "triangle.h"  // triangle(Q, u, v, mat) — extends quad
#include "vec3.h"

// ============================================================================
// STL Loader for Ray Tracing in One Weekend
//
// Supports both ASCII and binary STL formats.
// Returns a hittable_list of triangles with the given material.
//
// Usage:
//     auto mesh = load_stl("model.stl", my_material);
//     world.add(make_shared<hittable_list>(mesh));
//
// Optional: use load_stl_centered() to auto-center and scale the model.
// ============================================================================

namespace stl {

// ----- Internal helpers -----------------------------------------------------

static inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

static bool is_binary_stl(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;

    // Read first 80 bytes (header) + 4 bytes (triangle count)
    char header[80];
    file.read(header, 80);
    if (!file) return false;

    uint32_t num_triangles = 0;
    file.read(reinterpret_cast<char*>(&num_triangles), 4);
    if (!file) return false;

    // Check if file size matches binary format expectation:
    //   80 (header) + 4 (count) + num_triangles * 50 bytes each
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    auto expected_size = 80 + 4 + static_cast<std::streamoff>(num_triangles) * 50;

    // If the sizes match, it's binary. Also guard against the header
    // starting with "solid" in a binary file (yes, this happens).
    return file_size == expected_size;
}

// ----- Coordinate conversion ------------------------------------------------
// Tinkercad (and most CAD/3D-printing tools) use Z-up.
// RTIOW uses Y-up.  Swap Y and Z so models appear upright.

static inline point3 z_up_to_y_up(double x, double y, double z) {
    return point3(x, z, -y);
}

// ----- ASCII STL parser -----------------------------------------------------

static hittable_list load_ascii_stl(const std::string& filepath,
                                    shared_ptr<material> mat)
{
    hittable_list triangles;
    std::ifstream file(filepath);

    if (!file) {
        std::cerr << "stl_loader: cannot open file: " << filepath << "\n";
        return triangles;
    }

    std::string line;
    int tri_count = 0;

    while (std::getline(file, line)) {
        std::string trimmed = to_lower(trim(line));

        if (trimmed.rfind("facet normal", 0) == 0) {
            // We ignore the stored normal and compute our own from vertices
            // (STL normals are often unreliable)

            // Skip "outer loop"
            std::getline(file, line);

            point3 verts[3];
            for (int i = 0; i < 3; i++) {
                std::getline(file, line);
                std::string vtx_line = trim(line);
                // Format: "vertex x y z"
                std::istringstream iss(vtx_line);
                std::string keyword;
                double x, y, z;
                iss >> keyword >> x >> y >> z;
                verts[i] = z_up_to_y_up(x, y, z);
            }

            // Skip "endloop" and "endfacet"
            std::getline(file, line);
            std::getline(file, line);

            // triangle(Q, u, v): Q = origin vertex, u/v = edge vectors
            triangles.add(make_shared<triangle>(
                verts[0], verts[1] - verts[0], verts[2] - verts[0], mat));
            tri_count++;
        }
    }

    std::clog << "stl_loader: loaded " << tri_count
              << " triangles (ASCII) from " << filepath << "\n";
    return triangles;
}

// ----- Binary STL parser ----------------------------------------------------

static hittable_list load_binary_stl(const std::string& filepath,
                                     shared_ptr<material> mat)
{
    hittable_list triangles;
    std::ifstream file(filepath, std::ios::binary);

    if (!file) {
        std::cerr << "stl_loader: cannot open file: " << filepath << "\n";
        return triangles;
    }

    // Skip 80-byte header
    file.seekg(80);

    uint32_t num_triangles = 0;
    file.read(reinterpret_cast<char*>(&num_triangles), 4);

    for (uint32_t i = 0; i < num_triangles; i++) {
        float data[12]; // normal(3) + v0(3) + v1(3) + v2(3)
        file.read(reinterpret_cast<char*>(data), 48);

        // Skip 2-byte attribute byte count
        uint16_t attr;
        file.read(reinterpret_cast<char*>(&attr), 2);

        // data[0..2] = normal (ignored, we compute our own)
        point3 v0 = z_up_to_y_up(data[3],  data[4],  data[5]);
        point3 v1 = z_up_to_y_up(data[6],  data[7],  data[8]);
        point3 v2 = z_up_to_y_up(data[9],  data[10], data[11]);

        // triangle(Q, u, v): Q = origin vertex, u/v = edge vectors
        triangles.add(make_shared<triangle>(v0, v1 - v0, v2 - v0, mat));
    }

    std::clog << "stl_loader: loaded " << num_triangles
              << " triangles (binary) from " << filepath << "\n";
    return triangles;
}

// ----- Public API -----------------------------------------------------------

// Load an STL file (auto-detects ASCII vs binary).
// All triangles share the given material.
static hittable_list load_stl(const std::string& filepath,
                               shared_ptr<material> mat)
{
    if (is_binary_stl(filepath))
        return load_binary_stl(filepath, mat);
    else
        return load_ascii_stl(filepath, mat);
}

// Mesh bounding box info for centering/scaling
struct mesh_bounds {
    point3 min_pt;
    point3 max_pt;
    point3 center;
    double max_extent;  // largest axis-aligned dimension
};

// Compute bounds by scanning vertices in an STL file.
// (Re-reads the file; call once before load if you need bounds.)
static mesh_bounds compute_bounds(const std::string& filepath) {
    // Quick and dirty: parse vertices without building triangles
    double minx = 1e30, miny = 1e30, minz = 1e30;
    double maxx = -1e30, maxy = -1e30, maxz = -1e30;

    auto update = [&](double x, double y, double z) {
        minx = std::fmin(minx, x); maxx = std::fmax(maxx, x);
        miny = std::fmin(miny, y); maxy = std::fmax(maxy, y);
        minz = std::fmin(minz, z); maxz = std::fmax(maxz, z);
    };

    if (is_binary_stl(filepath)) {
        std::ifstream file(filepath, std::ios::binary);
        file.seekg(80);
        uint32_t n; file.read(reinterpret_cast<char*>(&n), 4);
        for (uint32_t i = 0; i < n; i++) {
            float d[12]; file.read(reinterpret_cast<char*>(d), 48);
            uint16_t a; file.read(reinterpret_cast<char*>(&a), 2);
            auto p0 = z_up_to_y_up(d[3], d[4], d[5]);
            auto p1 = z_up_to_y_up(d[6], d[7], d[8]);
            auto p2 = z_up_to_y_up(d[9], d[10], d[11]);
            update(p0.x(), p0.y(), p0.z());
            update(p1.x(), p1.y(), p1.z());
            update(p2.x(), p2.y(), p2.z());
        }
    } else {
        std::ifstream file(filepath);
        std::string line;
        while (std::getline(file, line)) {
            std::string t = to_lower(trim(line));
            if (t.rfind("vertex", 0) == 0) {
                std::istringstream iss(t);
                std::string kw; double x, y, z;
                iss >> kw >> x >> y >> z;
                auto p = z_up_to_y_up(x, y, z);
                update(p.x(), p.y(), p.z());
            }
        }
    }

    mesh_bounds b;
    b.min_pt = point3(minx, miny, minz);
    b.max_pt = point3(maxx, maxy, maxz);
    b.center = point3((minx+maxx)/2, (miny+maxy)/2, (minz+maxz)/2);
    b.max_extent = std::fmax(std::fmax(maxx-minx, maxy-miny), maxz-minz);
    return b;
}

// Load an STL, then translate so it's centered at `target_center`
// and uniformly scaled so its largest dimension equals `target_size`.
// This is handy since STL files come in arbitrary coordinate spaces.
static hittable_list load_stl_centered(const std::string& filepath,
                                        shared_ptr<material> mat,
                                        point3 target_center = point3(0,0,0),
                                        double target_size = 2.0)
{
    mesh_bounds bounds = compute_bounds(filepath);
    double scale = target_size / bounds.max_extent;

    // Load raw, then rebuild with transformed vertices
    // (We re-parse rather than transforming after the fact to keep things simple)
    hittable_list result;

    auto transform = [&](point3 p) -> point3 {
        return point3(
            (p.x() - bounds.center.x()) * scale + target_center.x(),
            (p.y() - bounds.center.y()) * scale + target_center.y(),
            (p.z() - bounds.center.z()) * scale + target_center.z()
        );
    };

    if (is_binary_stl(filepath)) {
        std::ifstream file(filepath, std::ios::binary);
        file.seekg(80);
        uint32_t n; file.read(reinterpret_cast<char*>(&n), 4);
        for (uint32_t i = 0; i < n; i++) {
            float d[12]; file.read(reinterpret_cast<char*>(d), 48);
            uint16_t a; file.read(reinterpret_cast<char*>(&a), 2);
            auto v0 = transform(z_up_to_y_up(d[3], d[4], d[5]));
            auto v1 = transform(z_up_to_y_up(d[6], d[7], d[8]));
            auto v2 = transform(z_up_to_y_up(d[9], d[10], d[11]));
            result.add(make_shared<triangle>(v0, v1 - v0, v2 - v0, mat));
        }
    } else {
        std::ifstream file(filepath);
        std::string line;
        while (std::getline(file, line)) {
            std::string trimmed = to_lower(trim(line));
            if (trimmed.rfind("facet normal", 0) == 0) {
                std::getline(file, line); // outer loop
                point3 verts[3];
                for (int i = 0; i < 3; i++) {
                    std::getline(file, line);
                    std::istringstream iss(trim(line));
                    std::string kw; double x, y, z;
                    iss >> kw >> x >> y >> z;
                    verts[i] = transform(z_up_to_y_up(x, y, z));
                }
                std::getline(file, line); // endloop
                std::getline(file, line); // endfacet
                result.add(make_shared<triangle>(
                    verts[0], verts[1] - verts[0], verts[2] - verts[0], mat));
            }
        }
    }

    std::clog << "stl_loader: loaded and centered mesh from " << filepath << "\n";
    return result;
}

} // namespace stl

#endif