#!/bin/bash
glslc shaders/skybox.vert.glsl -o compiledshaders/skybox.vert.spv
glslc shaders/skybox.frag.glsl -o compiledshaders/skybox.frag.spv
