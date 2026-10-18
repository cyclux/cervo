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

  program.add_argument("--fastmove")
      .help("Enable fast move to position without smoothing")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--function")
      .help("Set the movement function (e.g., easeInOutQuad, linear)")
      .default_value(std::string("easeInOutQuad"));

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  const std::string CHIP = PWMController::DEFAULT_PWM_DEVICE;

  PWMController servo(CHIP);
  servo.init({PWMController::CHANNEL_PAN, PWMController::CHANNEL_TILT});

  int targetDutyCyclePan = program.get<int>("--pos");
  int targetDutyCycleTilt = program.get<int>("--pos");

  int speed = program.get<int>("--speed");
  int stepdist = program.get<int>("--stepdist");
  std::string movementFunction = program.get<std::string>("--function");

  if (program.is_used("--pos")) {
    std::cout << "Current pos - pan: "
              << servo.getCurrentDutyCycle(PWMController::CHANNEL_PAN)
              << ", tilt: "
              << servo.getCurrentDutyCycle(PWMController::CHANNEL_TILT)
              << std::endl;

    if (program.get<bool>("--fastmove")) {
      // Immediate movement
      servo.setDutyCycle(PWMController::CHANNEL_PAN, targetDutyCyclePan);
      servo.setDutyCycle(PWMController::CHANNEL_TILT, targetDutyCycleTilt);
    } else {
      // Smooth movement using setPosition
      servo.setPosition(targetDutyCyclePan, targetDutyCycleTilt, speed,
                        movementFunction);
    }
  }

  if (program.get<bool>("--keycontrol")) {
    handleKeyPresses(servo, speed, stepdist, movementFunction);
  }

  // Move back to neutral position
  servo.setPosition(PWMConfig::PULSE_WIDTH_NEUTRAL,
                    PWMConfig::PULSE_WIDTH_NEUTRAL, speed, movementFunction);

  // Wait for movements to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(speed + 100));

  servo.disable(PWMController::CHANNEL_PAN);
  servo.disable(PWMController::CHANNEL_TILT);

  servo.stopControlLoop();

  return 0;
}
