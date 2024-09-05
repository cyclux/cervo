#!/bin/bash

# Default values
CHANNEL=3
CHIP=pwmchip2
PWM=pwm$CHANNEL
DEFAULT_DUTY_CYCLE=1500000 # Default duty cycle if not provided

# Check if a duty cycle was provided as a command-line argument
if [ $# -eq 1 ]; then
  DUTY_CYCLE=$(($1 * 1000))
else
  DUTY_CYCLE=$DEFAULT_DUTY_CYCLE
fi

# Export the PWM channel
echo $CHANNEL >/sys/class/pwm/$CHIP/export
sleep 1

# Set the period to 20,000µs (50Hz)
echo 20000000 >/sys/class/pwm/$CHIP/$PWM/period

# Set the duty cycle to the provided value or default value
echo $DUTY_CYCLE >/sys/class/pwm/$CHIP/$PWM/duty_cycle

# Enable the PWM signal

echo 1 >/sys/class/pwm/$CHIP/$PWM/enable
