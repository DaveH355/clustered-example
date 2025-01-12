#pragma once

#include "core/core.h"
#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

enum class MoveType
{
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT,
  UP,
  DOWN,

  PITCH_UP,
  PITCH_DOWN,
  PITCH_LEFT,
  PITCH_RIGHT,
};

static const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

class Camera
{
private:
  float yaw = -90.0f;
  float pitch;

public:
  float fov = 55;
  float near = 0.1f;
  float far = 400.0f;

  float lookSensX = 0.09f;
  float lookSensY = 0.07f;

  float flySpeed = 10.0f;

  glm::vec3 position{};
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 direction{1, 0, 0};

  glm::mat4 view;
  glm::mat4 projection;

  Camera() : position(0.0f) {}

  void update_matrixes(int width, int height)
  {

    view = glm::mat4(1.0f);
    view = glm::lookAt(position, position + direction, up);

    projection =
        glm::perspective(glm::radians(fov), width / (float)height, near, far);
  }

  void offset_yaw_pitch(float yawOffset, float pitchOffset)
  {
    yaw += yawOffset;
    pitch += pitchOffset;

    if (pitch > 89.0f)
      pitch = 89.0f;
    if (pitch < -89.0f)
      pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    this->direction = glm::normalize(direction);
  }

  void process_keyboard(MoveType moveType)
  {
    float deltaTime = Core::get_deltatime();
    float speed = flySpeed * deltaTime;

    glm::vec3 moveDir(direction);
    moveDir.y = 0;
    moveDir = glm::normalize(moveDir); // re-normalize  after changing y to 0

    if (moveType == MoveType::FORWARD)
    {
      position += speed * moveDir;
    }
    if (moveType == MoveType::LEFT)
    {
      position -= speed * glm::normalize(glm::cross(moveDir, worldUp));
    }
    if (moveType == MoveType::BACKWARD)
    {
      position -= speed * moveDir;
    }
    if (moveType == MoveType::RIGHT)
    {
      position += speed * glm::normalize(glm::cross(moveDir, worldUp));
    }
    if (moveType == MoveType::UP)
    {
      position.y += speed;
    }
    if (moveType == MoveType::DOWN)
    {
      position.y -= speed;
    }
    // pitch
    double offsetX = 0;
    double offsetY = 0;
    if (moveType == MoveType::PITCH_UP)
    {
      offsetY += 1;
    }
    if (moveType == MoveType::PITCH_DOWN)
    {
      offsetY -= 1;
    }
    if (moveType == MoveType::PITCH_LEFT)
    {
      offsetX -= 1;
    }
    if (moveType == MoveType::PITCH_RIGHT)
    {
      offsetX += 1;
    }
    offsetY *= deltaTime * 50;
    offsetX *= deltaTime * 60;

    if (offsetX != 0 || offsetY != 0)
    {
      offset_yaw_pitch(offsetX, offsetY);
    }
  }

  void update_mouse()
  {

    auto [mouseDeltaX, mouseDeltaY] = Core::Input::get_mouse_pos_delta();

    float xoffset = lookSensX * mouseDeltaX;
    float yoffset = lookSensY * mouseDeltaY;

    offset_yaw_pitch(xoffset, yoffset);
  }
};
