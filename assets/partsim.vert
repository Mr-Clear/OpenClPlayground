#version 330

layout(location = 0) in vec2 pos;

uniform mat4 matrix;

void main() {
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
