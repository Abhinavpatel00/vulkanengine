#version 450

layout(location = 0) in vec4 inPosition;

void main() {
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
    gl_PointSize = 2.0;
}