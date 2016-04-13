#version 330 core

in vec4 ciPosition;
uniform mat4 ciModelViewProjection;

void main()
{
    gl_Position = vec4(ciPosition.x, ciPosition.y, ciPosition.z, 1.0);
}