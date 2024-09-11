#ifndef PWMCONTROLLER_HPP
#define PWMCONTROLLER_HPP

#include <map>
#include <string>
#include <vector>

class PWMController {
public:
  // Constructor: Initialize the PWM controller with multiple channels
  PWMController(const std::string &chip = "pwmchip2");
  ~PWMController(); // Destructor to close all file descriptors

  // Method to initialize the PWM controller for multiple channels
  void init(const std::vector<int> &channels);

  // Methods to interact with individual PWM channels
  void exportChannel(int channel);
  void setPeriod(int channel, int period);
  void setDutyCycle(int channel, int dutyCycle);
  void smoothMove(int channel, int &currentDutyCycle, int targetDutyCycle,
                  int duration);
  void enable(int channel);
  void disable(int channel);

  // Helper method to check if channel is exported
  bool isExportReady(const int channel) const;
  // Helper method to convert duty cycle values
  int convertDutyCycle(const int dutyCycle) const;

private:
  std::string chip;

  // Structure to hold file descriptors for each PWM channel
  struct ChannelFDs {
    int export_fd;
    int period_fd;
    int duty_cycle_fd;
    int enable_fd;
  };

  // Map to store file descriptors for each channel
  std::map<int, ChannelFDs> channel_fds;

  // Helper method to write values to sysfs files via file descriptors
  void writeToFile(int fd, const std::string &value) const;

  // Helper method to open file descriptors and form file paths
  void openChannelFileDescriptors(int channel);
  void initializeChannel(int channel);
  void configureChannel(int channel);

  // General method to write values to a specific property
  void setPWMValue(const int channel, const std::string &property,
                   const std::string &value);
};

#endif // PWMCONTROLLER_HPP
