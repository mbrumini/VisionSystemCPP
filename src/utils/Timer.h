#pragma once

#include <chrono>

class Timer
{
public:
  using Clock = std::chrono::steady_clock;

  Timer()
  {
    reset();
  }

  void reset()
  {
    m_start = Clock::now();
  }

  double elapsedMilliseconds() const
  {
    const auto elapsed = Clock::now() - m_start;
    return std::chrono::duration<double, std::milli>(elapsed).count();
  }

private:
  Clock::time_point m_start;
};
