#version 450

// Input from vertex shader
layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;

// Output
layout(location = 0) out vec4 outColor;

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 model;
    vec3 cameraPos;
    uint numLights;
    // Point lights array follows
} ubo;

// Texture sampler for ground texture
layout(binding = 2) uniform sampler2D groundTexture;

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = ubo.proj * ubo.view * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = ubo.proj * ubo.view * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * 0.1 * 100.0) / (100.0 + 0.1 - clip_space_depth * (100.0 - 0.1)); // get linear value between 0.01 and 100
    return linearDepth / 100.0; // normalize
}

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) {
    // Sample the ground texture with tiling
    vec2 textureCoord = fragPos3D.xz * 0.1; // Scale texture coordinates for tiling
    vec3 textureColor = texture(groundTexture, textureCoord).rgb;
    
    // Just return the texture color without any grid lines
    vec4 color = vec4(textureColor, 0.7); // 70% opacity for the textured ground
    
    return color;
}

void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    gl_FragDepth = computeDepth(fragPos3D);

    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    // Add multiple grid scales for better visual hierarchy
    vec4 grid1 = grid(fragPos3D, 10, true);   // 1 unit grid
    vec4 grid2 = grid(fragPos3D, 1, true);    // 10 unit grid
    
    // Combine grids - take the maximum alpha to ensure visibility
    vec4 combinedGrid = mix(grid2 * 0.5, grid1, grid1.a);
    
    // Ensure minimum visibility and apply fading
    combinedGrid.a = max(combinedGrid.a, 0.1) * fading; // Minimum 10% opacity
    combinedGrid.rgb *= fading;
    
    // Only render if intersection is in front of camera
    outColor = combinedGrid * float(t > 0);
}
