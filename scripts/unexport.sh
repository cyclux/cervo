#!/bin/sh

# Find the pwmchip of the RP1 PWM0 block. Its number can change between kernels.
DEVICE=1f00098000.pwm
for c in /sys/class/pwm/pwmchip*; do
  [ "$(basename "$(readlink "$c/device")")" = "$DEVICE" ] && CHIP=$c
done
[ -n "$CHIP" ] || { echo "No PWM chip found for $DEVICE" >&2; exit 1; }

echo 2 >>"$CHIP/unexport"
echo 3 >>"$CHIP/unexport"
