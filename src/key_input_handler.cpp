#include "key_input_handler.hpp"
#include <ncurses.h>

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
