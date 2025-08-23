#version 450

// Input vertex attributes
layout(location = 0) in vec2 inPosition;

// Output to fragment shader
layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 model;
    vec3 cameraPos;
    uint numLights;
    // Point lights array follows but we don't need it in vertex shader
} ubo;

vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    
    // Unproject the position to get world space points
    nearPoint = unprojectPoint(inPosition.x, inPosition.y, 0.0, ubo.view, ubo.proj).xyz; // Near plane
    farPoint = unprojectPoint(inPosition.x, inPosition.y, 1.0, ubo.view, ubo.proj).xyz;  // Far plane
}
