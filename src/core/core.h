#pragma once

// a set of core utils
#include <gldoc.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <tuple>
#include <vector>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtx/matrix_decompose.hpp>

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texcoord;
};
class Transform
{
private:
  glm::vec3 position;
  glm::vec3 scale;
  glm::quat rotation;

  mutable glm::mat4 matrix;
  mutable bool dirty = true;

public:
  Transform(const glm::vec3 &pos = glm::vec3(0.0f),
            const glm::vec3 &scl = glm::vec3(1.0f),
            const glm::quat &rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
      : position(pos), scale(scl), rotation(rot), matrix(1.0f), dirty(true)
  {
  }

  void set_position(const glm::vec3 &pos)
  {
    position = pos;
    dirty = true;
  }

  void set_scale(const glm::vec3 &scl)
  {
    scale = scl;
    dirty = true;
  }

  void set_rotation(const glm::quat &rot)
  {
    rotation = rot;
    dirty = true;
  }

  const glm::vec3 &get_position() const { return position; }

  const glm::vec3 &get_scale() const { return scale; }

  const glm::quat &get_rotation() const { return rotation; }

  // lazy computed matrix
  const glm::mat4 &get_matrix() const
  {
    if (dirty)
    {
      matrix = glm::translate(glm::mat4(1.0f), position) *
               glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
      dirty = false;
    }
    return matrix;
  }

  void rotate_global(float angleDeg, const glm::vec3 &axis)
  {
    auto identity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    rotation = glm::rotate(identity, glm::radians(angleDeg), axis) * rotation;
    dirty = true;
  }

  void rotate_local(float angleDeg, const glm::vec3 &axis)
  {
    auto identity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    rotation = rotation * glm::rotate(identity, glm::radians(angleDeg), axis);
    dirty = true;
  }
};

// Mesh is not a general API. Mesh is for loading from model loaders.
class Mesh
{

public:
  unsigned int diffuseTextureID{}, specularTextureID{};
  unsigned int vao{}, vbo{}, ebo{};
  unsigned int triangleCount{};
  unsigned int indicesCount{};

  Transform transform;
};

struct Model
{
  std::vector<Mesh> meshes;
};

namespace Core
{
void init_window(int width, int height, const char *title);
void close_window();

void begin_drawing();
void end_drawing();

void toggle_fullscreen();

GLFWwindow *get_window();
float get_deltatime();
bool iswindow_resized();

int get_fps();      // fps delayed update for friendly displaying
int get_live_fps(); // instantaneous fps
int get_targetfps();
void set_targetfps(int target);

std::tuple<int, int> get_framebuffer_size();
std::tuple<int, int> get_inital_window_dimensions();

namespace Input
{

bool iskey_combo_pressed(int key, int mod);
bool iskey_down(int key);
bool iskey_released(int key);
bool iskey_pressed(int key);

bool iscursor_disabled();
void toggle_mouse();
float get_scrolldelta();
std::tuple<double, double> get_mouse_pos_delta();

} // namespace Input

namespace GL
{
Model load_gltf(const char *path);
unsigned int load_texture(const char *path, bool isLinear);

void draw_mesh_instanced(std::vector<glm::mat4> matrixes, unsigned int IBO,
                         const Mesh &srcMesh);

void render_fullscreen_quad();

// simple vertex only meshes.
void draw_cube();
void draw_cube(const std::vector<Transform> &instances);

void draw_sphere();
void draw_sphere(const std::vector<Transform> &instances);

} // namespace GL

} // namespace Core
