#include "cervo.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

PWMController::PWMController(const std::string &chip)
    : chip(resolveChip(chip)) {
  initializeMovementFunctions();
}

// Returns the pwmchipN name for a pwmchipN name or a PWM device name.
std::string PWMController::resolveChip(const std::string &chipOrDevice) {
  if (chipOrDevice.rfind("pwmchip", 0) == 0) {
    return chipOrDevice;
  }
  for (const auto &entry :
       std::filesystem::directory_iterator("/sys/class/pwm")) {
    std::error_code ec;
    const auto device = std::filesystem::read_symlink(entry.path() / "device", ec);
    if (!ec && device.filename() == chipOrDevice) {
      return entry.path().filename().string();
    }
  }
  throw std::runtime_error("No PWM chip found for device " + chipOrDevice);
}

PWMController::~PWMController() {
  stopControlLoop();
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
    currentDutyCycles[channel] = PWMConfig::PULSE_WIDTH_NEUTRAL;
  }
  std::cout << "PWMController initialization complete." << std::endl;
  startControlLoop();
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

void PWMController::setPeriod(const int channel, const int period) {
  ChannelFDs &fds = channel_fds[channel];
  std::lock_guard<std::mutex> lock(fds.mutex);
  writeToFile(fds.period_fd, std::to_string(period));
}

void PWMController::setDutyCycle(const int channel, int dutyCycle) {
  dutyCycle = std::clamp(dutyCycle, PWMConfig::PULSE_WIDTH_MIN,
                         PWMConfig::PULSE_WIDTH_MAX);
  const int convertedDutyCycle = convertDutyCycle(dutyCycle);

  ChannelFDs &fds = channel_fds[channel];
  std::lock_guard<std::mutex> lock(fds.mutex);
  writeToFile(fds.duty_cycle_fd, std::to_string(convertedDutyCycle));

  currentDutyCycles[channel] = dutyCycle;
}

void PWMController::enable(const int channel) {
  ChannelFDs &fds = channel_fds[channel];
  std::lock_guard<std::mutex> lock(fds.mutex);
  writeToFile(fds.enable_fd, "1");
}

void PWMController::disable(const int channel) {
  ChannelFDs &fds = channel_fds[channel];
  std::lock_guard<std::mutex> lock(fds.mutex);
  writeToFile(fds.enable_fd, "0");
}

int PWMController::convertDutyCycle(const int dutyCycle) const {
  return dutyCycle * 1000;
}

void PWMController::writeToFile(int fd, const std::string &value) const {
  if (pwrite(fd, value.c_str(), value.size(), 0) == -1) {
    std::cerr << "Error writing to file descriptor " << fd << ": "
              << strerror(errno) << std::endl;
  }
}

int PWMController::getCurrentDutyCycle(int channel) const {
  auto it = currentDutyCycles.find(channel);
  if (it != currentDutyCycles.end()) {
    return it->second;
  } else {
    return PWMConfig::PULSE_WIDTH_NEUTRAL;
  }
}

void PWMController::initializeMovementFunctions() {
  movementFunctions["easeInOutQuad"] = easeInOutQuad;
  movementFunctions["linear"] = linear;
  // Add more movement functions here if needed
}

void PWMController::setPosition(int targetPan, int targetTilt, int duration,
                                const std::string &movementFunctionName) {
  setPositionPanTilt(targetPan, targetTilt, duration, movementFunctionName);
}

void PWMController::setPositionPanTilt(
    int targetPan, int targetTilt, int duration,
    const std::string &movementFunctionName) {
  auto it = movementFunctions.find(movementFunctionName);
  if (it == movementFunctions.end()) {
    std::cerr << "Movement function not found: " << movementFunctionName
              << std::endl;
    return;
  }
  auto movementFunction = it->second;

  // Pan Command
  MovementCommand cmdPan;
  cmdPan.channel = CHANNEL_PAN;
  cmdPan.startDutyCycle = getCurrentDutyCycle(CHANNEL_PAN);
  cmdPan.targetDutyCycle = std::clamp(targetPan, PWMConfig::PULSE_WIDTH_MIN,
                                      PWMConfig::PULSE_WIDTH_MAX);
  cmdPan.duration = duration;
  cmdPan.movementFunction = movementFunction;
  addMovementCommand(cmdPan);

  // Tilt Command
  MovementCommand cmdTilt;
  cmdTilt.channel = CHANNEL_TILT;
  cmdTilt.startDutyCycle = getCurrentDutyCycle(CHANNEL_TILT);
  cmdTilt.targetDutyCycle = std::clamp(targetTilt, PWMConfig::PULSE_WIDTH_MIN,
                                       PWMConfig::PULSE_WIDTH_MAX);
  cmdTilt.duration = duration;
  cmdTilt.movementFunction = movementFunction;
  addMovementCommand(cmdTilt);
}

void PWMController::addMovementCommand(const MovementCommand &cmd) {
  std::lock_guard<std::mutex> lock(queueMutex);
  commandQueue.push(cmd);
}

void PWMController::startControlLoop() {
  running = true;
  controlLoopThread = std::thread(&PWMController::controlLoop, this);
}

void PWMController::stopControlLoop() {
  running = false;
  if (controlLoopThread.joinable()) {
    controlLoopThread.join();
  }
}

void PWMController::controlLoop() {
  const int updateIntervalMs = 20; // 50Hz
  std::vector<MovementCommand> activeCommands;

  while (running) {
    auto loopStartTime = std::chrono::steady_clock::now();

    // Transfer commands from queue to activeCommands
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      while (!commandQueue.empty()) {
        MovementCommand cmd = commandQueue.front();
        commandQueue.pop();
        cmd.startTime = std::chrono::steady_clock::now();
        activeCommands.push_back(cmd);
      }
    }

    // Process active commands
    for (auto it = activeCommands.begin(); it != activeCommands.end();) {
      MovementCommand &cmd = *it;
      auto now = std::chrono::steady_clock::now();
      int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now - cmd.startTime)
                          .count();

      if (elapsedMs <= cmd.duration) {
        float t = static_cast<float>(elapsedMs) / cmd.duration;
        int change = cmd.targetDutyCycle - cmd.startDutyCycle;
        int newDutyCycle = cmd.movementFunction(t, cmd.startDutyCycle, change);
        setDutyCycle(cmd.channel, newDutyCycle);
        ++it;
      } else {
        // Movement complete, set final position
        setDutyCycle(cmd.channel, cmd.targetDutyCycle);
        it = activeCommands.erase(it); // Remove completed command
      }
    }

    // Sleep until next update
    auto loopEndTime = std::chrono::steady_clock::now();
    int loopDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                             loopEndTime - loopStartTime)
                             .count();
    int sleepTimeMs = updateIntervalMs - loopDurationMs;
    if (sleepTimeMs > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
    } else {
      // Loop is taking longer than expected
      // Optionally handle this case
    }
  }
}

int linear(float t, int b, int c) { return b + c * t; }

// Movement functions
int easeInOutQuad(float t, int b, int c) {
  t /= 0.5f;
  if (t < 1)
    return c / 2 * t * t + b;
  t -= 1;
  return -c / 2 * (t * (t - 2) - 1) + b;
}

// Helper methods for file paths and descriptors
std::string PWMController::getPWMFilePath(const std::string &chip, int channel,
                                          const std::string &property) const {
  return "/sys/class/pwm/" + chip + "/pwm" + std::to_string(channel) + "/" +
         property;
}

std::string PWMController::getPWMFilePath(const std::string &chip,
                                          const std::string &property) const {
  return "/sys/class/pwm/" + chip + "/" + property;
}

int PWMController::openFileDescriptor(const std::string &filePath) {
  int fd = open(filePath.c_str(), O_WRONLY);
  if (fd == -1) {
    std::cerr << "Unable to open file: " << filePath << std::endl;
  }
  return fd;
}
