#version 430
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

out vec2 fragTexCoord;

uniform mat4x4 uModel;

void main() {
    fragTexCoord = inTexCoord;
    gl_Position = uModel * vec4(inPosition, 1.0);
}