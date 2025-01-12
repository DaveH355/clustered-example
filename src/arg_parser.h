#pragma once
#include "render_manager.h"
#include <argparse.hpp>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

namespace ArgParser
{
using namespace argparse;

// return success bool
inline bool parse(int argc, char *argv[])
{
  ArgumentParser program("opengl-playground");

  program.add_argument("--quality")
      .default_value(std::string("low"))    //
      .choices("low", "high")               //
      .help("Set graphics quality preset"); //

  try
  {
    program.parse_args(argc, argv);
  }
  catch (const std::exception &err)
  {
    std::cerr << err.what() << std::endl;
    std::cerr << program; // print help message
    return false;
  }

  auto quality = program.get<std::string>("--quality");

  constexpr float DEFAULT_SSAO_RADIUS = 1.45f;
  constexpr float DEFAULT_SSAO_BIAS = 0.055f;
  constexpr float DEFAULT_SSAO_POWER = 2.5f;
  using Render::ssaoUniforms;

  if (quality == "high")
  {
    ssaoUniforms.enableSSAO = true;
    ssaoUniforms.bias = DEFAULT_SSAO_BIAS;
    ssaoUniforms.radius = DEFAULT_SSAO_RADIUS;
    ssaoUniforms.samples = 48;
    ssaoUniforms.power = DEFAULT_SSAO_POWER;

    Render::set_ssao_resolution(resolution_to_vec2(Resolution::_1280_720),
                                false);
    // gbuffer resolution
    Render::set_gbuffer_resolution(resolution_to_vec2(Resolution::_1920_1080),
                                   false);
    printf("settings high\n");
  }
  else if (quality == "low")
  {
    printf("settings low\n");

    ssaoUniforms.enableSSAO = false;
    ssaoUniforms.bias = DEFAULT_SSAO_BIAS;
    ssaoUniforms.radius = DEFAULT_SSAO_RADIUS;
    ssaoUniforms.samples = 48;
    ssaoUniforms.power = DEFAULT_SSAO_POWER;

    Render::set_ssao_resolution(resolution_to_vec2(Resolution::_960_540),
                                false);
    // gbuffer resolution
    Render::set_gbuffer_resolution(resolution_to_vec2(Resolution::_1920_1080),
                                   false);
  }

  return true;
}
} // namespace ArgParser
