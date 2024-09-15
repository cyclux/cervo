#include "key_input_handler.hpp"
#include <ncurses.h>
#include <thread>
#include <unordered_set>

void handleKeyPresses(PWMController &servo, const int speed, const int stepdist,
                      int &currentDutyCyclePan, int &currentDutyCycleTilt) {
  // Initialize ncurses to capture key presses
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);           // Hide the cursor
  nodelay(stdscr, TRUE); // Non-blocking getch

  int targetDutyCyclePan = currentDutyCyclePan;
  int targetDutyCycleTilt = currentDutyCycleTilt;

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
        targetDutyCyclePan = currentDutyCyclePan + (stepdist * panDirection);
      }
      if (tiltChanged) {
        targetDutyCycleTilt = currentDutyCycleTilt + (stepdist * tiltDirection);
      }

      if (panChanged && tiltChanged) {
        // Move both pan and tilt concurrently
        std::thread panThread(&PWMController::smoothMove, &servo, 2,
                              std::ref(currentDutyCyclePan), targetDutyCyclePan,
                              speed);
        std::thread tiltThread(&PWMController::smoothMove, &servo, 3,
                               std::ref(currentDutyCycleTilt),
                               targetDutyCycleTilt, speed);

        panThread.join();
        tiltThread.join();
      } else if (panChanged) {
        servo.smoothMove(2, currentDutyCyclePan, targetDutyCyclePan, speed);
      } else if (tiltChanged) {
        servo.smoothMove(3, currentDutyCycleTilt, targetDutyCycleTilt, speed);
      }
    }

    // Clear the screen and print the current duty cycle
    clear();
    mvprintw(0, 0, "Current Duty Cycle | Pan: %d | Tilt: %d",
             currentDutyCyclePan, currentDutyCycleTilt);
    refresh();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(50)); // Adjust as needed
  }

  // Cleanup ncurses
  endwin();
}
