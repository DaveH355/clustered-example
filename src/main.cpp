#include "arg_parser.h"
#include "camera.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gldoc.hpp>
#include "core/util.h"
#include "debug/debug_manager.h"
#include "core/core.h"
#include "core/shader.h"
#include "render_manager.h"
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_uint2.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <random>
#include <chrono>
#include <strings.h>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void process_input();

std::vector<PointLight> lightList;
std::vector<Transform> lightPositions;
std::vector<glm::mat4> lightMats;
unsigned int lightSSBO;

GLFWwindow *window;

Camera camera{};

int main(int argc, char *argv[])
{
  if (!ArgParser::parse(argc, argv))
    return 0;

  Core::init_window(1920, 1080, "OpenGL playground");
  window = Core::get_window();



  // init globals

  DebugGui::init();
  Render::init();

  Shader quadShader(ASSETS_PATH "quad.vert", ASSETS_PATH "quad.frag");

  Timer modelTimer;
  modelTimer.start();
  Model myModel = Core::GL::load_gltf(ASSETS_PATH "models/sponza_rotated.glb");
  modelTimer.stop_and_print();

  // Model myModel = Core::GL::load_gltf(ASSETS_PATH
  // "models/sponza_low_res.glb");
  // Model myModel = Core::GL::load_gltf(ASSETS_PATH "models/untitled.glb");

  {
    std::mt19937 rng = []()
    {
      std::random_device rd{};

      // Create seed_seq with clock and 7 random numbers from
      // std::random_device
      std::seed_seq ss{
          static_cast<std::seed_seq::result_type>(
              std::chrono::steady_clock::now().time_since_epoch().count()),
          rd(),
          rd(),
          rd(),
          rd(),
          rd(),
          rd(),
          rd()};

      return std::mt19937{ss};
    }();

    constexpr int numLights = 1024;
    std::uniform_real_distribution<float> distXZ(-125.0f, 125.0f);
    std::uniform_real_distribution<float> distY(0, 55.0f);

    lightList.reserve(numLights);
    for (int i = 0; i < numLights; ++i)
    {
      PointLight light{};
      // Generating random positions within the specified ranges
      float randomX = distXZ(rng);
      float randomY = distY(rng);
      float randomZ = distXZ(rng);

      glm::vec4 position(1.0f);
      position.x = randomX;
      position.y = randomY;
      position.z = randomZ;

      light.position = position;
      // printf("%.2f, %.2f, %.2f\n", randomX, randomY, randomZ);

      float rColor = static_cast<float>(((rand() % 100) / 200.0f) +
                                        0.5); // between 0.5 and 1.)
      float gColor = static_cast<float>(((rand() % 100) / 200.0f) +
                                        0.5); // between 0.5 and 1.)
      float bColor = static_cast<float>(((rand() % 100) / 200.0f) +
                                        0.5); // between 0.5 and 1.)
      light.color = {rColor, gColor, bColor, 1.0f};

      light.intensity = 1.7;
      light.radius = 13.0f;
      lightList.push_back(light);

      Transform t{position};
      t.set_scale(glm::vec3{0.5f});

      lightPositions.push_back(t);
    }

    glGenBuffers(1, &lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);

    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 lightList.size() * sizeof(PointLight), lightList.data(),
                 GL_DYNAMIC_DRAW);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }

  while (!glfwWindowShouldClose(window))
  {
    Core::begin_drawing();

    // input
    process_input();

    // update camera matrixes
    auto [width, height] = Core::get_framebuffer_size();


    camera.update_matrixes(width, height);
    glm::mat4 view = camera.view;
    glm::mat4 projection = camera.projection;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // compute

    Render::Compute::cull_lights_compute(camera);

    Render::pre_render_checks();
    Shader gBufferGeoPassShader = //
        Render::begin_gbuffer_render();

    // render
    gBufferGeoPassShader.set_mat4("view", view);
    gBufferGeoPassShader.set_mat4("projection", projection);

    // draw scene
    for (Mesh &myMesh : myModel.meshes)
    {
      glActiveTexture(GL_TEXTURE0);

      glBindTexture(GL_TEXTURE_2D, myMesh.diffuseTextureID);
      gBufferGeoPassShader.set_mat4("model",
                                    myMesh.transform.get_matrix());

      // glActiveTexture(GL_TEXTURE1);
      // glBindTexture(GL_TEXTURE_2D, myMesh.specularTextureID);
      // shader.set_int("material.specular", 1);

      glBindVertexArray(myMesh.vao);
      glDrawElements(GL_TRIANGLES, myMesh.indicesCount, GL_UNSIGNED_INT, 0);
    }
    Render::end_gbuffer_render();

    Render::ssao_pass(projection);

    Shader lightPassShader = //
        Render::begin_lighting_pass();

    lightPassShader.set_float("zNear", camera.near);
    lightPassShader.set_float("zFar", camera.far);
    lightPassShader.set_mat4("view", view);
    // glm::vec4 lightPosView = view * glm::vec4(lightPos, 1.0);
    // lightPassShader.set_vec4("light.position", lightPosView);
    // lightPassShader.set_vec4("light.color",
    // glm::vec4(1.0, 1.0, 1.0, 1.0f));

    Render::end_lighting_pass();

    Render::hdr_pass();

    // Render::Debug::show_light_positions(camera);
    // Render::Compute::draw_aabbs(camera);

    // quadShader.use();
    // glDisable(GL_DEPTH_TEST);
    // glActiveTexture(GL_TEXTURE0);
    //
    // glBindTexture(GL_TEXTURE_2D, framebuffer.gAlbedoSpec);
    // Core::GL::render_fullscreen_quad();

    DebugGui::draw(&camera);

    // end render
    Core::end_drawing();
  }

  DebugGui::destroy();

  Core::close_window();

  return 0;
}

void process_input()
{
  using namespace Core::Input;

  if (iskey_pressed(GLFW_KEY_ESCAPE))
  {
    glfwSetWindowShouldClose(window, true);
    return;
  }
  if (iskey_pressed(GLFW_KEY_X))
  {
    toggle_mouse();
  }
  if (iskey_pressed(GLFW_KEY_F11))
  {
    Core::toggle_fullscreen();
  }
  // disallow moving camera when cursor active
  if (!iscursor_disabled())
    return;

  camera.update_mouse();
  if (iskey_down(GLFW_KEY_W))
  {
    camera.process_keyboard(MoveType::FORWARD);
  }
  if (iskey_down(GLFW_KEY_A))
  {
    camera.process_keyboard(MoveType::LEFT);
  }
  if (iskey_down(GLFW_KEY_S))
  {
    camera.process_keyboard(MoveType::BACKWARD);
  }
  if (iskey_down(GLFW_KEY_D))
  {
    camera.process_keyboard(MoveType::RIGHT);
  }
  if (iskey_down(GLFW_KEY_LEFT_SHIFT))
  {
    camera.process_keyboard(MoveType::DOWN);
  }
  if (iskey_down(GLFW_KEY_SPACE))
  {
    camera.process_keyboard(MoveType::UP);
  }
  // pitch
  if (iskey_down(GLFW_KEY_UP))
  {
    camera.process_keyboard(MoveType::PITCH_UP);
  }
  if (iskey_down(GLFW_KEY_DOWN))
  {
    camera.process_keyboard(MoveType::PITCH_DOWN);
  }
  if (iskey_down(GLFW_KEY_LEFT))
  {
    camera.process_keyboard(MoveType::PITCH_LEFT);
  }
  if (iskey_down(GLFW_KEY_RIGHT))
  {
    camera.process_keyboard(MoveType::PITCH_RIGHT);
  }
}
