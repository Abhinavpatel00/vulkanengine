#!/bin/bash
set -e

echo "🧹 Cleaning previous build..."
rm -f ./build/tri

echo "🛠️  Compiling GLSL shaders..."
glslangValidator -V shaders/tri.vert.glsl -o shaders/tri.vert.spv
glslangValidator -V shaders/grid.vert.glsl -o shaders/grid.vert.spv
glslangValidator -V shaders/grid.frag.glsl -o shaders/grid.frag.spv
glslangValidator -V shaders/tri.frag.glsl -o shaders/tri.frag.spv

echo "📁 Ensuring build directory exists..."
mkdir -p build

echo "📦 Compiling Nuklear (only if needed)..."
if [ ! -f build/nuklear.o ]; then
    gcc -c -o build/nuklear.o nuklear.c
fi

echo "🔧 Compiling main.c..."
clang -c main.c -o build/main.o \
  -Wpointer-arith -Wformat=2 -Wall -Wextra -Wshadow \
  -ggdb -std=c99 -pedantic  \
  -D_DEBUG -DVK_USE_PLATFORM_WAYLAND_KHR

echo "🔗 Linking executable..."
clang build/main.o build/nuklear.o -o build/tri \
  -Wpointer-arith -Wformat=2 -Wall -Wextra -Wshadow \
  -ggdb -std=c99 -pedantic  \
  -D_DEBUG -DVK_USE_PLATFORM_WAYLAND_KHR \
  -lvulkan -lm -lglfw -lpthread -ldl

echo "📈 Running with Valgrind Massif..."
valgrind --tool=massif --massif-out-file=massif.out ./build/tri

echo "📊 Opening Massif Visualizer..."
massif-visualizer massif.out
