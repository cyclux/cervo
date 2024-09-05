#include "main.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

PWMController::PWMController(int channel, const std::string &chip)
    : channel(channel), chip_name(chip), chip(nullptr) {
  // line_request is left uninitialized here, to be set later in initGPIO()
}

PWMController::~PWMController() {
  if (line_request) {
    line_request->release();
  }

  if (chip) {
    chip->close();
  }
}

void PWMController::init(const std::vector<int> &channels) {
  initGPIO();

  setDutyCycle(PULSE_WIDTH_NEUTRAL);
  enable();
}

void PWMController::initGPIO() {
  try {
    chip = std::make_shared<gpiod::chip>(std::filesystem::path(chip_name));
  } catch (const std::system_error &e) {
    std::cerr << "Failed to open GPIO chip: " << chip_name
              << " with error: " << e.what() << std::endl;
    return;
  }

  gpiod::line_settings line_settings;

  line_settings.set_direction(gpiod::line::direction::OUTPUT);
  line_settings.set_output_value(gpiod::line::value::INACTIVE);

  gpiod::line_config line_config;
  line_config.add_line_settings(gpiod::line::offset(channel), line_settings);

  gpiod::request_builder req_builder = chip->prepare_request();
  req_builder.set_consumer("servo").set_line_config(line_config);

  try {
    line_request = req_builder.do_request();
  } catch (const std::system_error &e) {
    std::cerr << "Failed to request GPIO line as output: " << e.what()
              << std::endl;
    chip->close();
    return;
  }
}

void PWMController::setDutyCycle(int dutyCycle) {
  if (dutyCycle > PULSE_WIDTH_MAX) {
    dutyCycle = PULSE_WIDTH_MAX;
  }
  if (dutyCycle < PULSE_WIDTH_MIN) {
    dutyCycle = PULSE_WIDTH_MIN;
  }

  line_request->set_value(channel,
                          gpiod::line::value::ACTIVE); // Start the pulse
  std::this_thread::sleep_for(std::chrono::microseconds(
      dutyCycle)); // Keep the line high for the duration of the duty cycle

  line_request->set_value(channel,
                          gpiod::line::value::INACTIVE); // End the pulse
  std::this_thread::sleep_for(std::chrono::microseconds(
      PERIOD - dutyCycle)); // Wait for the remainder of the period
}

void PWMController::enable() {
  line_request->set_value(channel,
                          gpiod::line::value::ACTIVE); // Set the GPIO line high
}

void PWMController::disable() {
  line_request->set_value(
      channel,
      gpiod::line::value::INACTIVE); // Set the GPIO line low
}

void PWMController::smoothMove(int targetDutyCycle, int duration) {
  int currentDutyCycle = PULSE_WIDTH_NEUTRAL;
  int stepDuration = duration / 100;

  for (int step = 0; step <= 100; ++step) {
    int newDutyCycle = easeInOutQuad(step, currentDutyCycle,
                                     targetDutyCycle - currentDutyCycle, 100);
    setDutyCycle(newDutyCycle);

    std::this_thread::sleep_for(std::chrono::milliseconds(stepDuration));
  }
}

int easeInOutQuad(int t, int b, int c, int d) {
  t /= d / 2;
  if (t < 1)
    return c / 2 * t * t + b;
  t--;
  return -c / 2 * (t * (t - 2) - 1) + b;
}

int main() {
  std::cout << "PWM Controller Test" << std::endl;

  PWMController servo;
  servo.init({2, 3});
  servo.smoothMove(1800, 3000); // Move to position over 5 seconds

  return 0;
}
