#!/bin/bash
set -e  # Exit immediately on error

echo "Cleaning old build..."
rm -rf ./build

mkdir -p build compiledshaders

# Compilation flags
# CFLAGS="-Wpointer-arith -Wformat=2 -Wall -Wextra -Wshadow -ggdb -std=c99 -pedantic"
CFLAGS="-D_DEBUG -DVK_USE_PLATFORM_WAYLAND_KHR"
LDFLAGS="-lvulkan -lm -lglfw -lpthread -ldl"

# Shader list
SHADERS=(
    "grid.vert"
    "grid.frag"
    "tri.vert"
    "tri.frag"
    "compute_path_mask.comp"
    "particle.comp"
    "particle.vert"
    "particle.frag"
    "skybox.vert"
    "skybox.frag"
)

echo "Compiling GLSL shaders..."
for shader in "${SHADERS[@]}"; do
    in="shaders/$shader.glsl"
    out="compiledshaders/$shader.spv"
    if [ -f "$in" ]; then
        echo "  → $shader"
        glslangValidator -V "$in" -o "$out"
    else
        echo "  !! Missing shader: $in"
        exit 1
    fi
done

echo "Compiling sources..."
SRC_FILES=(
    src/pipelines.c
    src/compute.c
    src/cleanup.c
    src/main.c
    src/model.c
    src/helpers.c
    src/texture.c
    src/initialise.c
    src/descriptors.c
    src/skybox.c
)

for src in "${SRC_FILES[@]}"; do
    obj="build/$(basename "${src%.c}.o")"
    echo "  → $(basename "$src")"
    gcc -c "$src" -o "$obj" $CFLAGS
done

echo "Linking final binary..."
gcc build/*.o -o build/tri $CFLAGS $LDFLAGS

echo "Build complete. Running ./build/tri"
./build/tri

# gcc -ggdb lol2.c -o tri -D_DEBUG -DVK_USE_PLATFORM_XLIB_KHR -lvulkan -lglfw -lm && ./tri 
# -save-temps is very useful for preprocessed code and insights 
