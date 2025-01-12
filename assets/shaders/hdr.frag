#version 430 core
// dependent on simple_screenspace.vert

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D hdrBuffer;

uniform bool enable;
uniform float exposure;
uniform float gamma;

float linear_to_srgb(float x)
{
    return x <= 0.0031308 ? x * 12.92f : 1.055 * pow(x, 1.0 / 2.4) - 0.055f;
}

void main()
{
    vec3 sampled = texture(hdrBuffer, TexCoords).rgb;

    sampled = vec3(linear_to_srgb(sampled.r), linear_to_srgb(sampled.g), linear_to_srgb(sampled.b));

    FragColor = vec4(sampled, 1.0);
}
