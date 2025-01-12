#include "render_manager.h"
#include "camera.h"
#include "core/core.h"
#include "core/shader.h"
#include "core/util.h"
#include "debug/debug_manager.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <gldoc.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/ext/vector_uint2.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

// HACK: access light info from main.cpp
extern std::vector<PointLight> lightList;
extern std::vector<Transform> lightPositions;
extern std::vector<glm::mat4> lightMats;
extern unsigned int lightSSBO;

struct GBufferFramebuffer
{
  unsigned int fbo, gPosition, gNormal, gAlbedoSpec, rbo;
  bool dirty = false;

  void create(int width, int height)
  {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // - position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           gPosition, 0);

    // - normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           gNormal, 0);

    // - color + specular color buffer
    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                           gAlbedoSpec, 0);

    // - tell OpenGL which color attachments we'll use (of this framebuffer) for
    // rendering
    unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                   GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    // renderbuffer
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
      printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
    }
  }

  void destroy()
  {
    glDeleteFramebuffers(1, &fbo);

    glDeleteTextures(1, &gPosition);
    glDeleteTextures(1, &gNormal);
    glDeleteTextures(1, &gAlbedoSpec);
    glDeleteRenderbuffers(1, &rbo);
  }
} gBuffer;

// simple framebuffer with 1 color attachment
struct SimpleFrameBuffer
{

  unsigned int fbo, color;
  bool dirty = false;
  void create(int width, int height, auto internalFormat, auto inputFormat)
  {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    // SSAO color buffer
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                 inputFormat, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
      printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
    }
  }
  void destroy()
  {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &color);
  }
};

// forward declares
// ===============
namespace Render
{
namespace Compute
{
// TODO: clean this up ... or not
constexpr unsigned int gridSizeX = 12;
constexpr unsigned int gridSizeY = 12;
constexpr unsigned int gridSizeZ = 24;
constexpr unsigned int numClusters = gridSizeX * gridSizeY * gridSizeZ;
void init();
} // namespace Compute
namespace Debug
{
void init();
}

} // namespace Render
//============

namespace Render
{

// ssao
SimpleFrameBuffer ssao, ssaoBlur;
unsigned int noiseTexture; // noise texture for tiling over the screen
std::vector<glm::vec3> ssaoKernel;
Shader ssaoShader, ssaoBlurShader;

// NOTE: initial values set by args parser
glm::vec2 ssaoResolution(-1, -1);

// gbuffer
glm::vec2 gBufferResolution(-1, -1);
Shader geoPassShader;
Shader lightPassShader;

// hdr. We render lighting into hdr fbo
SimpleFrameBuffer hdr;
Shader hdrShader;

void init()
{

  // init fbos
  auto [width, height] = Core::get_framebuffer_size();
  gBuffer.create(gBufferResolution.x, gBufferResolution.y);
  hdr.create(width, height, GL_RGBA16F, GL_RGBA); // hdr alwways tied to fbo
                                                  // size

  ssao.create(ssaoResolution.x, ssaoResolution.y, GL_RED, GL_RED);
  ssaoBlur.create(ssaoResolution.x, ssaoResolution.y, GL_RED, GL_RED);

  // load shaders
  {
    // shader to fill gBuffer with data
    geoPassShader = Shader(ASSETS_PATH "shaders/gBuffer_geo_pass.vert",
                           ASSETS_PATH "shaders/gBuffer_geo_pass.frag");

    // light pass is basically a screenspace effect
    lightPassShader = Shader(ASSETS_PATH "shaders/base/simple_screenspace.vert",
                             ASSETS_PATH "shaders/gBuffer_light_pass.frag");

    ssaoShader = Shader(ASSETS_PATH "shaders/base/simple_screenspace.vert",
                        ASSETS_PATH "shaders/ssao.frag");
    ssaoBlurShader = Shader(ASSETS_PATH "shaders/base/simple_screenspace.vert",
                            ASSETS_PATH "shaders/ssao_blur.frag");
    hdrShader = Shader(ASSETS_PATH "shaders/base/simple_screenspace.vert",
                       ASSETS_PATH "shaders/hdr.frag");

    lightPassShader.use();
    lightPassShader.set_int("gPosition", 0);
    lightPassShader.set_int("gNormal", 1);
    lightPassShader.set_int("gAlbedoSpec", 2);
    lightPassShader.set_int("ssao", 3);

    ssaoShader.use();
    ssaoShader.set_int("gPosition", 0);
    ssaoShader.set_int("gNormal", 1);
    ssaoShader.set_int("texNoise", 2);

    ssaoBlurShader.use();
    ssaoBlurShader.set_int("ssaoInput", 0);

    hdrShader.use();
    hdrShader.set_int("hdrBuffer", 0);
  }

  // init ssao
  {
    // generate sample kernel
    std::uniform_real_distribution<GLfloat> randomFloats(
        0.0, 1.0); // generates random floats between 0.0 and 1.0
    ssaoKernel.reserve(64);
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 64; ++i)
    {
      glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0,
                       randomFloats(generator) * 2.0 - 1.0,
                       randomFloats(generator));
      sample = glm::normalize(sample);
      sample *= randomFloats(generator);
      float scale = float(i) / 64.0f;

      // scale samples s.t. they're more aligned to center of kernel
      auto lerp = [](float a, float b, float f) { return a + f * (b - a); };
      scale = lerp(0.1f, 1.0f, scale * scale);
      sample *= scale;
      ssaoKernel.push_back(sample);
    }

    // generate noise texture
    // ----------------------
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++)
    {
      glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0,
                      randomFloats(generator) * 2.0 - 1.0,
                      0.0f); // rotate around z-axis (in tangent space)
      ssaoNoise.push_back(glm::normalize(noise));
    };
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT,
                 ssaoNoise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  Compute::init();
  Debug::init();
}

// usually we reize, but on intialzation, we don't
void set_ssao_resolution(glm::vec2 vec2, bool resize)
{
  ssaoResolution = vec2;
  if (resize)
  {
    ssao.dirty = true;
    ssaoBlur.dirty = true;
  }
}
void set_gbuffer_resolution(glm::vec2 vec2, bool resize)
{
  gBufferResolution = vec2;
  if (resize)
  {
    gBuffer.dirty = true;
  }
}

bool &is_wireframe()
{
  static bool wireframe = false;
  return wireframe;
}
float &get_hdr_exposure()
{
  static float exposure = 1.0f;
  return exposure;
}
float &get_gamma()
{
  static float gamma = 2.2f;
  return gamma;
}

void pre_render_checks()
{
  if (Core::iswindow_resized())
  {
    hdr.destroy();

    auto [width, height] = Core::get_framebuffer_size();
    hdr.create(width, height, GL_RGBA16F, GL_RGBA); // hdr alwways tied to fbo
                                                    // size
  }
  if (gBuffer.dirty)
  {
    gBuffer.dirty = false;
    gBuffer.destroy();
    gBuffer.create(gBufferResolution.x, gBufferResolution.y);
  }
  if (ssao.dirty)
  {
    ssao.dirty = false;
    ssao.destroy();
    ssao.create(ssaoResolution.x, ssaoResolution.y, GL_RED, GL_RED);
  }
  if (ssaoBlur.dirty)
  {
    ssaoBlur.dirty = false;
    ssaoBlur.destroy();
    ssaoBlur.create(ssaoResolution.x, ssaoResolution.y, GL_RED, GL_RED);
  }
}
Shader begin_gbuffer_render()
{
  glViewport(0, 0, gBufferResolution.x, gBufferResolution.y);

  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (is_wireframe())
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }

  geoPassShader.use();
  return geoPassShader;
}
void end_gbuffer_render()
{
  // reset glViewport to default
  auto [width, height] = Core::get_framebuffer_size();
  glViewport(0.0, 0.0, width, height);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // reset wireframe
  // glBindFramebuffer(GL_FRAMEBUFFER, 0);      // bind to default fbo
}
void ssao_pass(glm::mat4 projection)
{
  if (!ssaoUniforms.enableSSAO)
  {
    return;
  }
  auto [width, height] = Core::get_framebuffer_size();

  glViewport(0, 0, ssaoResolution.x, ssaoResolution.y);
  glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo);
  glClear(GL_COLOR_BUFFER_BIT);
  ssaoShader.use();
  // Send kernel + rotation
  for (unsigned int i = 0; i < 64; ++i)
    ssaoShader.set_vec3(("samples[" + std::to_string(i) + "]").c_str(),
                        ssaoKernel[i]);
  ssaoShader.set_mat4("projection", projection);
  ssaoShader.set_int("kernelSize", ssaoUniforms.samples);
  ssaoShader.set_float("radius", ssaoUniforms.radius);
  ssaoShader.set_float("bias", ssaoUniforms.bias);
  ssaoShader.set_float("power", ssaoUniforms.power);
  ssaoShader.set_uvec2("screenDimensions", {width, height});

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gBuffer.gPosition);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gBuffer.gNormal);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, noiseTexture);
  Core::GL::render_fullscreen_quad();

  // Blur ssao to remove noise
  glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlur.fbo);

  glClear(GL_COLOR_BUFFER_BIT);
  ssaoBlurShader.use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ssao.color);
  Core::GL::render_fullscreen_quad();

  // glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // reset glViewport to default
  glViewport(0.0, 0.0, width, height);
}

Shader begin_lighting_pass()
{
  // we render into hdr fbo
  glBindFramebuffer(GL_FRAMEBUFFER, hdr.fbo);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  lightPassShader.use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gBuffer.gPosition);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gBuffer.gNormal);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gBuffer.gAlbedoSpec);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, ssaoBlur.color); // read ssao from blur fbo

  using namespace Compute;
  auto [width, height] = Core::get_framebuffer_size();
  lightPassShader.set_bool("enableSSAO", ssaoUniforms.enableSSAO);
  lightPassShader.set_uvec3("gridSize", {gridSizeX, gridSizeY, gridSizeZ});
  lightPassShader.set_uvec2("screenDimensions", {width, height});

  return lightPassShader; // return shader for further uniform setting
}
void end_lighting_pass()
{
  Core::GL::render_fullscreen_quad();
  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void hdr_pass()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  hdrShader.use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hdr.color);

  hdrShader.set_float("exposure", get_hdr_exposure());
  hdrShader.set_float("gamma", get_gamma());

  Core::GL::render_fullscreen_quad();
}
namespace Debug
{

// constantShader outputs constant color

Shader constantInstanced;
Shader constantShader;

void init()
{
  // load shaders
  constantInstanced = Shader(ASSETS_PATH "shaders/constant_instanced.vert",
                             ASSETS_PATH "shaders/constant.frag");
  constantShader = Shader(ASSETS_PATH "shaders/constant.vert",
                          ASSETS_PATH "shaders/constant.frag");
}

void show_light_positions(const Camera &camera)
{
  // copy depth values from gBuffer to default
  auto [dstWidth, dstHeight] = Core::get_framebuffer_size();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer.fbo); // read from gBuffer
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
  glBlitFramebuffer(0, 0, gBufferResolution.x, gBufferResolution.y, 0, 0,
                    dstWidth, dstHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  constantInstanced.use();
  constantInstanced.set_mat4("projection", camera.projection);
  constantInstanced.set_mat4("view", camera.view);
  constantInstanced.set_vec4("color", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));



  Core::GL::draw_cube(lightPositions);

}

} // namespace Debug

namespace Compute
{

struct Plane
{
  glm::vec3 normal;
  float distance;

  // Calculate the signed distance from the plane to a point
  float distanceToPoint(const glm::vec3 &point) const
  {
    return glm::dot(normal, point) + distance;
  }
};

struct alignas(16) Cluster
{
  glm::vec4 minPoint;
  glm::vec4 maxPoint;
  unsigned int count;
  unsigned int
      lightIndices[100]; // the lights visible to this cluster. Elements
                         // are indices that access the global light ssbo
};


unsigned int clusterGridSSBO;

// TODO: light ssbo is not created here for simplicity. Change that?
void init_ssbos()
{

  // clusterGridSSBO
  {
    glGenBuffers(1, &clusterGridSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, clusterGridSSBO);

    // NOTE: we only need to allocate memory. No need for initialization because
    // comp shader builds the AABBs, count is reset every frame and indices are
    // overridden
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Cluster) * numClusters,
                 nullptr, GL_STATIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, clusterGridSSBO);
  }
}

std::array<Plane, 6> extractFrustumPlanes(const glm::mat4 &viewProj)
{
  std::array<Plane, 6> planes{};
  // Left plane
  planes[0].normal.x =
      viewProj[0][3] + viewProj[0][0];
  planes[0].normal.y =
      viewProj[1][3] + viewProj[1][0];
  planes[0].normal.z =
      viewProj[2][3] + viewProj[2][0];
  planes[0].distance =
      viewProj[3][3] + viewProj[3][0];

  // Right plane
  planes[1].normal.x =
      viewProj[0][3] - viewProj[0][0];
  planes[1].normal.y =
      viewProj[1][3] - viewProj[1][0];
  planes[1].normal.z =
      viewProj[2][3] - viewProj[2][0];
  planes[1].distance =
      viewProj[3][3] - viewProj[3][0];

  // Bottom plane
  planes[2].normal.x =
      viewProj[0][3] + viewProj[0][1];
  planes[2].normal.y =
      viewProj[1][3] + viewProj[1][1];
  planes[2].normal.z =
      viewProj[2][3] + viewProj[2][1];
  planes[2].distance =
      viewProj[3][3] + viewProj[3][1];

  // Top plane
  planes[3].normal.x =
      viewProj[0][3] - viewProj[0][1];
  planes[3].normal.y =
      viewProj[1][3] - viewProj[1][1];
  planes[3].normal.z =
      viewProj[2][3] - viewProj[2][1];
  planes[3].distance =
      viewProj[3][3] - viewProj[3][1];

  // Near plane
  planes[4].normal.x =
      viewProj[0][3] + viewProj[0][2];
  planes[4].normal.y =
      viewProj[1][3] + viewProj[1][2];
  planes[4].normal.z =
      viewProj[2][3] + viewProj[2][2];
  planes[4].distance =
      viewProj[3][3] + viewProj[3][2];

  // Far plane
  planes[5].normal.x =
      viewProj[0][3] - viewProj[0][2];
  planes[5].normal.y =
      viewProj[1][3] - viewProj[1][2];
  planes[5].normal.z =
      viewProj[2][3] - viewProj[2][2];
  planes[5].distance =
      viewProj[3][3] - viewProj[3][2];

  // Normalize the plane equations
  for (int i = 0; i < 6; ++i)
  {
    float length = glm::length(planes[i].normal);
    planes[i].normal /= length;
    planes[i].distance /= length;
  }
  return planes;
}

bool isLightInFrustum(const PointLight &light,
                      const std::array<Plane, 6> &planes)
{
  for (const auto &plane : planes)
  {
    float distance = plane.distanceToPoint(glm::vec3(light.position));
    if (distance < -light.radius)
    {
      // sphere outside of Frustrum
      return false;
    }
  }
  // sphere intersects with or is completely inside the frustum
  return true;
}

void update_ssbos(const Camera &camera)
{
  // cull lights
  static Timer timer;
  timer.start();

  glm::mat4 viewProj = camera.projection * camera.view;
  std::array<Plane, 6> planes = extractFrustumPlanes(viewProj);

  std::vector<PointLight> visibleLights;
  for (int k = 0; k < lightList.size(); ++k)
  {
    PointLight &light = lightList.at(k);
    Transform &t = lightPositions.at(
        k); // serves as center that doesn't change :D, very messy
    const int radius = 10;
    const float speed = 20;
    // light.position.x =
    //     t.position.x + radius * cos(glm::radians(glfwGetTime() * speed));
    // light.position.z =
    //     t.position.z + radius * sin(glm::radians(glfwGetTime() * speed));

    if (isLightInFrustum(light, planes))
    {
      visibleLights.push_back(light);
    }
  }
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               visibleLights.size() * sizeof(PointLight), visibleLights.data(),
               GL_DYNAMIC_DRAW);

  DebugGui::labeledFloatManager.setValue("Frustum Cull Lights ms: ",
                                         timer.stop_and_get_time_ms());
  DebugGui::labeledFloatManager.setValue("Cull result: ", visibleLights.size());
}

Shader clusterComp;
Shader cullLightComp;

void cull_lights_compute(const Camera &camera)
{
  update_ssbos(camera);

  auto [width, height] = Core::get_framebuffer_size();

  // build AABBs, doesn't need to run every frame but fast
  clusterComp.use();
  clusterComp.set_float("zNear", camera.near);
  clusterComp.set_float("zFar", camera.far);
  clusterComp.set_mat4("inverseProjection", glm::inverse(camera.projection));
  clusterComp.set_uvec3("gridSize", {gridSizeX, gridSizeY, gridSizeZ});
  clusterComp.set_uvec2("screenDimensions", {width, height});

  glDispatchCompute(gridSizeX, gridSizeY, gridSizeZ);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // cull lights
  cullLightComp.use();
  cullLightComp.set_mat4("viewMatrix", camera.view);

  glDispatchCompute(27, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void init()
{
  init_ssbos();
  // load shaders
  clusterComp = Shader(ASSETS_PATH "shaders/clusterShader.comp");
  cullLightComp = Shader(ASSETS_PATH "shaders/clusterCullLightShader.comp");

}

} // namespace Compute

} // namespace Render
