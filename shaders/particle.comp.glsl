#version 450

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0) buffer Particles {
    vec4 pos[];
} particles;

layout (binding = 1) uniform Uniforms {
    vec2 mouse_pos;
    float delta_time;
};

void main() {
    uint index = gl_GlobalInvocationID.x;

    // Add some velocity and gravity
    vec2 velocity = particles.pos[index].zw;
    velocity.y -= 9.8 * delta_time; // Gravity
    particles.pos[index].xy += velocity * delta_time;

    // If particle is off-screen, reset it to the mouse position
    if (particles.pos[index].y < -1.0) {
        particles.pos[index].xy = mouse_pos;
        particles.pos[index].zw = vec2(0.0, 0.0);
    }
}
