
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <gldoc.hpp>
#include "debug_manager.h"
#include "camera.h"
#include "core/core.h"
#include <iostream>
#include <map>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "render_manager.h"

// logging
void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                      GLsizei length, const GLchar *message,
                      const void *user_param)
{
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    return; // ignore notifications

  const char *sourceStr;
  switch (source)
  {
  case GL_DEBUG_SOURCE_API:
    sourceStr = "API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
    sourceStr = "Window System";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    sourceStr = "Shader Compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    sourceStr = "Third Party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION:
    sourceStr = "Application";
    break;
  case GL_DEBUG_SOURCE_OTHER:
    sourceStr = "Other";
    break;
  default:
    sourceStr = "Unknown";
    break;
  }

  const char *typeStr;
  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR:
    typeStr = "Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    typeStr = "Deprecated Behavior";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    typeStr = "Undefined Behavior";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    typeStr = "Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    typeStr = "Performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    typeStr = "Marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    typeStr = "Push Group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    typeStr = "Pop Group";
    break;
  case GL_DEBUG_TYPE_OTHER:
    typeStr = "Other";
    break;
  default:
    typeStr = "Unknown";
    break;
  }

  const char *severityStr;
  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH:
    severityStr = "High";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    severityStr = "Medium";
    break;
  case GL_DEBUG_SEVERITY_LOW:
    severityStr = "Low";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    severityStr = "Notification";
    break;
  default:
    severityStr = "Unknown";
    break;
  }

  std::cout << "\nOpenGL Debug Message [" << id << "]: " << message << "\n";
  std::cout << "Severity: " << severityStr << "\n";
  std::cout << "Type: " << typeStr << "\n";
  std::cout << "Source: " << sourceStr << "\n" << std::endl;
  // std::cout << "ID: " << id << "\n";
}

static void HelpMarker(const char *desc)
{
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip())
  {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}
namespace DebugGui
{
void init()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiStyle &style = ImGui::GetStyle();
  ImGuiIO &io = ImGui::GetIO();

  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
  io.ConfigFlags &=
      ~ImGuiConfigFlags_NavEnableKeyboard; // Disable keyboard navigation

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(
      Core::get_window(),
      true); // Second param install_callback=true will install
             // GLFW callbacks and chain to existing ones.
  ImGui_ImplOpenGL3_Init();

  io.Fonts->AddFontFromFileTTF(ASSETS_PATH "Karla-Regular.ttf", 22.0f);
  io.Fonts->Build();

  style.FrameRounding = 5;

  // logging
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(message_callback, nullptr);
}

static const ImVec4 green = ImColor(119, 185, 0);
void draw(Camera *camera)
{

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Debug");
  ImGui::Text("FPS: %d", Core::get_fps());

  // Camera *camera = World::get_camera_no_const();
  ImGui::Text("Position: %.2f, %.2f, %.2f", camera->position.x,
              camera->position.y, camera->position.z);

  // ImGui::PushStyleColor(ImGuiCol_Text, green);

  labeledFloatManager.update(Core::get_deltatime());
  labeledFloatManager.render();
  // ImGui::PopStyleColor();

  // fps control
  static int targetFps = Core::get_targetfps();
  ImGui::SliderInt("Target fps", &targetFps, 30, 500);

  targetFps = std::max(targetFps, 30); // when manually entering value in imgui,
                                       // can cause low fps mid typing
  Core::set_targetfps(targetFps);

  // camera controls
  static bool scrollControl = true;

  // directly modify flySpeed
  float &flySpeed = camera->flySpeed;
  constexpr float flyMin = 0.5f;
  constexpr float flyMax = 125.0f;

  flySpeed = std::clamp(flySpeed, flyMin, flyMax);

  ImGui::SliderFloat("Camera fly speed", &flySpeed, flyMin, flyMax);
  ImGui::Checkbox("Scroll speed control", &scrollControl);

  if (scrollControl && Core::Input::get_scrolldelta() != 0)
    flySpeed += 1.0f * Core::Input::get_scrolldelta();

  // wireframe
  ImGui::Checkbox("Wireframe", &Render::is_wireframe());

  // face cull
  static bool backfaceCull = true;
  ImGui::Checkbox("Backface culling", &backfaceCull);
  if (backfaceCull)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  ImGui::SeparatorText("SSAO");
  ImGui::Checkbox("Enable SSAO", &Render::ssaoUniforms.enableSSAO);

  ImGui::BeginDisabled(!Render::ssaoUniforms.enableSSAO);
  {
    // ssao backing resolution
    static int currentIndex = 2;
    bool changed =
        ImGui::Combo("Resolution", &currentIndex, RESOLUTION_STRINGS.data(),
                     RESOLUTION_STRINGS.size());
    if (changed)
    {
      Resolution r = int_to_Resolution(currentIndex);
      Render::set_ssao_resolution(resolution_to_vec2(r));
    }
    static int sampleStep = 1;

    int &samples = Render::ssaoUniforms.samples;
    float &radius = Render::ssaoUniforms.radius;
    float &bias = Render::ssaoUniforms.bias;

    ImGui::InputScalar("Samples", ImGuiDataType_S32, &samples, &sampleStep);
    ImGui::SliderFloat("Radius", &radius, 0.1, 10);

    ImGui::SliderFloat("Bias", &bias, 0, 0.1);

    ImGui::SliderFloat("Power", &Render::ssaoUniforms.power, 1, 5);

    ImGui::SameLine();
    HelpMarker("Raises ssao by power");

    samples = std::clamp(samples, 4, 64);
  }
  ImGui::EndDisabled();

  ImGui::SeparatorText("HDR");
  ImGui::SliderFloat("Exposure", &Render::get_hdr_exposure(), 0.1, 5);
  ImGui::SliderFloat("Gamma", &Render::get_gamma(), 1, 3);

  ImGui::End();

  // ImGui::ShowDemoWindow();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void destroy()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
} // namespace DebugGui
