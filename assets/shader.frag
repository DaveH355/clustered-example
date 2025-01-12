#version 430 core
struct Material
{
  sampler2D diffuse;
  sampler2D specular;
  float shininess;
};
// allows to control light intensity for different parts
struct Light
{
  vec3 position;

  vec3 ambient;
  vec3 diffuse;
  vec3 specular;

  float constant;
  float linear;
  float quadratic;
};
uniform Light light;
uniform Material material;
uniform vec3 viewPos;

out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

void main()
{
  // ambient
  vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));

  // diffuse
  vec3 norm = normalize(Normal);
  vec3 lightDir = normalize(light.position - FragPos);

  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuse =
      light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));

  vec3 viewDir = normalize(viewPos - FragPos);
  vec3 reflectDir = reflect(-lightDir, norm);

  vec3 halfwayDir = normalize(lightDir + viewDir);
  float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
  // float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specular =
      light.specular * spec * vec3(texture(material.specular, TexCoords));

  float distance = length(light.position - FragPos);
  float attenuation = 1.0 / (light.constant + light.linear * distance +
                             light.quadratic * (distance * distance));

  ambient *= attenuation;
  diffuse *= attenuation;
  specular *= attenuation;

  vec3 result = ambient + diffuse;
  FragColor = vec4(result, 1.0);
}
