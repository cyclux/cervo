#ifndef PWMCONTROLLER_HPP
#define PWMCONTROLLER_HPP

#include "../third_party/libgpiod-2.1.3/bindings/cxx/gpiod.hpp" // Include C++ bindings for GPIO with libgpiod
#include <filesystem>
#include <memory> // For std::shared_ptr
#include <optional>
#include <string>
#include <vector>

// Constants for the PWM configuration
constexpr int PULSE_WIDTH_MIN = 800;
constexpr int PULSE_WIDTH_MAX = 2200;
constexpr int PULSE_WIDTH_NEUTRAL = 1500;
constexpr int PERIOD = 20000; // 20ms (50Hz)
constexpr int CHANNEL = 2;    // 2 and 3 are valid on pi 5
constexpr const char *CHIP = "pwmchip2";

// Quadratic easing function for smoother servo control
int easeInOutQuad(int t, int b, int c, int d);

class PWMController {
public:
  PWMController(int channel = CHANNEL, const std::string &chip = CHIP);
  ~PWMController(); // Destructor to clean up resources

  void init(const std::vector<int> &channels);

  // Methods to interact with individual PWM channels
  void setDutyCycle(int dutyCycle);
  void enable();
  void disable();

  // Smoothing control method
  void smoothMove(int targetDutyCycle, int duration);

private:
  int channel;
  std::string chip_name;
  std::shared_ptr<gpiod::chip> chip;
  std::optional<gpiod::line_request> line_request;

  void initGPIO();
};

#endif // PWMCONTROLLER_HPP
