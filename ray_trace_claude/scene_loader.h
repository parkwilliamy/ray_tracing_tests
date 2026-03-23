/**
 * scene_loader.h — Scene File Parser
 * ====================================
 * Parses a simple text-based scene description file to populate a
 * HittableList with spheres, triangles, and their materials.
 *
 * File format (lines starting with '#' are comments):
 *
 *   MATERIALS — define named materials before using them on objects:
 *     material lambertian <name> <r> <g> <b>
 *     material metal <name> <r> <g> <b> <fuzz>
 *     material dielectric <name> <ior>
 *
 *   OBJECTS — reference materials by name:
 *     sphere <cx> <cy> <cz> <radius> <material_name>
 *     triangle <x0> <y0> <z0> <x1> <y1> <z1> <x2> <y2> <z2> <material_name>
 *
 * Example scene file:
 *   material lambertian ground 0.8 0.8 0.0
 *   material lambertian center 0.1 0.2 0.5
 *   material dielectric glass 1.5
 *   material metal mirror 0.8 0.6 0.2 0.0
 *
 *   sphere 0.0 -100.5 -1.0 100.0 ground
 *   sphere 0.0 0.0 -1.0 0.5 center
 *   sphere -1.0 0.0 -1.0 0.5 glass
 *   sphere 1.0 0.0 -1.0 0.5 mirror
 */

#ifndef SCENE_LOADER_H
#define SCENE_LOADER_H

#include "geometry.h"
#include "material.h"
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <memory>
#include <iostream>

class SceneLoader {
public:
    /**
     * Load a scene from file. Returns true on success.
     * Populates the provided HittableList with all parsed objects.
     */
    static bool load(const std::string& filename, HittableList& world) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: could not open scene file: " << filename << "\n";
            return false;
        }

        std::map<std::string, std::shared_ptr<Material>> materials;
        std::string line;
        int line_num = 0;

        while (std::getline(file, line)) {
            line_num++;

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Trim leading whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            std::istringstream iss(line);
            std::string keyword;
            iss >> keyword;

            if (keyword == "material") {
                parse_material(iss, materials, line_num);
            } else if (keyword == "sphere") {
                parse_sphere(iss, materials, world, line_num);
            } else if (keyword == "triangle") {
                parse_triangle(iss, materials, world, line_num);
            } else {
                std::cerr << "Warning (line " << line_num
                          << "): unknown keyword '" << keyword << "'\n";
            }
        }

        std::cout << "Scene loaded: " << world.objects.size() << " objects, "
                  << materials.size() << " materials.\n";
        return true;
    }

    /**
     * Create a default scene (RTiOW-style) when no scene file is provided.
     * Contains a ground plane, three spheres (diffuse, glass, metal),
     * and a small triangle to demonstrate triangle rendering.
     */
    static void load_default(HittableList& world) {
        // Materials
        auto mat_ground = std::make_shared<Lambertian>(Color(0.8f, 0.8f, 0.0f));
        auto mat_center = std::make_shared<Lambertian>(Color(0.1f, 0.2f, 0.5f));
        auto mat_left   = std::make_shared<Dielectric>(Scalar(1.5f));
        auto mat_right  = std::make_shared<Metal>(Color(0.8f, 0.6f, 0.2f), Scalar(0.0f));
        auto mat_tri    = std::make_shared<Lambertian>(Color(0.9f, 0.2f, 0.2f));

        // Ground: large sphere at y = -100.5 (surface at y = 0)
        world.add(std::make_shared<Sphere>(
            Point3(0.0f, -100.5f, -1.0f), Scalar(100.0f), mat_ground));

        // Center sphere: matte blue
        world.add(std::make_shared<Sphere>(
            Point3(0.0f, 0.0f, -1.0f), Scalar(0.5f), mat_center));

        // Left sphere: glass
        world.add(std::make_shared<Sphere>(
            Point3(-1.0f, 0.0f, -1.0f), Scalar(0.5f), mat_left));

        // Inner left sphere: negative radius creates a hollow glass bubble
        world.add(std::make_shared<Sphere>(
            Point3(-1.0f, 0.0f, -1.0f), Scalar(-0.4f), mat_left));

        // Right sphere: polished gold metal
        world.add(std::make_shared<Sphere>(
            Point3(1.0f, 0.0f, -1.0f), Scalar(0.5f), mat_right));

        // A small red triangle above and behind center sphere
        world.add(std::make_shared<Triangle>(
            Point3(-0.3f, 0.6f, -0.8f),
            Point3(0.3f, 0.6f, -0.8f),
            Point3(0.0f, 1.1f, -1.0f),
            mat_tri));

        std::cout << "Default scene loaded: " << world.objects.size() << " objects.\n";
    }

private:
    static void parse_material(
        std::istringstream& iss,
        std::map<std::string, std::shared_ptr<Material>>& materials,
        int line_num)
    {
        std::string type, name;
        iss >> type >> name;

        if (type == "lambertian") {
            float r, g, b;
            if (!(iss >> r >> g >> b)) {
                std::cerr << "Error (line " << line_num << "): lambertian needs r g b\n";
                return;
            }
            materials[name] = std::make_shared<Lambertian>(Color(r, g, b));

        } else if (type == "metal") {
            float r, g, b, fuzz;
            if (!(iss >> r >> g >> b >> fuzz)) {
                std::cerr << "Error (line " << line_num << "): metal needs r g b fuzz\n";
                return;
            }
            materials[name] = std::make_shared<Metal>(Color(r, g, b), Scalar(fuzz));

        } else if (type == "dielectric") {
            float ior;
            if (!(iss >> ior)) {
                std::cerr << "Error (line " << line_num << "): dielectric needs ior\n";
                return;
            }
            materials[name] = std::make_shared<Dielectric>(Scalar(ior));

        } else {
            std::cerr << "Error (line " << line_num
                      << "): unknown material type '" << type << "'\n";
        }
    }

    static void parse_sphere(
        std::istringstream& iss,
        const std::map<std::string, std::shared_ptr<Material>>& materials,
        HittableList& world,
        int line_num)
    {
        float cx, cy, cz, radius;
        std::string mat_name;
        if (!(iss >> cx >> cy >> cz >> radius >> mat_name)) {
            std::cerr << "Error (line " << line_num
                      << "): sphere needs cx cy cz radius material_name\n";
            return;
        }
        auto it = materials.find(mat_name);
        if (it == materials.end()) {
            std::cerr << "Error (line " << line_num
                      << "): unknown material '" << mat_name << "'\n";
            return;
        }
        world.add(std::make_shared<Sphere>(
            Point3(cx, cy, cz), Scalar(radius), it->second));
    }

    static void parse_triangle(
        std::istringstream& iss,
        const std::map<std::string, std::shared_ptr<Material>>& materials,
        HittableList& world,
        int line_num)
    {
        float x0, y0, z0, x1, y1, z1, x2, y2, z2;
        std::string mat_name;
        if (!(iss >> x0 >> y0 >> z0 >> x1 >> y1 >> z1 >> x2 >> y2 >> z2 >> mat_name)) {
            std::cerr << "Error (line " << line_num
                      << "): triangle needs x0 y0 z0 x1 y1 z1 x2 y2 z2 material_name\n";
            return;
        }
        auto it = materials.find(mat_name);
        if (it == materials.end()) {
            std::cerr << "Error (line " << line_num
                      << "): unknown material '" << mat_name << "'\n";
            return;
        }
        world.add(std::make_shared<Triangle>(
            Point3(x0, y0, z0), Point3(x1, y1, z1), Point3(x2, y2, z2),
            it->second));
    }
};

#endif // SCENE_LOADER_H
