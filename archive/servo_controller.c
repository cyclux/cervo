#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef int bool;
#define true 1
#define false 0

#define POL_CMD_LEN 2
#define MAX_SERVOS 24

typedef struct {
  int usb;
  char PololuCmd[POL_CMD_LEN];
  int Targets[MAX_SERVOS];
  int Mins[MAX_SERVOS];
  int Maxs[MAX_SERVOS];
} Controller;

void closeController(Controller *ctrl) { close(ctrl->usb); }

void sendCmd(Controller *ctrl, const char *cmd, size_t cmd_len) {
  char cmdStr[POL_CMD_LEN + cmd_len];
  memcpy(cmdStr, ctrl->PololuCmd, POL_CMD_LEN);
  memcpy(cmdStr + POL_CMD_LEN, cmd, cmd_len);
  if (write(ctrl->usb, cmdStr, POL_CMD_LEN + cmd_len) < 0) {
    perror("Error writing to serial port");
  }
}

void setRange(Controller *ctrl, int chan, int min, int max) {
  ctrl->Mins[chan] = min;
  ctrl->Maxs[chan] = max;
}

int getMin(Controller *ctrl, int chan) { return ctrl->Mins[chan]; }

int getMax(Controller *ctrl, int chan) { return ctrl->Maxs[chan]; }

void setTarget(Controller *ctrl, int chan, int target) {
  if (ctrl->Mins[chan] > 0 && target < ctrl->Mins[chan]) {
    target = ctrl->Mins[chan];
  }
  if (ctrl->Maxs[chan] > 0 && target > ctrl->Maxs[chan]) {
    target = ctrl->Maxs[chan];
  }
  int lsb = target & 0x7F;
  int msb = (target >> 7) & 0x7F;
  char cmd[4] = {0x04, (char)chan, (char)lsb, (char)msb};
  sendCmd(ctrl, cmd, sizeof(cmd));
  ctrl->Targets[chan] = target;
}

void setSpeed(Controller *ctrl, int chan, int speed) {
  int lsb = speed & 0x7F;
  int msb = (speed >> 7) & 0x7F;
  char cmd[4] = {0x07, (char)chan, (char)lsb, (char)msb};
  sendCmd(ctrl, cmd, sizeof(cmd));
}

void setAccel(Controller *ctrl, int chan, int accel) {
  int lsb = accel & 0x7F;
  int msb = (accel >> 7) & 0x7F;
  char cmd[4] = {0x09, (char)chan, (char)lsb, (char)msb};
  sendCmd(ctrl, cmd, sizeof(cmd));
}

int getPosition(Controller *ctrl, int chan) {
  char cmd[2] = {0x10, (char)chan};
  sendCmd(ctrl, cmd, sizeof(cmd));
  unsigned char lsb, msb;
  int res = read(ctrl->usb, &lsb, 1);
  if (res < 0) {
    perror("Error reading LSB position");
    return -1;
  }
  if (res == 0) {
    fprintf(stderr, "No data read for LSB position\n");
    return -1;
  }
  res = read(ctrl->usb, &msb, 1);
  if (res < 0) {
    perror("Error reading MSB position");
    return -1;
  }
  if (res == 0) {
    fprintf(stderr, "No data read for MSB position\n");
    return -1;
  }
  return (msb << 8) + lsb;
}

bool isMoving(Controller *ctrl, int chan) {
  if (ctrl->Targets[chan] > 0) {
    if (getPosition(ctrl, chan) != ctrl->Targets[chan]) {
      return true;
    }
  }
  return false;
}

bool getMovingState(Controller *ctrl) {
  char cmd[1] = {0x13};
  sendCmd(ctrl, cmd, sizeof(cmd));
  char response;
  int res = read(ctrl->usb, &response, 1);
  if (res < 0) {
    perror("Error reading moving state");
    return false;
  }
  if (res == 0) {
    fprintf(stderr, "No data read for moving state\n");
    return false;
  }
  return response != 0;
}

void runScriptSub(Controller *ctrl, int subNumber) {
  char cmd[2] = {0x27, (char)subNumber};
  sendCmd(ctrl, cmd, sizeof(cmd));
}

void stopScript(Controller *ctrl) {
  char cmd[1] = {0x24};
  sendCmd(ctrl, cmd, sizeof(cmd));
}

Controller *createController(const char *ttyStr, int device) {
  Controller *ctrl = (Controller *)malloc(sizeof(Controller));
  if (ctrl == NULL) {
    perror("Failed to allocate memory for Controller");
    exit(1);
  }

  struct termios options;
  ctrl->usb = open(ttyStr, O_RDWR | O_NOCTTY | O_NDELAY);
  if (ctrl->usb == -1) {
    perror("Failed to open the port");
    free(ctrl);
    exit(1);
  }

  if (tcgetattr(ctrl->usb, &options) < 0) {
    perror("Failed to get terminal attributes");
    close(ctrl->usb);
    free(ctrl);
    exit(1);
  }

  cfsetispeed(&options, B9600);
  cfsetospeed(&options, B9600);

  options.c_cflag |= (CLOCAL | CREAD);
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;

  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_iflag &= ~(IXON | IXOFF | IXANY);
  options.c_oflag &= ~OPOST;

  tcflush(ctrl->usb, TCIFLUSH);

  if (tcsetattr(ctrl->usb, TCSANOW, &options) < 0) {
    perror("Failed to set terminal attributes");
    close(ctrl->usb);
    free(ctrl);
    exit(1);
  }

  fcntl(ctrl->usb, F_SETFL, 0);

  ctrl->PololuCmd[0] = 0xAA;
  ctrl->PololuCmd[1] = device;

  for (int i = 0; i < MAX_SERVOS; ++i) {
    ctrl->Targets[i] = 0;
    ctrl->Mins[i] = 0;
    ctrl->Maxs[i] = 0;
  }

  return ctrl;
}

int main() {
  Controller *controller = createController("/dev/ttyACM0", 0x0C);

  // Example usage of the controller
  setTarget(controller, 0, 2000);
  setSpeed(controller, 0, 0);
  setAccel(controller, 0, 0);

  int position = getPosition(controller, 0);
  if (position != -1) {
    printf("Position: %d\n", position);
  } else {
    printf("Failed to get position\n");
  }

  if (isMoving(controller, 0)) {
    printf("Servo is moving.\n");
  } else {
    printf("Servo is not moving.\n");
  }

  closeController(controller);
  free(controller);

  return 0;
}
