#version 430 core
// simple vertex shader for screenspace effects like
// deferred rendering, ssao, blur etc, that require a fullscreen quad.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos, 1.0);
}
