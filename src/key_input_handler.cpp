#include "key_input_handler.hpp"
#include <ncurses.h>
#include <thread>
#include <unordered_set>

void handleKeyPresses(PWMController &servo, const int speed, const int stepdist,
                      const std::string movementFunction) {
  // Initialize ncurses to capture key presses
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);           // Hide the cursor
  nodelay(stdscr, TRUE); // Non-blocking getch

  int targetDutyCyclePan =
      servo.getCurrentDutyCycle(PWMController::CHANNEL_PAN);
  int targetDutyCycleTilt =
      servo.getCurrentDutyCycle(PWMController::CHANNEL_TILT);

  bool running = true;

  while (running) {
    int ch = getch();
    bool panChanged = false;
    bool tiltChanged = false;
    int panDirection = 0;
    int tiltDirection = 0;

    if (ch != ERR) {
      if (ch == 'q') {
        running = false;
        break;
      }

      switch (ch) {
      case KEY_UP:
        tiltDirection = 1;
        tiltChanged = true;
        break;
      case KEY_DOWN:
        tiltDirection = -1;
        tiltChanged = true;
        break;
      case KEY_LEFT:
        panDirection = 1;
        panChanged = true;
        break;
      case KEY_RIGHT:
        panDirection = -1;
        panChanged = true;
        break;
      default:
        break;
      }
    }

    if (panChanged || tiltChanged) {
      if (panChanged) {
        targetDutyCyclePan =
            servo.getCurrentDutyCycle(PWMController::CHANNEL_PAN) +
            (stepdist * panDirection);
      }
      if (tiltChanged) {
        targetDutyCycleTilt =
            servo.getCurrentDutyCycle(PWMController::CHANNEL_TILT) +
            (stepdist * tiltDirection);
      }

      // Ensure duty cycles are within valid range
      targetDutyCyclePan =
          std::clamp(targetDutyCyclePan, PWMConfig::PULSE_WIDTH_MIN,
                     PWMConfig::PULSE_WIDTH_MAX);
      targetDutyCycleTilt =
          std::clamp(targetDutyCycleTilt, PWMConfig::PULSE_WIDTH_MIN,
                     PWMConfig::PULSE_WIDTH_MAX);

      // Set position using the servo controller
      servo.setPosition(targetDutyCyclePan, targetDutyCycleTilt, speed,
                        movementFunction);
    }

    // Clear the screen and print the current duty cycle
    clear();
    mvprintw(0, 0, "Current Duty Cycle | Pan: %d | Tilt: %d",
             servo.getCurrentDutyCycle(PWMController::CHANNEL_PAN),
             servo.getCurrentDutyCycle(PWMController::CHANNEL_TILT));
    refresh();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(50)); // Adjust as needed
  }

  // Cleanup ncurses
  endwin();
}
