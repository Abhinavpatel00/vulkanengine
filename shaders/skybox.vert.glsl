#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outTexCoord;

// IMPORTANT: Match C struct order (proj first, then view)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
} ubo;

void main() {
    // Sample direction in local cube space; position uses only camera rotation (no translation)
    outTexCoord = inPosition;
    vec4 pos = ubo.proj * mat4(mat3(ubo.view)) * vec4(inPosition, 1.0);
    gl_Position = pos.xyww; // push to far plane
}
