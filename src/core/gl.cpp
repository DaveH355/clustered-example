#include "core.h"
#include "core/util.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <gldoc.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

#include <cmath>
#include <string_view>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

struct Image
{
  unsigned char *data;
  int width, height;
  int numChannels;
};

static Image load_image_from_memory(const unsigned char *rawData, int len)
{
  Image image{};
  image.data = stbi_load_from_memory(rawData, len, &image.width, &image.height,
                                     &image.numChannels, 0);
  if (!image.data)
  {
    std::cerr << "Failed to load image from memory" << std::endl;
  }
  return image;
}
static Image load_image(const char *path)
{
  Image image{};
  image.data =
      stbi_load(path, &image.width, &image.height, &image.numChannels, 0);
  if (!image.data)
  {
    std::cerr << "Failed to load image from path: " << path << std::endl;
  }
  return image;
}
static void free_image(Image &image) { stbi_image_free(image.data); }

static unsigned int create_gltexture(Image &image, bool isLinearColorSpace)
{
  int numChannels = image.numChannels;
  int width = image.width;
  int height = image.height;
  unsigned char *data = image.data;

  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  // Set the texture wrapping/filtering options (on the currently bound texture
  // object)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  auto format = GL_RGB;
  auto internalFormat = GL_RGB;

  if (numChannels == 1) // always assumed linear color space
  {
    format = GL_RED;
    internalFormat = GL_RED;
  }
  else if (numChannels == 3)
  {
    format = GL_RGB;
    internalFormat = isLinearColorSpace ? GL_RGB : GL_SRGB;
  }
  else if (numChannels == 4)
  {
    format = GL_RGBA;
    internalFormat = isLinearColorSpace ? GL_RGBA : GL_SRGB_ALPHA;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
               GL_UNSIGNED_BYTE, data);

  glGenerateMipmap(GL_TEXTURE_2D);
  return textureID;
}

// parentDir is the directory the gltf model lives in
static Image load_cgltf_image(const cgltf_image *image,
                              const std::filesystem::path &parentDir)
{
  // Load embedded image
  if (image->buffer_view)
  {
    cgltf_buffer_view *buffer_view = image->buffer_view;
    const unsigned char *rawImageData =
        static_cast<const unsigned char *>(buffer_view->buffer->data) +
        buffer_view->offset;

    Image i = load_image_from_memory(rawImageData, buffer_view->size);

    printf(
        "Loading embedded image: size on disk (%d kb), width %d, height %d\n",
        static_cast<int>(buffer_view->size / 1000), i.width, i.height);
    return i;
  }

  // Load external image
  if (image->uri)
  {
    namespace fs = std::filesystem;
    // image uri is the relative path, we travel up that path from our model dir
    fs::path combinedPath = parentDir / fs::path(image->uri);
    if (fs::exists(combinedPath))
    {
      combinedPath = fs::canonical(combinedPath);
    }
    else
    {
      std::cout << "Warning: " << combinedPath
                << " does not exist on the filesystem." << std::endl;
    }

    std::cout << "Loading external image from:  " << combinedPath << std::endl;
    return load_image(combinedPath.c_str());
  }

  std::cerr << "Image does not have a valid buffer view or URI" << std::endl;

  return {};
}

static Transform get_node_transform(const cgltf_node &node)
{
  Transform t{};

  if (node.has_matrix)
  {
    glm::mat4 mat4 = glm::make_mat4(node.matrix);

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(mat4, scale, rotation, translation, skew, perspective);

    t = {translation, scale, rotation};
  }
  else
  {

    if (node.has_translation)
    {
      t.set_position(glm::make_vec3(node.translation));
    }

    if (node.has_rotation)
    {
      t.set_rotation(glm::make_quat(node.rotation));
    }

    if (node.has_scale)
    {
      t.set_scale(glm::make_vec3(node.scale));
    }
  }

  return t;
}
// check if the mesh data we want is there.
// after check, can assume meshes will be trianglulated and have indices,
// normals, texcoords
static bool check_mesh(const cgltf_mesh &mesh)
{

  // loop over primitives
  for (size_t i = 0; i < mesh.primitives_count; ++i)
  {
    const cgltf_primitive &primitive = mesh.primitives[i];
    if (primitive.type != cgltf_primitive_type_triangles)
    {
      std::cout << "Mesh Check: Non-triangle primitives are not supported."
                << std::endl;
      return false;
    }
    if (!primitive.indices)
    {
      std::cout << "Mesh Check: Primitive must have indices." << std::endl;
      return false;
    }

    // position not here, assumed to always have position
    bool hasNormal, hasTexcoord = false;
    // check vertex attributes
    for (size_t k = 0; k < primitive.attributes_count; ++k)
    {
      const cgltf_attribute &attribute = primitive.attributes[k];

      if (attribute.data->component_type != cgltf_component_type_r_32f)
      {
        std::cout
            << "Mesh Check: Primitive attribute (position, normal, texcoord,"
               "etc) doesn't use float type. Use float"
            << std::endl;
        return false;
      }

      if (attribute.type == cgltf_attribute_type_normal)
        hasNormal = true;
      if (attribute.type == cgltf_attribute_type_texcoord)
        hasTexcoord = true;
    }
    if (!hasNormal || !hasTexcoord)
    {
      std::cout << "Mesh Check:: Primitive doesn't have either "
                   "normals or texcoords"
                << std::endl;
      return false;
    }
  }
  return true;
}

static void
load_primitive_vertices_and_indices(cgltf_primitive *primitive,
                                    std::vector<Vertex> &vertices,
                                    std::vector<unsigned int> &indices)
{

  // Load positions
  for (size_t j = 0; j < primitive->attributes_count; ++j)
  {
    auto *attribute = &primitive->attributes[j];
    if (attribute->type == cgltf_attribute_type_position)
    {
      cgltf_accessor *accessor = attribute->data;
      for (size_t k = 0; k < accessor->count; ++k) // each element
      {
        glm::vec3 position;
        cgltf_accessor_read_float(accessor, k, &position.x,
                                  3); // each component in element
        vertices.emplace_back(position, glm::vec3{}, glm::vec2{});
        // we load all positions and put placeholders for next loops
      }
    }
  }

  // Load normals
  for (size_t j = 0; j < primitive->attributes_count; ++j)
  {
    auto *attribute = &primitive->attributes[j];
    if (attribute->type == cgltf_attribute_type_normal)
    {
      cgltf_accessor *accessor = attribute->data;
      for (size_t k = 0; k < accessor->count; ++k)
      {
        glm::vec3 normal;
        cgltf_accessor_read_float(accessor, k, &normal.x, 3);
        vertices[k].normal = normal;
      }
    }
  }

  // Load texcoords
  for (size_t j = 0; j < primitive->attributes_count; ++j)
  {
    auto *attribute = &primitive->attributes[j];
    if (attribute->type == cgltf_attribute_type_texcoord)
    {
      cgltf_accessor *accessor = attribute->data;
      for (size_t k = 0; k < accessor->count; ++k)
      {
        glm::vec2 texCoords;
        cgltf_accessor_read_float(accessor, k, &texCoords.x, 2);
        vertices[k].texcoord = texCoords;
      }
    }
  }

  // Load indices
  for (size_t j = 0; j < primitive->indices->count; ++j)
  {
    unsigned int index;
    cgltf_accessor_read_uint(primitive->indices, j, &index, 1);
    indices.push_back(index);
  }
}

namespace Core::GL
{

unsigned int debugDiffuse, debugSpecular;

void init() // called by core init window
{
  debugDiffuse = load_texture(ASSETS_PATH "debug_diffuse.jpg", true);
  debugSpecular = load_texture(ASSETS_PATH "debug_specular.jpg", false);
}
// note: blender will auto trianglulate gltf exports
Model load_gltf(const char *path)
{
  cgltf_options options{};
  cgltf_data *data = nullptr;
  cgltf_result result = cgltf_parse_file(&options, path, &data);

  if (result != cgltf_result_success)
  {
    std::cerr << "Failed to parse glTF file." << std::endl;
  }

  result = cgltf_load_buffers(&options, data, path);
  if (result != cgltf_result_success)
  {
    std::cerr << "Failed to load buffers." << std::endl;
    cgltf_free(data);
  }
  auto *file_type = data->file_type == cgltf_file_type_glb ? "glb" : "gltf";
  std::cout << file_type << " model loaded successfully from: " << path << "\n";

  std::cout << "Mesh count: " << data->meshes_count << "\n";
  std::cout << "Node count: " << data->nodes_count << "\n";
  std::cout << "Material count: " << data->materials_count << "\n";

  std::cout << "Image count: " << data->images_count
            << "\n"; // num of raw images. May be higher than material count due
                     // to material having normal & specular textures, etc
  std::cout << "Texture count: " << data->textures_count
            << "\n"; // num of texture entries (texture may use same image but
                     // define different tex params)
  Model model{};

  // check meshes
  for (size_t i = 0; i < data->meshes_count; ++i)
  {
    if (!check_mesh(data->meshes[i]))
      return model;
  }

  std::unordered_map<std::string_view, unsigned int> loadedImages{};
  int estTexMemUsage = 0; // bytes

  std::filesystem::path modelPath(path);

  // traverse materials and load only diffuse/specular for now
  for (size_t i = 0; i < data->materials_count; ++i)
  {
    const cgltf_material &material = data->materials[i];
    if (!material.has_pbr_metallic_roughness)
    {
      continue;
    }

    auto load = [&](cgltf_image *cgltf_image, bool isLinear)
    {
      if (loadedImages.contains(cgltf_image->name))
      {
        // avoid dup image load because different material can use same texture
        return;
      }
      Image image = load_cgltf_image(cgltf_image, modelPath.parent_path());
      estTexMemUsage += (image.width * image.height * image.numChannels) *
                        1.33; // 1.33 for mipmapping
      loadedImages[cgltf_image->name] = create_gltexture(image, isLinear);
      free_image(image);
    };

    if (material.pbr_metallic_roughness.base_color_texture.texture)
    {
      // diffuse
      load(material.pbr_metallic_roughness.base_color_texture.texture->image,
           false);
    }

    // specular
    if (!material.has_specular || !material.specular.specular_texture.texture)
      continue;

    // load(material.specular.specular_texture.texture->image);
  }
  std::cout << "Loaded " << loadedImages.size() << " images. ";
  std::cout << "Est GPU Memory usage: " << estTexMemUsage / 1'000'000
            << " mb\n";

  for (size_t i = 0; i < data->nodes_count; ++i)
  {
    cgltf_node node = data->nodes[i];

    if (node.children_count != 0)
    {
      std::cout << "node has children. This is not supported yet\n";
      continue;
    }
    if (!node.mesh)
    {
      std::cout << "node is not mesh. Skipping for now\n";
      continue;
    }
    if (node.mesh)
    {
      for (size_t j = 0; j < node.mesh->primitives_count; ++j)
      {
        cgltf_primitive &primitive = node.mesh->primitives[j];

        std::vector<Vertex> vertices{};
        std::vector<unsigned int> indices{};
        load_primitive_vertices_and_indices(&primitive, vertices, indices);

        std::cout << "Indices: " << indices.size() << "\n";

        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);

        GLuint ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int), indices.data(),
                     GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, texcoord));

        Mesh myMesh{};
        myMesh.vao = vao;
        myMesh.vbo = vbo;
        myMesh.ebo = ebo;
        myMesh.transform = get_node_transform(node);
        myMesh.triangleCount = indices.size() / 3;
        myMesh.indicesCount = indices.size();
        std::cout << "Triangles: " << myMesh.triangleCount << "\n";

        // texture loading
        // always give mesh debug textures
        myMesh.diffuseTextureID = debugDiffuse;
        myMesh.specularTextureID = debugSpecular;

        if (!primitive.material ||
            !primitive.material->has_pbr_metallic_roughness)
        {
          // bad
        }
        else
        {
          // diffuse
          cgltf_texture *texture = primitive.material->pbr_metallic_roughness
                                       .base_color_texture.texture;
          if (texture)
          {
            myMesh.diffuseTextureID = loadedImages.at(texture->image->name);
          }
          // specular
          if (primitive.material->has_specular &&
              primitive.material->specular.specular_texture.texture)
          {

            // myMesh.specularTextureID =
            //     loadedImages.at(primitive.material->specular.specular_texture
            //                         .texture->image->name);
          }
        }

        model.meshes.push_back(myMesh);
      }
    }
  }

  cgltf_free(data);
  return model;
}

unsigned int load_texture(const char *path, bool isLinear)
{
  Image image = load_image(path);
  unsigned int textureID = create_gltexture(image, isLinear);
  free_image(image);
  return textureID;
}

// TODO: this is incomplete. Also should be internal api
void draw_mesh_instanced(std::vector<glm::mat4> matrixes, unsigned int IBO,
                         const Mesh &srcMesh)
{
  glBindBuffer(GL_ARRAY_BUFFER, IBO);
  glBufferData(GL_ARRAY_BUFFER, matrixes.size() * sizeof(glm::mat4),
               matrixes.data(), GL_DYNAMIC_DRAW);

  glBindVertexArray(srcMesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES, srcMesh.indicesCount, GL_UNSIGNED_INT,
                          0, matrixes.size());
}
// internal api to instance a vao.
unsigned int create_instance_buffer_object(unsigned int vao)
{
  unsigned int ibo;
  glGenBuffers(1, &ibo);
  glBindBuffer(GL_ARRAY_BUFFER, ibo);

  glBindVertexArray(vao);
  // set attribute pointers for matrix (4 times vec4)
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)0);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(sizeof(glm::vec4)));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(2 * sizeof(glm::vec4)));
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(3 * sizeof(glm::vec4)));

  glVertexAttribDivisor(3, 1);
  glVertexAttribDivisor(4, 1);
  glVertexAttribDivisor(5, 1);
  glVertexAttribDivisor(6, 1);
  return ibo;
}
// internal api to update ibo with matrix data
void update_instance_buffer_object(unsigned int ibo,
                                   const std::vector<Transform> &instances)
{
  const int instancesCount = instances.size();

  std::vector<glm::mat4> matrixes(instancesCount);
  for (int i = 0; i < instancesCount; ++i)
  {
    matrixes[i] = instances[i].get_matrix();
  }

  glBindBuffer(GL_ARRAY_BUFFER, ibo);
  glBufferData(GL_ARRAY_BUFFER, instancesCount * sizeof(glm::mat4),
               matrixes.data(), GL_DYNAMIC_DRAW);
}

void render_fullscreen_quad()
{
  static unsigned int quadVAO, quadVBO = 0;
  if (quadVAO == 0)
  {
    float vertices[] = {
        // positions        // texture Coords
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (GLvoid *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (GLvoid *)(3 * sizeof(GLfloat)));
  }

  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

unsigned int cubeVAO;
unsigned int cubeIndicesCount;

void create_cube_lazy()
{
  static bool created = false;
  if (!created)
  {
    constexpr std::array vertices = {
        // positions
        -0.5f, 0.5f,  -0.5f, //
        0.5f,  0.5f,  -0.5f, //
        0.5f,  0.5f,  0.5f,  //
        -0.5f, 0.5f,  0.5f,  //
        -0.5f, -0.5f, -0.5f, //
        0.5f,  -0.5f, -0.5f, //
        0.5f,  -0.5f, 0.5f,  //
        -0.5f, -0.5f, 0.5f   //
    };

    constexpr std::array indices = {// Front face
                                    0, 1, 2, 2, 3, 0,
                                    // Right face
                                    1, 5, 6, 6, 2, 1,
                                    // Back face
                                    7, 6, 5, 5, 4, 7,
                                    // Left face
                                    4, 0, 3, 3, 7, 4,
                                    // Bottom face
                                    4, 5, 1, 1, 0, 4,
                                    // Top face
                                    3, 2, 6, 6, 7, 3};

    unsigned int vbo, ebo = 0;

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)0);

    cubeIndicesCount = indices.size();

    created = true;
  }
}
void draw_cube()
{
  create_cube_lazy();

  glBindVertexArray(cubeVAO);
  glDrawElements(GL_TRIANGLES, cubeIndicesCount, GL_UNSIGNED_INT, nullptr);
}
void draw_cube(const std::vector<Transform> &instances)
{
  create_cube_lazy();

  static unsigned int ibo;
  static bool created = false;
  if (!created)
  {
    ibo = create_instance_buffer_object(cubeVAO);
    created = true;
  }
  update_instance_buffer_object(ibo, instances);

  glBindVertexArray(cubeVAO);
  glDrawElementsInstanced(GL_TRIANGLES, cubeIndicesCount, GL_UNSIGNED_INT, 0,
                          instances.size());
}

unsigned int sphereVAO;
unsigned int sphereIndicesCount = 0;

void create_sphere_lazy()
{
  constexpr int X_SEGMENTS = 16;
  constexpr int Y_SEGMENTS = 16;

  constexpr float PI = glm::pi<double>();
  constexpr float TWO_PI = glm::two_pi<float>();

  static bool sphereCreated = false;
  if (!sphereCreated)
  {
    std::vector<glm::vec3> positions;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= Y_SEGMENTS; ++y)
    {
      for (int x = 0; x <= X_SEGMENTS; ++x)
      {
        float xSegment = static_cast<float>(x) / static_cast<float>(X_SEGMENTS);
        float ySegment = static_cast<float>(y) / static_cast<float>(Y_SEGMENTS);
        float xPos = std::cos(xSegment * TWO_PI) * std::sin(ySegment * PI);
        float yPos = std::cos(ySegment * PI);
        float zPos = std::sin(xSegment * TWO_PI) * std::sin(ySegment * PI);

        positions.emplace_back(xPos, yPos, zPos);
      }
    }

    for (int y = 0; y < Y_SEGMENTS; ++y)
    {
      for (int x = 0; x < X_SEGMENTS; ++x)
      {
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        indices.push_back(y * (X_SEGMENTS + 1) + x);
        indices.push_back(y * (X_SEGMENTS + 1) + x + 1);

        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        indices.push_back(y * (X_SEGMENTS + 1) + x + 1);
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
      }
    }
    sphereIndicesCount = indices.size(); // or X_SEGMENTS * Y_SEGMENTS * 6

    // Generate sphereVAO and VBO
    glGenVertexArrays(1, &sphereVAO);
    glBindVertexArray(sphereVAO);

    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_STATIC_DRAW);

    unsigned int ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    sphereCreated = true;
  }
}
void draw_sphere()
{
  create_sphere_lazy();

  // draw sphere
  glBindVertexArray(sphereVAO);
  glDrawElements(GL_TRIANGLES, sphereIndicesCount, GL_UNSIGNED_INT, nullptr);
}
// TODO: measure performance of this
void draw_sphere(const std::vector<Transform> &instances)
{
  create_sphere_lazy();

  static unsigned int ibo;
  static bool created = false;
  if (!created)
  {
    ibo = create_instance_buffer_object(sphereVAO);
    created = true;
  }
  update_instance_buffer_object(ibo, instances);

  // draw
  glBindVertexArray(sphereVAO);
  glDrawElementsInstanced(GL_TRIANGLES, sphereIndicesCount, GL_UNSIGNED_INT, 0,
                          instances.size());
}

} // namespace Core::GL
