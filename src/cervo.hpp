#ifndef PWMCONTROLLER_HPP
#define PWMCONTROLLER_HPP

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Configuration constants for the PWM controller
struct PWMConfig {
  static constexpr int PULSE_WIDTH_MIN = 800;
  static constexpr int PULSE_WIDTH_MAX = 2000;
  static constexpr int PULSE_WIDTH_NEUTRAL = 1500;
  static constexpr int STEP_DIST = 500;
  static constexpr int SPEED = 750;
  static constexpr int PERIOD = 20000000; // 20,000µs (50Hz)
  static constexpr int EXPORT_READY_DELAY = 50;
  static constexpr int INIT_DELAY = 100;
};

class PWMController {
public:
  // Device of the RP1 PWM0 block (GPIO18/19 with the pwm-2chan overlay).
  // The pwmchipN number of this device can change between kernels.
  static constexpr const char *DEFAULT_PWM_DEVICE = "1f00098000.pwm";

  // chip is a pwmchipN name or a device name such as "1f00098000.pwm".
  PWMController(const std::string &chip = DEFAULT_PWM_DEVICE);

  // Returns the pwmchipN name for a pwmchipN name or a PWM device name.
  static std::string resolveChip(const std::string &chipOrDevice);
  ~PWMController();

  void init(const std::vector<int> &channels);
  void exportChannel(int channel);
  void setPeriod(int channel, int period);
  void setDutyCycle(int channel, int dutyCycle);

  void enable(int channel);
  void disable(int channel);
  bool isExportReady(const int channel) const;
  int convertDutyCycle(const int dutyCycle) const;

  int getCurrentDutyCycle(int channel) const;

  // Constants for channel numbers
  static constexpr int CHANNEL_PAN = 2;
  static constexpr int CHANNEL_TILT = 3;

  // Movement command structure
  struct MovementCommand {
    int channel;
    int startDutyCycle;
    int targetDutyCycle;
    int duration; // in milliseconds
    std::function<int(float, int, int)>
        movementFunction; // Movement pattern function
    std::chrono::steady_clock::time_point startTime;
  };

  // Set position methods
  void setPosition(int targetPan, int targetTilt, int duration,
                   const std::string &movementFunctionName = "easeInOutQuad");
  void
  setPositionPanTilt(int targetPan, int targetTilt, int duration,
                     const std::string &movementFunctionName = "easeInOutQuad");

  void startControlLoop();
  void stopControlLoop();

private:
  std::string chip;
  struct ChannelFDs {
    int export_fd = -1;
    int period_fd = -1;
    int duty_cycle_fd = -1;
    int enable_fd = -1;
    mutable std::mutex mutex; // Per-channel mutex
  };
  std::map<int, ChannelFDs> channel_fds;
  void writeToFile(int fd, const std::string &value) const;
  void openChannelFileDescriptors(int channel);
  void initializeChannel(int channel);
  void configureChannel(int channel);

  static int openFileDescriptor(const std::string &filePath);
  std::string getPWMFilePath(const std::string &chip, int channel,
                             const std::string &property) const;
  std::string getPWMFilePath(const std::string &chip,
                             const std::string &property) const;

  // Member variables for current duty cycles
  std::map<int, int> currentDutyCycles;

  // Control loop variables
  std::queue<MovementCommand> commandQueue;
  std::mutex queueMutex;
  std::thread controlLoopThread;
  bool running = false;

  void controlLoop();

  // Movement function map
  std::map<std::string, std::function<int(float, int, int)>> movementFunctions;

  // Initialize movement functions
  void initializeMovementFunctions();

  // Add movement command
  void addMovementCommand(const MovementCommand &cmd);
};

// Movement functions
int linear(float t, int b, int c);
int easeInOutQuad(float t, int b, int c);

#endif // PWMCONTROLLER_HPP
