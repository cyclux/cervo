#ifndef KEYINPUTHANDLER_HPP
#define KEYINPUTHANDLER_HPP

#include "cervo.hpp"

void handleKeyPresses(PWMController &servo, const int speed, const int stepdist,
                      const std::string movementFunction);

#endif // KEYINPUTHANDLER_HPP
