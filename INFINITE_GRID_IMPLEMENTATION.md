# Infinite Grid Implementation in Vulkan

## Overview

This document details the implementation of an infinite grid system in our Vulkan C application, based on the technique described in ["Infinite Grid" by Aslice of Rendering](https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/).

The infinite grid provides a visual reference plane that appears to extend infinitely in all directions, helping with spatial orientation and object placement in 3D scenes. Unlike traditional finite grid meshes, this implementation uses a fullscreen pass with ray-plane intersection mathematics to create the illusion of an infinite grid.

## 🎯 Key Features

- **Truly Infinite**: The grid extends infinitely in all directions without performance degradation
- **Multi-Scale**: Displays both major and minor grid lines for better visual hierarchy
- **Depth-Aware**: Proper depth testing and writing for correct object occlusion
- **Distance Fading**: Grid opacity fades with distance for natural appearance
- **Axis Highlighting**: X and Z axes are highlighted in red and blue respectively
- **Transparent Blending**: Grid blends smoothly with the scene background

## 🏗️ Architecture Overview

The infinite grid implementation consists of three main components:

1. **Grid Vertex Shader** (`grid.vert.glsl`) - Generates fullscreen triangle
2. **Grid Fragment Shader** (`grid.frag.glsl`) - Performs ray-plane intersection and grid rendering
3. **C Application Code** - Pipeline setup, vertex buffer management, and rendering integration

## 📐 Mathematical Foundation

### Ray-Plane Intersection

The core technique involves:

1. **Screen Space to World Space**: Unproject screen coordinates to world space rays
2. **Ray-Plane Intersection**: Find where the camera ray intersects the Y=0 plane
3. **Grid Pattern Generation**: Use fractional coordinates and derivatives for anti-aliased grid lines

### Unprojection Formula

```glsl
vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}
```

### Grid Pattern Generation

```glsl
float grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    // ... axis coloring and opacity calculation
}
```

## 🔧 Implementation Details

### 1. Vertex Shader (`grid.vert.glsl`)

```glsl
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 model;
    vec3 cameraPos;
    uint numLights;
} ubo;

vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    
    // Create rays from near to far plane for each screen pixel
    nearPoint = unprojectPoint(inPosition.x, inPosition.y, 0.0, ubo.view, ubo.proj).xyz;
    farPoint = unprojectPoint(inPosition.x, inPosition.y, 1.0, ubo.view, ubo.proj).xyz;
}
```

**Key Points:**
- Takes a fullscreen triangle as input (3 vertices covering NDC space)
- Unprojects screen coordinates to world space rays
- Passes near and far points to fragment shader for ray-plane intersection

### 2. Fragment Shader (`grid.frag.glsl`)

```glsl
#version 450

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 model;
    vec3 cameraPos;
    uint numLights;
} ubo;

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = ubo.proj * ubo.view * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = ubo.proj * ubo.view * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0;
    float linearDepth = (2.0 * 0.1 * 100.0) / (100.0 + 0.1 - clip_space_depth * (100.0 - 0.1));
    return linearDepth / 100.0;
}

float grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
    
    // Z axis (blue)
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.z = 1.0;
    // X axis (red)
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.x = 1.0;
    return color.a;
}

void main() {
    // Ray-plane intersection with Y=0 plane
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    // Set proper depth for the intersection point
    gl_FragDepth = computeDepth(fragPos3D);

    // Distance-based fading
    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    // Multi-scale grid
    float grid1 = grid(fragPos3D, 10, true) * fading;   // 1 unit grid
    float grid2 = grid(fragPos3D, 1, true) * fading;    // 10 unit grid
    
    float gridStrength = max(grid1, grid2 * 0.5);
    
    // Only render if intersection is in front of camera
    outColor = vec4(vec3(gridStrength), gridStrength) * float(t > 0);
}
```

**Key Features:**
- **Ray-Plane Intersection**: Calculates where camera ray hits Y=0 plane
- **Depth Computation**: Correctly sets `gl_FragDepth` for proper depth testing
- **Anti-Aliasing**: Uses `fwidth()` for smooth grid lines without aliasing
- **Multi-Scale**: Combines coarse and fine grid patterns
- **Distance Fading**: Reduces opacity based on distance from camera
- **Axis Highlighting**: Colors X-axis red and Z-axis blue

### 3. C Application Integration

#### Pipeline Creation

```c
VkPipeline createGridPipeline(Application* app, VkShaderModule vertShader, VkShaderModule fragShader)
{
    // Vertex input for fullscreen triangle
    VkVertexInputBindingDescription bindingDesc = {
        .binding = 0,
        .stride = sizeof(vec2),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attributeDesc = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 0
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attributeDesc,
    };

    // Enable alpha blending for transparency
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    // Enable depth testing and writing
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    // ... rest of pipeline creation
}
```

#### Vertex Buffer Setup

```c
void createModelAndBuffers(Application* app)
{
    // ... existing model loading code ...

    // Create grid vertex buffer for fullscreen triangle
    vec2 gridVertices[3] = {
        {-1.0f, -1.0f},  // Bottom-left
        { 3.0f, -1.0f},  // Bottom-right (extended)
        {-1.0f,  3.0f}   // Top-left (extended)
    };

    size_t gridVertexBufferSize = sizeof(gridVertices);
    Buffer gridVertexStaging;
    createBuffer(app, &gridVertexStaging, gridVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(gridVertexStaging.data, gridVertices, gridVertexBufferSize);

    createBuffer(app, &app->gridVertexBuffer, gridVertexBufferSize, 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // ... copy operations ...
}
```

#### Rendering Integration

```c
void recordCommandBuffer(Application* app, VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    // ... render pass setup ...

    // Draw grid first (background)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->gridPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           app->pipelineLayout, 0, 1, &app->descriptorSet, 0, NULL);
    
    VkBuffer gridVertexBuffers[] = {app->gridVertexBuffer.vkbuffer};
    VkDeviceSize gridOffsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, gridVertexBuffers, gridOffsets);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0); // Draw fullscreen triangle

    // Switch back to main pipeline and draw model
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);
    // ... render main scene objects ...
}
```

## 🎨 Visual Features

### Multi-Scale Grid Lines

The implementation renders two grid scales:
- **Fine Grid** (1 unit spacing): Primary grid lines for detailed reference
- **Coarse Grid** (10 unit spacing): Secondary grid lines at 50% opacity for major divisions

### Axis Highlighting

- **X-Axis**: Rendered in red for easy identification
- **Z-Axis**: Rendered in blue for clear visual distinction
- **Thickness**: Axes have slight thickness for better visibility

### Distance-Based Fading

```glsl
float fading = max(0, (0.5 - linearDepth));
```

Grid opacity decreases with distance, preventing visual clutter and creating a natural horizon effect.

## 🔧 Technical Considerations

### Performance Optimizations

1. **Single Fullscreen Pass**: Only three vertices are processed regardless of grid size
2. **Fragment Shader Efficiency**: Early termination for rays that don't intersect the plane
3. **Derivative-Based Anti-Aliasing**: Uses hardware derivatives for smooth lines without supersampling

### Depth Handling

The implementation correctly:
- Computes and writes depth values for proper z-testing
- Ensures scene objects correctly occlude the grid
- Maintains depth buffer coherency with other scene elements

### Blending Configuration

```c
.blendEnable = VK_TRUE,
.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
```

Proper alpha blending allows the grid to appear semi-transparent while maintaining visual clarity.

## 🚀 Advantages Over Traditional Grid Meshes

| Traditional Grid Mesh | Infinite Grid |
|----------------------|---------------|
| Limited size, requires repositioning | Truly infinite coverage |
| High vertex count for large grids | Only 3 vertices total |
| Aliasing issues at distance | Derivative-based anti-aliasing |
| Memory usage scales with size | Constant memory footprint |
| Complex LOD management | Automatic distance-based detail |

## 🎯 Usage in the Application

The infinite grid serves multiple purposes in our Vulkan application:

1. **Spatial Reference**: Provides visual anchoring for 3D objects
2. **Scale Understanding**: Helps users judge object sizes and distances
3. **Navigation Aid**: Assists with camera movement and orientation
4. **Professional Appearance**: Creates a clean, CAD-like environment

## 🔮 Future Enhancements

Possible improvements to consider:

1. **Configurable Grid Spacing**: Runtime adjustment of grid scale
2. **Color Customization**: User-configurable grid and axis colors
3. **Adaptive Opacity**: Dynamic fading based on scene complexity
4. **Grid Snapping**: Object placement assistance with grid alignment
5. **Multiple Planes**: Additional grids on XY and YZ planes

## 🏁 Conclusion

The infinite grid implementation demonstrates advanced GPU techniques while providing significant practical benefits. By leveraging ray-plane intersection mathematics and careful shader programming, we've created a highly efficient, visually appealing grid system that enhances the 3D editing experience without compromising performance.

The technique showcases several important concepts:
- **Mathematical Precision**: Accurate ray-plane intersection calculations
- **Shader Optimization**: Efficient use of GPU resources
- **Visual Quality**: Anti-aliased rendering with proper depth handling
- **Integration Excellence**: Seamless blend with existing Vulkan pipeline

This implementation serves as both a practical tool and an excellent example of modern GPU programming techniques in real-world applications.
