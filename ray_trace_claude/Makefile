# =============================================================================
# Makefile — Configurable Ray Tracer
# =============================================================================
# Build targets:
#   make           → Build with optimizations (-O2)
#   make debug     → Build with debug symbols and sanitizers
#   make clean     → Remove build artifacts
#   make run       → Build and run with default settings
# =============================================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I.

# Release flags: -O2 for reasonable speed without breaking IEEE float behavior.
# Note: -ffast-math is intentionally NOT used because it would interfere with
# the number format simulation (it reorders/fuses float ops, changing precision).
RELEASE_FLAGS = -O2

# Debug flags: sanitizers catch memory bugs and undefined behavior
DEBUG_FLAGS = -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer

TARGET = raytracer
SRC    = main.cpp

# Header dependencies (rebuild if any header changes)
HEADERS = number_format.h math_engine.h scalar.h vec3.h ray.h \
          geometry.h material.h camera.h renderer.h scene_loader.h

.PHONY: all debug clean run

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -o $(TARGET) $(SRC)

debug: $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(TARGET)_debug $(SRC)

clean:
	rm -f $(TARGET) $(TARGET)_debug *.ppm

# Quick test: build and run with all defaults (no interactive prompts)
run: $(TARGET)
	./$(TARGET) --defaults
