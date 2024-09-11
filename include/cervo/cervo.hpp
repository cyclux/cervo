#ifndef PWMCONTROLLER_HPP
#define PWMCONTROLLER_HPP

#include <map>
#include <string>
#include <vector>

// Configuration constants for the PWM controller
struct PWMConfig {
  static constexpr int PULSE_WIDTH_MIN = 800;
  static constexpr int PULSE_WIDTH_MAX = 2000;
  static constexpr int PULSE_WIDTH_NEUTRAL = 1500;
  static constexpr int STEP_DIST = 500;
  static constexpr int SPEED = 1000;
  static constexpr int PERIOD = 20000000; // 20,000µs (50Hz)
  static constexpr int EXPORT_READY_DELAY = 10;
  static constexpr int INIT_DELAY = 100;
};

class PWMController {
public:
  PWMController(const std::string &chip = "pwmchip2");
  ~PWMController();
  void init(const std::vector<int> &channels);
  void exportChannel(int channel);
  void setPeriod(int channel, int period);
  void setDutyCycle(int channel, int dutyCycle);
  void smoothMove(int channel, int &currentDutyCycle, int targetDutyCycle,
                  int duration);
  void enable(int channel);
  void disable(int channel);
  bool isExportReady(const int channel) const;
  int convertDutyCycle(const int dutyCycle) const;

private:
  std::string chip;
  struct ChannelFDs {
    int export_fd;
    int period_fd;
    int duty_cycle_fd;
    int enable_fd;
  };
  std::map<int, ChannelFDs> channel_fds;
  void writeToFile(int fd, const std::string &value) const;
  void openChannelFileDescriptors(int channel);
  void initializeChannel(int channel);
  void configureChannel(int channel);
  void setPWMValue(const int channel, const std::string &property,
                   const std::string &value);

  static int openFileDescriptor(const std::string &filePath);

  // Overloaded helper method to construct file paths for PWM
  std::string getPWMFilePath(const std::string &chip, int channel,
                             const std::string &property) const;
  std::string getPWMFilePath(const std::string &chip,
                             const std::string &property) const;
};

int easeInOutQuad(float t, int b, int c, int d);

#endif // PWMCONTROLLER_HPP
