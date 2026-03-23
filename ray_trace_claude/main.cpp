/**
 * main.cpp — Interactive Ray Tracer Configuration and Entry Point
 * =================================================================
 * This program drives the configurable ray tracer. On startup, it
 * interactively asks the user to select:
 *
 *   1. Render quality: samples per pixel, max ray bounces
 *   2. Number encoding format: fp8, fp16, fp32, Q8n.m, Q16n.m
 *   3. Arithmetic implementations for every operation category:
 *      addition, subtraction, multiplication, division, sqrt,
 *      trigonometry, log/exp, and random number generation.
 *   4. Scene: default built-in scene or a custom .scene file
 *   5. Image dimensions and output filename
 *
 * After configuration, the program prints a summary of all settings
 * and renders the scene, writing a PPM image file.
 *
 * Usage:
 *   ./raytracer                    (interactive mode)
 *   ./raytracer --defaults         (use all default settings for quick test)
 *   ./raytracer --scene file.scene (specify scene file, rest interactive)
 */

#include "number_format.h"
#include "math_engine.h"
#include "scalar.h"
#include "vec3.h"
#include "ray.h"
#include "geometry.h"
#include "material.h"
#include "camera.h"
#include "renderer.h"
#include "scene_loader.h"

#include <iostream>
#include <string>
#include <chrono>
#include <limits>
#include <cstring>

// ---------------------------------------------------------------------------
// Helper: read an integer from stdin with a prompt and default value
// ---------------------------------------------------------------------------
int read_int(const std::string& prompt, int default_val) {
    std::cout << prompt << " [default: " << default_val << "]: ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty()) return default_val;
    try { return std::stoi(input); }
    catch (...) {
        std::cout << "  Invalid input, using default: " << default_val << "\n";
        return default_val;
    }
}

// ---------------------------------------------------------------------------
// Helper: read a string from stdin with a prompt and default value
// ---------------------------------------------------------------------------
std::string read_string(const std::string& prompt, const std::string& default_val) {
    std::cout << prompt << " [default: " << default_val << "]: ";
    std::string input;
    std::getline(std::cin, input);
    return input.empty() ? default_val : input;
}

// ---------------------------------------------------------------------------
// Helper: present numbered options and read user's choice
// ---------------------------------------------------------------------------
template<typename EnumT>
EnumT choose_option(const std::string& category,
                    const std::vector<std::pair<std::string, EnumT>>& options,
                    int default_idx = 0)
{
    std::cout << "\n--- " << category << " ---\n";
    for (size_t i = 0; i < options.size(); i++) {
        std::cout << "  " << (i + 1) << ") " << options[i].first;
        if (static_cast<int>(i) == default_idx) std::cout << "  (default)";
        std::cout << "\n";
    }
    std::cout << "Choose [1-" << options.size() << "]: ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty()) return options[default_idx].second;
    try {
        int choice = std::stoi(input);
        if (choice >= 1 && choice <= static_cast<int>(options.size()))
            return options[choice - 1].second;
    } catch (...) {}
    std::cout << "  Invalid choice, using default.\n";
    return options[default_idx].second;
}

// ---------------------------------------------------------------------------
// Configure all math engine operations interactively
// ---------------------------------------------------------------------------
void configure_math_engine() {
    auto& me = MathEngine::instance();

    std::cout << "\n========================================\n"
              << " ARITHMETIC OPERATION CONFIGURATION\n"
              << "========================================\n"
              << "For each operation, choose the implementation algorithm.\n"
              << "Different algorithms introduce characteristic approximation\n"
              << "errors that affect final render quality.\n";

    // --- Addition ---
    me.add_method = choose_option<AddMethod>("Addition", {
        {"Standard (IEEE 754 float add)", AddMethod::STANDARD},
        {"Saturating (clamp on overflow)", AddMethod::SATURATING}
    }, 0);

    // --- Subtraction ---
    me.sub_method = choose_option<SubMethod>("Subtraction", {
        {"Standard (IEEE 754 float sub)", SubMethod::STANDARD},
        {"Saturating (clamp on overflow)", SubMethod::SATURATING}
    }, 0);

    // --- Multiplication ---
    me.mul_method = choose_option<MulMethod>("Multiplication", {
        {"Standard (IEEE 754 float mul)", MulMethod::STANDARD},
        {"Shift-and-Add (serial multiplier, 10-bit mantissa)", MulMethod::SHIFT_ADD},
        {"Log-Domain (LNS: exp(log(a) + log(b)))", MulMethod::LOG_DOMAIN}
    }, 0);

    // --- Division ---
    me.div_method = choose_option<DivMethod>("Division", {
        {"Standard (IEEE 754 float div)", DivMethod::STANDARD},
        {"Newton-Raphson (iterative reciprocal, 4 iterations)", DivMethod::NEWTON_RAPHSON},
        {"Goldschmidt (convergence division, 5 iterations)", DivMethod::GOLDSCHMIDT},
        {"SRT (radix-2 digit-recurrence, 24 bits)", DivMethod::SRT},
        {"LUT (256-entry ROM + linear interpolation)", DivMethod::LUT}
    }, 0);

    // --- Square Root ---
    me.sqrt_method = choose_option<SqrtMethod>("Square Root", {
        {"Standard (libm sqrt)", SqrtMethod::STANDARD},
        {"Newton/Heron (iterative, 6 iterations)", SqrtMethod::NEWTON},
        {"Fast Inverse Sqrt (Quake III magic constant)", SqrtMethod::FAST_INV_SQRT},
        {"CORDIC (hyperbolic vectoring mode, 20 iter)", SqrtMethod::CORDIC},
        {"LUT (256-entry ROM + linear interpolation)", SqrtMethod::LUT}
    }, 0);

    // --- Trigonometry ---
    me.trig_method = choose_option<TrigMethod>("Trigonometry (sin, cos, atan2)", {
        {"Standard (libm sin/cos/atan2)", TrigMethod::STANDARD},
        {"Taylor series (6 terms for sin, polynomial for atan)", TrigMethod::TAYLOR},
        {"CORDIC (rotational mode, 20 iterations)", TrigMethod::CORDIC},
        {"LUT (1024-entry sin table + symmetry)", TrigMethod::LUT},
        {"Chebyshev polynomial (uniform error distribution)", TrigMethod::CHEBYSHEV}
    }, 0);

    // --- Log / Exp ---
    me.logexp_method = choose_option<LogExpMethod>("Logarithm / Exponential", {
        {"Standard (libm log/exp)", LogExpMethod::STANDARD},
        {"Taylor series (range-reduced, 7 terms)", LogExpMethod::TAYLOR},
        {"LUT (256-entry ROM + linear interpolation)", LogExpMethod::LUT}
    }, 0);

    // --- RNG ---
    me.rng_method = choose_option<RngMethod>("Random Number Generation", {
        {"LFSR (32-bit Galois, fast but low quality)", RngMethod::LFSR},
        {"LCG (Linear Congruential, Numerical Recipes)", RngMethod::LCG},
        {"Xorshift32 (Marsaglia, good balance)", RngMethod::XORSHIFT},
        {"Mersenne Twister (MT19937, gold standard)", RngMethod::MERSENNE_TWISTER}
    }, 3);  // Default: Mersenne Twister
}

// ---------------------------------------------------------------------------
// Print a full configuration summary
// ---------------------------------------------------------------------------
void print_summary(int width, int height, int spp, int depth,
                   const std::string& scene_file, const std::string& output_file)
{
    const auto& nf = NumberFormatConfig::instance();
    const auto& me = MathEngine::instance();

    std::cout << "\n========================================\n"
              << " RENDER CONFIGURATION SUMMARY\n"
              << "========================================\n"
              << "Image: " << width << " x " << height << "\n"
              << "Samples per pixel: " << spp << "\n"
              << "Max ray bounces: " << depth << "\n"
              << "Number format: " << nf.to_string() << "\n"
              << "Scene: " << scene_file << "\n"
              << "Output: " << output_file << "\n"
              << "\nArithmetic methods:\n";
    me.print_config();
    std::cout << "========================================\n\n";
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::cout << "===========================================\n"
              << " Configurable Ray Tracer v1.0\n"
              << " Arithmetic-Accuracy Analysis Tool\n"
              << "===========================================\n";

    // --- Check for command-line flags ---
    bool use_defaults = false;
    std::string cli_scene = "";

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--defaults") == 0) {
            use_defaults = true;
        } else if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            cli_scene = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "\nUsage:\n"
                      << "  ./raytracer                      Interactive mode\n"
                      << "  ./raytracer --defaults            Use all defaults (quick test)\n"
                      << "  ./raytracer --scene file.scene    Specify scene file\n"
                      << "  ./raytracer --help                Show this help\n\n";
            return 0;
        }
    }

    // --- Default values ---
    int image_width  = 400;
    int image_height = 225;
    int spp          = 50;
    int max_depth    = 10;
    std::string number_format = "fp32";
    std::string scene_file = cli_scene.empty() ? "default" : cli_scene;
    std::string output_file = "output.ppm";

    if (!use_defaults) {
        // --- Interactive configuration ---

        std::cout << "\n--- IMAGE SETTINGS ---\n";
        image_width  = read_int("Image width", image_width);
        image_height = read_int("Image height", image_height);

        std::cout << "\n--- RENDER QUALITY ---\n";
        spp       = read_int("Samples per pixel (higher = less noise, slower)", spp);
        max_depth = read_int("Max ray bounces (higher = more realistic, slower)", max_depth);

        std::cout << "\n--- NUMBER FORMAT ---\n"
                  << "Available formats:\n"
                  << "  fp32          — 32-bit IEEE 754 (standard float)\n"
                  << "  fp16          — 16-bit IEEE 754 half precision\n"
                  << "  fp8           — 8-bit E4M3 minifloat\n"
                  << "  Q8n.m         — 8-bit signed fixed point (n+m=8)\n"
                  << "                  e.g. Q84.4 = 4 int bits, 4 frac bits\n"
                  << "  Q16n.m        — 16-bit signed fixed point (n+m=16)\n"
                  << "                  e.g. Q168.8 = 8 int bits, 8 frac bits\n";
        number_format = read_string("Number format", number_format);

        // --- Configure number format ---
        try {
            NumberFormatConfig::instance().parse(number_format);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing number format: " << e.what()
                      << "\nFalling back to fp32.\n";
            NumberFormatConfig::instance().type = NumberFormatType::FP32;
        }

        // --- Configure math operations ---
        configure_math_engine();

        // --- Scene selection ---
        std::cout << "\n--- SCENE ---\n";
        if (cli_scene.empty()) {
            scene_file = read_string("Scene file (or 'default' for built-in scene)", "default");
        }
        output_file = read_string("Output filename", output_file);

    } else {
        // All defaults: fp32, standard math, default scene
        std::cout << "\nUsing all default settings.\n";
        NumberFormatConfig::instance().type = NumberFormatType::FP32;
    }

    // --- Set RNG seed for reproducibility ---
    MathEngine::instance().set_rng_seed(42);

    // --- Print summary ---
    print_summary(image_width, image_height, spp, max_depth, scene_file, output_file);

    // --- Build scene ---
    HittableList world;
    if (scene_file == "default") {
        SceneLoader::load_default(world);
    } else {
        if (!SceneLoader::load(scene_file, world)) {
            std::cerr << "Failed to load scene. Using default scene.\n";
            SceneLoader::load_default(world);
        }
    }

    // --- Create camera and renderer ---
    Camera cam(image_width, image_height);
    Renderer renderer(spp, max_depth);

    // --- Render ---
    std::cout << "Rendering...\n";
    auto start = std::chrono::high_resolution_clock::now();

    renderer.render(cam, world, output_file);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    std::cout << "Render time: " << elapsed << " seconds\n";

    return 0;
}
