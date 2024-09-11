#ifndef KEYINPUTHANDLER_HPP
#define KEYINPUTHANDLER_HPP

#include "cervo.hpp"

void handleKeyPresses(PWMController &servo, const int speed, const int stepdist,
                      int &currentDutyCyclePan, int &currentDutyCycleTilt);

#endif // KEYINPUTHANDLER_HPP
