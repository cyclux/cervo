#include "cervo.hpp"
#include "key_input_handler.hpp"
#include "third_party/argparse.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
  argparse::ArgumentParser program("cervo");

  program.add_argument("--keycontrol")
      .help("Enable key control mode")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--pos")
      .help("Set the initial position of the PWM duty cycle")
      .scan<'i', int>()
      .default_value(PWMConfig::PULSE_WIDTH_NEUTRAL);

  program.add_argument("--speed")
      .help("Set the time it takes to smoothly move the servo in ms")
      .scan<'i', int>()
      .default_value(PWMConfig::SPEED);

  program.add_argument("--stepdist")
      .help("Set the distance for each step")
      .scan<'i', int>()
      .default_value(PWMConfig::STEP_DIST);

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  const std::string CHIP = "pwmchip2"; // Example value

  PWMController servo(CHIP);
  servo.init({2, 3});

  int currentDutyCyclePan = PWMConfig::PULSE_WIDTH_NEUTRAL;
  int currentDutyCycleTilt = PWMConfig::PULSE_WIDTH_NEUTRAL;
  int targetDutyCyclePan = PWMConfig::PULSE_WIDTH_NEUTRAL;
  int targetDutyCycleTilt = PWMConfig::PULSE_WIDTH_NEUTRAL;

  int speed = program.get<int>("--speed");
  int stepdist = program.get<int>("--stepdist");

  if (program.is_used("--pos")) {
    targetDutyCyclePan = program.get<int>("--pos");
    targetDutyCycleTilt = program.get<int>("--pos");

    std::cout << "Current pos - pan: " << currentDutyCyclePan
              << ", tilt: " << currentDutyCycleTilt << std::endl;
    servo.smoothMove(2, currentDutyCyclePan, targetDutyCyclePan, speed);
    servo.smoothMove(3, currentDutyCycleTilt, targetDutyCycleTilt, speed);
  }

  if (program.is_used("--keycontrol")) {
    handleKeyPresses(servo, speed, stepdist, currentDutyCyclePan,
                     currentDutyCycleTilt);
  }

  servo.smoothMove(2, currentDutyCyclePan, PWMConfig::PULSE_WIDTH_NEUTRAL,
                   speed);
  servo.smoothMove(3, currentDutyCycleTilt, PWMConfig::PULSE_WIDTH_NEUTRAL,
                   speed);
  servo.disable(2);
  servo.disable(3);

  return 0;
}
