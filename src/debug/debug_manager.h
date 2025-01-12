#pragma once

#include "camera.h"
#include "imgui.h"
#include <string>
#include <unordered_map>
#include <utility>

class LabeledFloat
{
public:
  LabeledFloat(std::string label, float updateDelaySecs)
      : label(std::move(label)), updateDelay(updateDelaySecs)
  {
  }

  void setValue(float value) { currentValue = value; }

  void update(float deltaTime)
  {
    accum += deltaTime;
    if (accum >= updateDelay)
    {
      displayValue = currentValue;
      accum = 0.0f;
    }
  }

  void render() { ImGui::Text("%s: %.4f", label.c_str(), displayValue); }

private:
  std::string label;
  float updateDelay;
  float accum = 0;
  float displayValue = 0;
  float currentValue = 0;
};

class LabeledFloatManager
{
public:
  void setValue(const std::string &label, float value,
                float updateDelaySecs = 0.25f)
  {
    if (!map.contains(label))
    {
      map.emplace(label, LabeledFloat(label, updateDelaySecs));
    }
    map.at(label).setValue(value);
  }

  void update(float deltaTime)
  {
    for (auto &[label, staggeredFloat] : map)
    {
      staggeredFloat.update(deltaTime);
    }
  }

  void render()
  {
    for (auto &[label, staggeredFloat] : map)
    {
      staggeredFloat.render();
    }
  }

private:
  std::unordered_map<std::string, LabeledFloat> map;
};

namespace DebugGui
{
void init();

void draw(Camera *camera);

void destroy();

inline LabeledFloatManager labeledFloatManager;


} // namespace DebugGui
