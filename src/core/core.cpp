#include "core.h"
#include <gldoc.hpp>
#include <GLFW/glfw3.h>
#include <array>

#include <chrono>
#include <iostream>
#include <thread>
#include <tuple>

struct CoreData
{
  // WINDOW
  GLFWwindow *window;
  int initial_width, initial_height;

  int lastWindowX; // last windowed values before fullscreen
  int lastWindowY;
  int lastWindowWidth;
  int lastWindowHeight;

  bool isWindowResized;
  bool isFullScreen;
  float deltaTime; // seconds
  float frameStartTime;
  float lastFrameTime;

  int targetFps;
  int fps; 
  int liveFPS;
};
static CoreData core{};

// forward declares
//=========
namespace Core
{
namespace GL
{
void init();
}
} // namespace Core
//==========

namespace Core
{

namespace Input
{
const int MAX_KEYS = 512;
std::array<bool, MAX_KEYS> currentKeyState;
std::array<bool, MAX_KEYS> previousKeyState;

// copy key states to previous at end of every frame
static void copy_keystates_to_previous() { previousKeyState = currentKeyState; }

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods)
{
  if (key >= 0 && key < MAX_KEYS)
  {
    if (action == GLFW_PRESS)
      currentKeyState[key] = true;
    else if (action == GLFW_RELEASE)
      currentKeyState[key] = false;
  }
}

bool iskey_pressed(int key)
{
  return currentKeyState.at(key) && !previousKeyState.at(key);
}

bool iskey_released(int key)
{
  return !currentKeyState.at(key) && previousKeyState.at(key);
}

bool iskey_down(int key) { return currentKeyState.at(key); }

bool iskey_combo_pressed(int key, int mod)
{
  return iskey_pressed(key) && currentKeyState.at(mod);
}

double scrollYDelta;

double lastMouseX;
double lastMouseY;
double mouseDeltaX, mouseDeltaY;
double smoothedMouseDeltaX, smoothedMouseDeltaY;

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos)
{
  static bool firstMouse = true;
  if (firstMouse)
  {
    lastMouseX = xpos;
    lastMouseY = ypos;
    firstMouse = false;
  }

  mouseDeltaX += xpos - lastMouseX;
  mouseDeltaY += (ypos - lastMouseY) * -1; // invert the y-axis
  // TODO: let Input namesapce handle inverting y axis as an option?

  lastMouseX = xpos;
  lastMouseY = ypos;
}

std::tuple<double, double> get_mouse_pos_delta()
{
  return std::make_tuple(mouseDeltaX, mouseDeltaY);
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
  scrollYDelta = yoffset;
}
// acts as an indicator for if any scroll. Returns exactly 0 for no scroll. It
//  should be explicity reset to 0 every frame.
// TODO: store x delta as well?
float get_scrolldelta() { return scrollYDelta; }

static void center_cursor()
{
  int width, height;
  glfwGetWindowSize(core.window, &width, &height);
  glfwSetCursorPos(core.window, width / 2.0f, height / 2.0f);
}
void toggle_mouse()
{
  if (glfwGetInputMode(core.window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL)
  {
    // Disable the cursor
    glfwSetInputMode(core.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }
  else
  {
    // Enable the cursor
    glfwSetInputMode(core.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    center_cursor();
  }
}

bool iscursor_disabled()
{
  return glfwGetInputMode(core.window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

} // namespace Input

static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
  core.isWindowResized = true;
  glViewport(0, 0, width, height);
}
static void query_opengl_implementation_maximums()
{
  int value[3];

  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value[0]);
  std::cout << "\tMax vertex attributes: " << value[0] << "\n";

  glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &value[0]);
  std::cout << "\tMax number of elements indices: " << value[0] << "\n";

  glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value[0]);
  std::cout << "\tMax number of elements vertices: " << value[0] << "\n";

  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &value[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &value[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &value[2]);
  std::cout << "\tMax compute work groups: (" << value[0] << ", "
            << value[1] << ", " << value[2] << ")"
            << "\n";

  glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &value[0]);
  std::cout << "\tMax compute work group invocations: " << value[0]
            << "\n";

  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &value[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &value[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &value[2]);
  std::cout << "\tMax compute work group size: (" << value[0] << ", "
            << value[1] << ", " << value[2] << ")"
            << "\n";

  glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &value[0]);
  std::cout << "\tMax compute shader shared memory: " << value[0] << "\n";

  glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &value[0]);
  std::cout << "\tMax number of SSBOs can used in compute shader: " << value[0]
            << "\n";

  glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value[0]);
  std::cout << "\tMax SSBO size: " << value[0] << "\n";

  glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &value[0]);
  std::cout << "\tMax SSBO bindings: " << value[0] << "\n";

  std::cout << std::endl;
}

void init_window(int width, int height, const char *title)
{

  // print glfw data
  std::cout << glfwGetVersionString() << std::endl;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // glfwWindowHint(GLFW_SAMPLES, 4); // MSAA

  GLFWwindow *window = glfwCreateWindow(width, height, title, nullptr, nullptr);

  if (window == nullptr)
  {
    std::cout << "Failed to create window." << std::endl;
    glfwTerminate();
  }
  // reset window hints now that primary window is made
  glfwDefaultWindowHints();
  glfwMakeContextCurrent(window);

  // glad: load all OpenGL function pointers
  if (gladLoadGL(glfwGetProcAddress) == 0)
  {
    std::cout << "Failed to initialize OpenGL context" << std::endl;
  }

  // glEnable(
  // GL_MULTISAMPLE); // should be on by default, but enable anyway for MSAA

  std::cout << "GPU: " << glGetString(GL_RENDERER) << "\n";
  std::cout << "GL Version: " << glGetString(GL_VERSION) << "\n";
  std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
            << "\n";

  query_opengl_implementation_maximums();

  // set callbacks
  glfwSetKeyCallback(window, Input::key_callback);
  glfwSetScrollCallback(window, Input::scroll_callback);
  glfwSetCursorPosCallback(window, Input::cursor_position_callback);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  core.window = window;
  core.targetFps = glfwGetVideoMode(glfwGetPrimaryMonitor())->refreshRate;
  core.initial_width = width;
  core.initial_height = height;

  glfwSwapInterval(0); // disable vsync
  GL::init();
}

void close_window() { glfwTerminate(); }

void begin_drawing()
{
  float currentTime = glfwGetTime();

  core.deltaTime = currentTime - core.lastFrameTime;
  core.lastFrameTime = currentTime;

  static float accum = 0;
  accum += core.deltaTime;

  if (accum >= 1.0f)
  {
    core.fps = 1.0f / core.deltaTime;
    accum = 0;
  }
  core.liveFPS = 1.0f / core.deltaTime;
}

void end_drawing()
{
  // here at end of frame, reset input states just before
  // glfwPollEvents
  core.isWindowResized = false;
  Input::copy_keystates_to_previous(); // current key states are now previous
  Input::scrollYDelta = 0;
  Input::mouseDeltaY = 0;
  Input::mouseDeltaX = 0;

  glfwPollEvents(); // fire callbacks, updating new states

  double frameEndTime = glfwGetTime();
  double target = 1.0 / (core.targetFps);
  double frameTime = frameEndTime - core.lastFrameTime;
  double remainingTime = target - frameTime;

  if (remainingTime > 0)
  {
    auto sleepTime = std::chrono::duration<double>(remainingTime);

    std::this_thread::sleep_for(sleepTime);
    // busy-wait for the last bit to get more accurate timing
    while (glfwGetTime() < core.lastFrameTime + target)
    {
    }
  }

  glfwSwapBuffers(core.window);
}

void toggle_fullscreen()
{
  GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);
  GLFWwindow *window = core.window;

  if (glfwGetWindowMonitor(window) == nullptr)
  {
    // Switch to fullscreen mode
    glfwGetWindowPos(window, &core.lastWindowX, &core.lastWindowY);
    glfwGetWindowSize(window, &core.lastWindowWidth, &core.lastWindowHeight);
    glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width,
                         mode->height, mode->refreshRate);
    core.isFullScreen = true;
  }
  else
  {
    // Switch back to windowed mode
    glfwSetWindowMonitor(window, nullptr, core.lastWindowX, core.lastWindowY,
                         core.lastWindowWidth, core.lastWindowHeight,
                         GLFW_DONT_CARE);
    core.isFullScreen = false;
  }
}

GLFWwindow *get_window() { return core.window; }
float get_deltatime() { return core.deltaTime; }

// glfw framebuffer size in pixels. Wrapper around glfwGetFramebufferSize
std::tuple<int, int> get_framebuffer_size()
{
  int width, height;
  glfwGetFramebufferSize(core.window, &width, &height);

  return std::make_tuple(width, height);
}

std::tuple<int, int> get_inital_window_dimensions()
{
  return std::make_tuple(core.initial_width, core.initial_height);
}

bool iswindow_resized() { return core.isWindowResized; }

int get_fps() { return core.fps; }
int get_live_fps() { return core.liveFPS; }
int get_targetfps() { return core.targetFps; }
void set_targetfps(int target) { core.targetFps = target; };

} // namespace Core
