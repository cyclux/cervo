#include "main.hpp"
#include "third_party/argparse.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ncurses.h>
#include <string>
#include <thread>
#include <vector>

// https://www.kernel.org/doc/html/latest/driver-api/pwm.html
// https://www.kernel.org/doc/html/v4.13/driver-api/miscellaneous.html#pulse-width-modulation-pwm

PWMController::PWMController(int channel, const std::string &chip)
    : chip(chip), pwm("pwm" + std::to_string(channel)) {}

// Method to initialize the PWM controller
void PWMController::init(const std::vector<int> &channels) {
  std::cout << "Initializing PWMController with channels: ";
  for (const int channel : channels) {
    std::cout << channel << " ";
  }
  std::cout << std::endl;

  for (const int channel : channels) {
    std::cout << "Exporting channel: " << channel << std::endl;
    exportChannel(channel);

    // Wait for the export to be ready
    std::cout << "Waiting for export to be ready for channel: " << channel
              << std::endl;
    while (!isExportReady(channel)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::cout << "Export not ready, retrying..." << std::endl;
    }

    std::cout << "Setting period for channel: " << channel << std::endl;
    setPeriod(channel, PERIOD);

    std::cout << "Setting duty cycle to neutral for channel: " << channel
              << std::endl;
    setDutyCycle(channel, PULSE_WIDTH_NEUTRAL);

    std::cout << "Enabling PWM for channel: " << channel << std::endl;
    enable(channel);
  }

  std::cout << "PWMController initialization complete." << std::endl;
}

// Method to export the channel
void PWMController::exportChannel(const int channel) {
  writeToFile("/sys/class/pwm/" + chip + "/export", std::to_string(channel));
}

// Method to set the PWM period
void PWMController::setPeriod(const int channel, const int period) {
  const std::string pwm = "pwm" + std::to_string(channel);
  writeToFile("/sys/class/pwm/" + chip + "/" + pwm + "/period",
              std::to_string(period));
}

// Method to set the PWM duty cycle
void PWMController::setDutyCycle(const int channel, int dutyCycle) {
  if (dutyCycle > PULSE_WIDTH_MAX) {
    dutyCycle = PULSE_WIDTH_MAX;
  }
  if (dutyCycle < PULSE_WIDTH_MIN) {
    dutyCycle = PULSE_WIDTH_MIN;
  }

  const std::string pwm = "pwm" + std::to_string(channel);

  const int convertedDutyCycle = convertDutyCycle(dutyCycle);
  writeToFile("/sys/class/pwm/" + chip + "/" + pwm + "/duty_cycle",
              std::to_string(convertedDutyCycle));
}

// Method to enable the PWM
void PWMController::enable(const int channel) {
  const std::string pwm = "pwm" + std::to_string(channel);
  writeToFile("/sys/class/pwm/" + chip + "/" + pwm + "/enable", "1");
}

// Method to disable the PWM
void PWMController::disable(const int channel) {
  const std::string pwm = "pwm" + std::to_string(channel);
  writeToFile("/sys/class/pwm/" + chip + "/" + pwm + "/enable", "0");
}

// Method to convert the duty cycle
int PWMController::convertDutyCycle(const int dutyCycle) const {
  return dutyCycle * 1000;
}

// Helper method to check if the export is ready
bool PWMController::isExportReady(const int channel) const {
  const std::string pwm = "pwm" + std::to_string(channel);
  const std::string path = "/sys/class/pwm/" + chip + "/" + pwm;
  return std::filesystem::exists(path);
}

// Helper method to write to a file
void PWMController::writeToFile(const std::string &path,
                                const std::string &value) const {
  std::ofstream file(path);
  if (file.is_open()) {
    file << value;
    file.close();
  } else {
    std::cerr << "Unable to open file: " << path << std::endl;
  }
}

void PWMController::smoothMove(int channel, int &currentDutyCycle,
                               int targetDutyCycle, int duration) {
  const int stepDuration = 15; // PWM period (20ms)
  const int totalSteps = duration / stepDuration;

  if (VERBOSE == true) {
    for (int step = 0; step <= totalSteps; ++step) {
      float t = step / static_cast<float>(
                           totalSteps); // Normalize step to range from 0 to 1
      int newDutyCycle = easeInOutQuad(t, currentDutyCycle,
                                       targetDutyCycle - currentDutyCycle, 1);
      std::cout << "channel:" << channel << " | stepDuration: " << stepDuration
                << " | newDutyCycle:" << newDutyCycle << std::endl;
    }
  }

  for (int step = 0; step <= totalSteps; ++step) {
    float t = step / static_cast<float>(
                         totalSteps); // Normalize step to range from 0 to 1
    int newDutyCycle = easeInOutQuad(t, currentDutyCycle,
                                     targetDutyCycle - currentDutyCycle, 1);
    setDutyCycle(channel, newDutyCycle);
    std::this_thread::sleep_for(std::chrono::milliseconds(stepDuration));
  }

  currentDutyCycle = targetDutyCycle;
}

int easeInOutQuad(float t, int b, int c, int d) {
  // Cast d to float to ensure floating-point division
  t /= (float)d / 2.0f;
  if (t < 1) {
    return (c / 2.0f) * t * t + b;
  }
  t--;
  return -(c / 2.0f) * (t * (t - 2) - 1) + b;
}

void handleKeyPresses(PWMController &servo, const int speed, const int stepdist,
                      int &currentDutyCyclePan, int &currentDutyCycleTilt) {
  // Initialize ncurses to capture key presses
  initscr();

  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0); // Hide the cursor
  int targetDutyCyclePan = currentDutyCyclePan;
  int targetDutyCycleTilt = currentDutyCycleTilt;

  int ch;
  while ((ch = getch()) != 'q') { // Press 'q' to quit
    switch (ch) {
    case KEY_UP:
      targetDutyCycleTilt = currentDutyCycleTilt + stepdist;
      servo.smoothMove(3, currentDutyCycleTilt, targetDutyCycleTilt, speed);
      break;
    case KEY_DOWN:
      targetDutyCycleTilt = currentDutyCycleTilt - stepdist;
      servo.smoothMove(3, currentDutyCycleTilt, targetDutyCycleTilt, speed);
      break;
    case KEY_LEFT:
      targetDutyCyclePan = currentDutyCyclePan + stepdist;
      servo.smoothMove(2, currentDutyCyclePan, targetDutyCyclePan, speed);
      break;
    case KEY_RIGHT:
      targetDutyCyclePan = currentDutyCyclePan - stepdist;
      servo.smoothMove(2, currentDutyCyclePan, targetDutyCyclePan, speed);
      break;
    default:
      break;
    }

    // Clear the screen and print the current duty cycle
    clear();
    mvprintw(0, 0, "Current Duty Cycle | Pan: %d | Tilt :%d",
             currentDutyCyclePan, currentDutyCycleTilt);
    refresh();
  }

  // Cleanup ncurses
  endwin();
}

int main(int argc, char *argv[]) {
  // Argument parsing
  argparse::ArgumentParser program("cervo");

  program.add_argument("--keycontrol")
      .help("Enable key control mode")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--pos")
      .help("Set the initial position of the PWM duty cycle")
      .scan<'i', int>()
      .default_value(PULSE_WIDTH_NEUTRAL);

  program.add_argument("--speed")
      .help("Set the time it takes to smoothly move the servo in ms")
      .scan<'i', int>()
      .default_value(SPEED);

  program.add_argument("--stepdist")
      .help("Set the distance for each step")
      .scan<'i', int>()
      .default_value(STEP_DIST);

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  // PWMController pwmController(CHANNEL, CHIP);
  PWMController servo;
  servo.init({2, 3});

  int currentDutyCyclePan = PULSE_WIDTH_NEUTRAL;
  int currentDutyCycleTilt = PULSE_WIDTH_NEUTRAL;
  int targetDutyCyclePan = PULSE_WIDTH_NEUTRAL;
  int targetDutyCycleTilt = PULSE_WIDTH_NEUTRAL;

  int speed = program.get<int>("--speed");
  int stepdist = program.get<int>("--stepdist");

  // Handle --pos parameter
  if (program.is_used("--pos")) {
    targetDutyCyclePan = program.get<int>("--pos");
    targetDutyCycleTilt = program.get<int>("--pos");

    std::cout << "current pos - pan: " << currentDutyCyclePan
              << ", tilt: " << currentDutyCycleTilt << std::endl;
    servo.smoothMove(2, currentDutyCyclePan, targetDutyCyclePan, speed);
    servo.smoothMove(3, currentDutyCycleTilt, targetDutyCycleTilt, speed);
  }

  // Handle --keycontrol parameter
  if (program.is_used("--keycontrol")) {
    handleKeyPresses(servo, speed, stepdist, currentDutyCyclePan,
                     currentDutyCycleTilt);
  }

  // Reset to default before exit
  servo.smoothMove(2, currentDutyCyclePan, PULSE_WIDTH_NEUTRAL, speed);
  servo.smoothMove(3, currentDutyCycleTilt, PULSE_WIDTH_NEUTRAL, speed);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  servo.disable(2);
  servo.disable(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return 0;
}
