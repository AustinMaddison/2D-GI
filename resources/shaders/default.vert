#version 430
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

out vec2 fragTexCoord;

void main() {
    // flip coords to better match the conventiosn of the GUI library i am using.
    // fragTexCoord = abs(inTexCoord.xy - vec2(0, 1.));
    fragTexCoord = abs(inTexCoord);
    gl_Position = vec4(inPosition, 1.0);
}