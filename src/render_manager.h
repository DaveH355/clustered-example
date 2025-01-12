#pragma once

#include "camera.h"
#include "core/shader.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float4.hpp>
#include <stdexcept>
#include <vector>

struct alignas(16) PointLight
{
  glm::vec4 position;
  glm::vec4 color;
  float intensity;
  float radius;
};

namespace Render
{
void init();
void pre_render_checks();

Shader begin_gbuffer_render();
void end_gbuffer_render();

void ssao_pass(glm::mat4 projection);

Shader begin_lighting_pass();
void end_lighting_pass();

void hdr_pass();

bool &is_wireframe();
float &get_hdr_exposure();
float &get_gamma();

struct SSAOUniforms
{
  bool enableSSAO = false;
  int samples = -1;
  float radius = -1;
  float bias = -1;
  float power = -1;
} inline ssaoUniforms;

void set_ssao_resolution(glm::vec2 vec2, bool resize = true);
void set_gbuffer_resolution(glm::vec2 vec2, bool resize = true);

namespace Debug
{
void show_light_positions(const Camera &camera);
}
namespace Compute
{
void cull_lights_compute(const Camera &camera);
void draw_aabbs(const Camera &camera);
} // namespace Compute

} // namespace Render

enum class Resolution
{
  _3840x2160_4K,
  _2560x1600_RETINA,
  _1920_1080,
  _1280_720,
  _960_540,
  _640_360,
  COUNT // Keep this last: it is used for counting the number of
        // enums.
};
constexpr std::array<const char *, static_cast<int>(Resolution::COUNT)>
    RESOLUTION_STRINGS = {
        "3840x2160 (4K)",       //
        "2560x1600 (retina)",   //
        "1920x1080",            //
        "1280x720",             //
        "960x540 (half 1080p)", //
        "640x360",              //
};

constexpr glm::vec2 resolution_to_vec2(Resolution resolution)
{
  using enum Resolution;
  switch (resolution)
  {
  case _3840x2160_4K:
    return {3840, 2160};

  case _2560x1600_RETINA:
    return {2560, 1600};

  case _1920_1080:
    return {1920, 1080};

  case _1280_720:
    return {1280, 720};

  case _960_540:
    return {960, 540};

  case _640_360:
    return {640, 360};
  }
  throw std::invalid_argument("Unknown Resolution");
}

constexpr Resolution int_to_Resolution(int index)
{
  if (index < 0 || index >= static_cast<int>(Resolution::COUNT))
  {
    throw std::out_of_range("Index is out of range for Resolution enum");
  }
  return static_cast<Resolution>(index);
}
