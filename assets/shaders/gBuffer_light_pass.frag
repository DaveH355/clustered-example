#version 430 core
// dependent on simple_screenspace.vert

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;

struct PointLight
{
    vec4 position;
    vec4 color;
    float intensity;
    float radius;
};

struct Cluster
{
    vec4 minPoint;
    vec4 maxPoint;
    uint count;
    uint lightIndices[100];
};

layout(std430, binding = 1) restrict buffer clusterSSBO {
    Cluster clusters[];
};
layout(std430, binding = 2) restrict buffer lightSSBO
{
    PointLight pointLight[];
};

uniform float zNear;
uniform float zFar;
uniform uvec3 gridSize;
uniform uvec2 screenDimensions;

uniform mat4 view;
uniform bool enableSSAO;

out vec4 FragColor;

in vec2 TexCoords;

void main()
{
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float AmbientOcclusion = enableSSAO ? texture(ssao, TexCoords).r : 1.0f;

    vec3 ambient = vec3(Diffuse * AmbientOcclusion * 0.05);
    vec3 lighting = ambient;

    vec3 viewDir = normalize(-FragPos); // viewpos is (0.0.0)

    // Locating which cluster you are a part of.
    uint zTile = uint((log(abs(FragPos.z) / zNear) * gridSize.z) / log(zFar / zNear));
    vec2 tileSize = screenDimensions / gridSize.xy;
    uvec3 tile = uvec3(gl_FragCoord.xy / tileSize, zTile);
    uint tileIndex =
        tile.x + (tile.y * gridSize.x) + (tile.z * gridSize.x * gridSize.y);

    uint lightCount = clusters[tileIndex].count;
    // if (lightCount >= 95) {
    //     //getting close to limit. Output red color and dip
    //     FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    //     return;
    // }

    for (int i = 0; i < lightCount; ++i)
    {
        uint lightIndex = clusters[tileIndex].lightIndices[i];
        PointLight light = pointLight[lightIndex];

        vec3 position = (view * light.position).xyz;
        vec3 color = light.color.rgb;
        float radius = light.radius;

        vec3 lightDir = normalize(position - FragPos);
        vec3 diffuse =
            max(dot(Normal, lightDir), 0.0) * Diffuse * color * light.intensity;

        const float constant = 1.0f;
        const float linear = 0.35;
        const float quadratic = 0.44;

        float attenuation = 0;
        float distance = length(position - FragPos);
        float edgeSoftness = 0.35; // Adjust to control the softness of the edge
        // (higher=softer, lower=harsher)
        float innerRadius = radius * (1.0 - edgeSoftness);
        float outerRadius =
            radius; // The original radius serves as the outer boundary

        if (distance > outerRadius)
        {
            attenuation = 0.0;
        }
        else
        {
            float smoothFactor = 1.0;
            if (distance > innerRadius)
            {
                // Smooth interpolation between the inner and outer radius
                smoothFactor = smoothstep(outerRadius, innerRadius, distance);
            }

            attenuation = 1.0 / (constant + linear * distance +
                        quadratic * (distance * distance));
            attenuation *= smoothFactor; // Apply smooth factor
            attenuation = min(attenuation * light.intensity, 1.0);
        }

        diffuse *= attenuation;
        lighting += diffuse;
    }

    FragColor = vec4(lighting, 1.0);
}
