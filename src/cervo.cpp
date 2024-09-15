#include "cervo.hpp"
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

// Helper method to construct file paths for PWM (with channel)
std::string PWMController::getPWMFilePath(const std::string &chip, int channel,
                                          const std::string &property) const {
  return "/sys/class/pwm/" + chip + "/pwm" + std::to_string(channel) + "/" +
         property;
}

// Helper method to construct file paths for PWM (without channel)
std::string PWMController::getPWMFilePath(const std::string &chip,
                                          const std::string &property) const {
  return "/sys/class/pwm/" + chip + "/" + property;
}

// Helper to open a file descriptor
int PWMController::openFileDescriptor(const std::string &filePath) {
  int fd = open(filePath.c_str(), O_WRONLY);
  if (fd == -1) {
    std::cerr << "Unable to open file: " << filePath << std::endl;
  }
  return fd;
}

int easeInOutQuad(float t, int b, int c, int d) {
  t /= (float)d / 2.0f;
  if (t < 1)
    return (c / 2.0f) * t * t + b;
  t--;
  return -(c / 2.0f) * (t * (t - 2) - 1) + b;
}

PWMController::PWMController(const std::string &chip)
    : chip(chip), currentDutyCyclePan(PWMConfig::PULSE_WIDTH_NEUTRAL),
      currentDutyCycleTilt(PWMConfig::PULSE_WIDTH_NEUTRAL) {}

PWMController::~PWMController() {
  for (auto &[channel, fds] : channel_fds) {
    if (fds.export_fd != -1)
      close(fds.export_fd);
    if (fds.period_fd != -1)
      close(fds.period_fd);
    if (fds.duty_cycle_fd != -1)
      close(fds.duty_cycle_fd);
    if (fds.enable_fd != -1)
      close(fds.enable_fd);
  }
}

void PWMController::init(const std::vector<int> &channels) {
  for (const int channel : channels) {
    initializeChannel(channel);
    openChannelFileDescriptors(channel);
    configureChannel(channel);
  }
  std::cout << "PWMController initialization complete." << std::endl;
}

void PWMController::initializeChannel(int channel) {
  std::cout << "Exporting channel: " << channel << std::endl;
  exportChannel(channel);
  std::cout << "Waiting for export to be ready for channel: " << channel
            << std::endl;

  while (!isExportReady(channel)) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(PWMConfig::EXPORT_READY_DELAY));
    std::cout << "Export not ready, retrying..." << std::endl;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(PWMConfig::INIT_DELAY));
}

void PWMController::openChannelFileDescriptors(int channel) {
  ChannelFDs &fds =
      channel_fds[channel]; // Directly access ChannelFDs in the map

  fds.period_fd = openFileDescriptor(getPWMFilePath(chip, channel, "period"));
  fds.duty_cycle_fd =
      openFileDescriptor(getPWMFilePath(chip, channel, "duty_cycle"));
  fds.enable_fd = openFileDescriptor(getPWMFilePath(chip, channel, "enable"));

  if (fds.period_fd == -1 || fds.duty_cycle_fd == -1 || fds.enable_fd == -1) {
    std::cerr << "Unable to open one or more file descriptors for channel "
              << channel << std::endl;
  }
}

void PWMController::configureChannel(int channel) {
  std::cout << "Setting period for channel: " << channel << std::endl;
  setPeriod(channel, PWMConfig::PERIOD);

  std::cout << "Setting duty cycle to neutral for channel: " << channel
            << std::endl;
  setDutyCycle(channel, PWMConfig::PULSE_WIDTH_NEUTRAL);

  std::cout << "Enabling PWM for channel: " << channel << std::endl;
  enable(channel);
}

void PWMController::exportChannel(const int channel) {
  int export_fd = openFileDescriptor(getPWMFilePath(chip, "export"));
  if (export_fd != -1) {
    writeToFile(export_fd, std::to_string(channel));
    close(export_fd);
  }
}

bool PWMController::isExportReady(const int channel) const {
  const std::string pwm_path =
      "/sys/class/pwm/" + chip + "/pwm" + std::to_string(channel);
  return std::filesystem::exists(pwm_path + "/period") &&
         std::filesystem::exists(pwm_path + "/duty_cycle") &&
         std::filesystem::exists(pwm_path + "/enable");
}

void PWMController::setPWMValue(const int channel, const std::string &property,
                                const std::string &value) {
  std::lock_guard<std::mutex> lock(
      channel_fds[channel].mutex); // Lock per-channel mutex
  int fd = openFileDescriptor(getPWMFilePath(chip, channel, property));
  if (fd != -1) {
    writeToFile(fd, value);
    close(fd);
  }
}

void PWMController::setPeriod(const int channel, const int period) {
  setPWMValue(channel, "period", std::to_string(period));
}

void PWMController::setDutyCycle(const int channel, int dutyCycle) {
  dutyCycle = std::clamp(dutyCycle, PWMConfig::PULSE_WIDTH_MIN,
                         PWMConfig::PULSE_WIDTH_MAX);
  const int convertedDutyCycle = convertDutyCycle(dutyCycle);
  setPWMValue(channel, "duty_cycle", std::to_string(convertedDutyCycle));
}

void PWMController::enable(const int channel) {
  setPWMValue(channel, "enable", "1");
}

void PWMController::disable(const int channel) {
  setPWMValue(channel, "enable", "0");
}

int PWMController::convertDutyCycle(const int dutyCycle) const {
  return dutyCycle * 1000;
}

void PWMController::writeToFile(int fd, const std::string &value) const {
  if (pwrite(fd, value.c_str(), value.size(), 0) == -1) {
    std::cerr << "Error writing to file descriptor " << fd << std::endl;
  }
}

void PWMController::smoothMove(int channel, int targetDutyCycle, int duration) {
  int *currentDutyCyclePtr = nullptr;
  if (channel == 2) {
    currentDutyCyclePtr = &currentDutyCyclePan;
  } else if (channel == 3) {
    currentDutyCyclePtr = &currentDutyCycleTilt;
  } else {
    std::cerr << "Invalid channel: " << channel << std::endl;
    return;
  }

  int &currentDutyCycle = *currentDutyCyclePtr;
  const int stepDuration = 20; // PWM period (20ms)
  const int totalSteps = duration / stepDuration;

  for (int step = 0; step <= totalSteps; ++step) {
    float t = step / static_cast<float>(totalSteps); // Normalize step to range from 0 to 1
    int newDutyCycle = easeInOutQuad(t, currentDutyCycle,
                                     targetDutyCycle - currentDutyCycle, 1);
    setDutyCycle(channel, newDutyCycle);
    std::this_thread::sleep_for(std::chrono::milliseconds(stepDuration));
  }

  currentDutyCycle = targetDutyCycle;
}

void PWMController::smoothMoveThread(int targetDutyCyclePan,
                                     int targetDutyCycleTilt, int duration) {
  std::thread panThread(&PWMController::smoothMove, this, 2, targetDutyCyclePan,
                        duration);
  std::thread tiltThread(&PWMController::smoothMove, this, 3,
                         targetDutyCycleTilt, duration);

  panThread.join();
  tiltThread.join();
}

void PWMController::moveThread(int dutyCyclePan, int dutyCycleTilt) {
  std::thread panThread(&PWMController::setDutyCycle, this, 2, dutyCyclePan);
  std::thread tiltThread(&PWMController::setDutyCycle, this, 3, dutyCycleTilt);

  panThread.join();
  tiltThread.join();

  currentDutyCyclePan = dutyCyclePan;
  currentDutyCycleTilt = dutyCycleTilt;
}
