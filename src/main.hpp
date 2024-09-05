#ifndef PWMCONTROLLER_HPP
#define PWMCONTROLLER_HPP

#include <fstream>
#include <string>
#include <thread>
#include <vector>

// Constants for the PWM configuration
constexpr int PULSE_WIDTH_MIN = 800;
constexpr int PULSE_WIDTH_MAX = 2000;
constexpr int PULSE_WIDTH_NEUTRAL = 1500;
constexpr int STEP_DIST = 500;
constexpr int SPEED = 1000;
constexpr int PERIOD = 20000000; // 20,000µs (50Hz)
constexpr int CHANNEL = 2;       // 2 and 3 are valid on pi 5
constexpr const char *CHIP = "pwmchip2";
constexpr bool VERBOSE = false;

class PWMController {
public:
  // Method to initialize the PWM controller for multiple channels
  PWMController(int channel = CHANNEL, const std::string &chip = CHIP);
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
  int channel;
  std::string chip;
  std::string pwm;
  // Method to write values to sysfs files
  void writeToFile(const std::string &path, const std::string &value) const;
};

int easeInOutQuad(float t, int b, int c, int d);

void handleKeyPresses(PWMController &servo, const int speed, const int stepdist,
                      int &currentDutyCyclePan, int &currentDutyCycleTilt);

#endif // PWMCONTROLLER_HPP
