#version 430
layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding=0) uniform image2D img_output;

layout(binding = 1) uniform UBO {
    vec3 Mouse_Position;
    int is_additive;
    vec2 path_mask_ws_dims;
} ubo;

void main() {

    float BRUSH_SIZE = 0.45; // DEFAULT 0.75;

// HACK
    if (ubo.is_additive == 0) {
        BRUSH_SIZE = 0.75 * 0.9;
    }
    
    // get index in global work group i.e x,y position
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dims = imageSize(img_output); // fetch image dimensions

     // base pixel colour for image
    vec4 pixel = imageLoad(img_output, pixel_coords); //vec4(0.0, 0.0, 0.0, 1.0);

    // MAIN ---------------------

    // range -1 to 1
    float x = (float(pixel_coords.x * 2 - dims.x) / dims.x);
    float y = (float(pixel_coords.y * 2 - dims.y) / dims.y);

    // convert to world space
    x *= ubo.path_mask_ws_dims.x / 2.0;
    y *= ubo.path_mask_ws_dims.y / 2.0;
    vec3 pixel_ws = vec3(x, 0.0, y);

    float d = distance(pixel_ws, ubo.Mouse_Position);
    d = clamp(d, 0.0, BRUSH_SIZE);
    d = (BRUSH_SIZE -d)/BRUSH_SIZE;

    if (d > 0.0) {
        d = 1.0;
    } else {
        d = 0.0;
    }

    if (ubo.is_additive == 1) {
        // draw path mask
        pixel = max(pixel, vec4(d,d,d, 1.0));
    } else {
        // erase path mask
        float v = max(pixel.x - d, 0.0);
        pixel = vec4(v,v,v, 1.0);
    }
    

    //-----------------------------

    // output to a specific pixel in the image
    imageStore(img_output, pixel_coords, pixel);
}