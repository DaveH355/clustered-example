#pragma once
#include "imgui.h"
#include <iostream>
#include <chrono>
#include <ratio>
#include <gldoc.hpp>
// timer to measure code execution time
class Timer
{
  using Clock = std::chrono::steady_clock;
  using TimePoint = std::chrono::time_point<Clock>;
  using Duration = std::chrono::duration<double, std::milli>;

private:
  TimePoint startTime;
  TimePoint endTime;

public:
  void start() { startTime = Clock::now(); }

  void stop_and_print()
  {
    std::cout << "Execution time: " << stop_and_get_time_ms() << " ms\n";
  }

  double stop_and_get_time_ms()
  {
    endTime = Clock::now();
    Duration elapsed = endTime - startTime;
    return elapsed.count();
  }
};
