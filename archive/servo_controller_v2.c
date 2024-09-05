#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*

Detailed Description of Import Modules:
1. <stdio.h>:
   - This header is part of the standard input-output library in C.
   - Used for functions such as `printf` for outputting data to the console, and
`perror` for printing error messages.

2. <stdlib.h>:
   - This header includes standard library functions for memory allocation,
process control, conversions, and others.
   - Used for functions such as `malloc` to allocate memory dynamically and
`free` to free the allocated memory.

3. <string.h>:
   - This header provides functions for manipulating C strings and memory.
   - Used for functions such as `memcpy` for copying memory areas and `strerror`
to get a string describing the error number.

4. <fcntl.h>:
   - This header includes functions for file control operations.
   - Used for functions such as `open` to open files and `fcntl` to perform
operations on file descriptors.

5. <unistd.h>:
   - This header provides access to the POSIX operating system API.
   - Used for functions such as `close` to close file descriptors, `write` to
write to a file descriptor, and `read` to read from a file descriptor.

6. <termios.h>:
   - This header is used for terminal control and provides the terminal I/O
interfaces.
   - Used for functions such as `tcgetattr` to get the parameters associated
with the terminal, `tcsetattr` to set the parameters associated with the
terminal, and `cfsetispeed`/`cfsetospeed` to set input/output baud rates.

7. <errno.h>:
   - This header defines macros for reporting and retrieving error conditions
through error codes.
   - Used to access the `errno` variable to determine the error code set by
system calls and library functions.
*/

// Define boolean type for convenience
typedef int bool;
#define true 1
#define false 0

// Constants
#define POL_CMD_LEN 2
#define MIN_SERVOS 800
#define MAX_SERVOS 2200

// Structure to store controller state and configuration
typedef struct {
  int usb;                     // USB file descriptor for serial communication
  char PololuCmd[POL_CMD_LEN]; // Pololu command header (0xAA and device number)
  int Targets[MAX_SERVOS];     // Array to store target positions for servos
  int Mins[MAX_SERVOS]; // Array to store minimum allowed positions for servos
  int Maxs[MAX_SERVOS]; // Array to store maximum allowed positions for servos
} Controller;

// Close the serial port connection
void closeController(Controller *ctrl) { close(ctrl->usb); }

// Send a command to the Maestro controller
void sendCmd(Controller *ctrl, const char *cmd, size_t cmd_len) {
  // Construct the full command string including the PololuCmd header
  char cmdStr[POL_CMD_LEN + cmd_len];
  memcpy(cmdStr, ctrl->PololuCmd, POL_CMD_LEN);
  memcpy(cmdStr + POL_CMD_LEN, cmd, cmd_len);

  // Write the command to the serial port
  if (write(ctrl->usb, cmdStr, POL_CMD_LEN + cmd_len) < 0) {
    perror("Error writing to serial port");
  }
}

// Set the minimum and maximum target positions for a specific servo channel
void setRange(Controller *ctrl, int chan, int min, int max) {
  ctrl->Mins[chan] = min;
  ctrl->Maxs[chan] = max;
}

// Get the minimum target position for a specific servo channel
int getMin(Controller *ctrl, int chan) { return ctrl->Mins[chan]; }

// Get the maximum target position for a specific servo channel
int getMax(Controller *ctrl, int chan) { return ctrl->Maxs[chan]; }

// Set the target position for a specific servo channel
void setTarget(Controller *ctrl, int chan, int target) {
  // Constrain the target position within the set range if applicable
  if (ctrl->Mins[chan] > 0 && target < ctrl->Mins[chan]) {
    target = ctrl->Mins[chan];
  }
  if (ctrl->Maxs[chan] > 0 && target > ctrl->Maxs[chan]) {
    target = ctrl->Maxs[chan];
  }

  // Calculate the least significant byte and most significant byte of the
  // target position
  int lsb = target & 0x7F;
  int msb = (target >> 7) & 0x7F;

  // Construct the command to set the target position
  char cmd[4] = {0x04, (char)chan, (char)lsb, (char)msb};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));

  // Store the target position in the Targets array
  ctrl->Targets[chan] = target;
}

// Set the speed for a specific servo channel
void setSpeed(Controller *ctrl, int chan, int speed) {
  // Calculate the least significant byte and most significant byte of the speed
  // value
  int lsb = speed & 0x7F;
  int msb = (speed >> 7) & 0x7F;

  // Construct the command to set the speed
  char cmd[4] = {0x07, (char)chan, (char)lsb, (char)msb};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));
}

// Set the acceleration for a specific servo channel
void setAccel(Controller *ctrl, int chan, int accel) {
  // Calculate the least significant byte and most significant byte of the
  // acceleration value
  int lsb = accel & 0x7F;
  int msb = (accel >> 7) & 0x7F;

  // Construct the command to set the acceleration
  char cmd[4] = {0x09, (char)chan, (char)lsb, (char)msb};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));
}

// Get the current position of a specific servo channel
int getPosition(Controller *ctrl, int chan) {
  // Construct the command to get the position
  char cmd[2] = {0x10, (char)chan};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));

  // Read the least significant byte and most significant byte of the position
  // from the serial port
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

  // Return the combined position value
  return (msb << 8) + lsb;
}

// Check if a specific servo channel is moving to its target position
bool isMoving(Controller *ctrl, int chan) {
  if (ctrl->Targets[chan] > 0) {
    if (getPosition(ctrl, chan) != ctrl->Targets[chan]) {
      return true;
    }
  }
  return false;
}

// Check if any servo channels are moving to their target positions
bool getMovingState(Controller *ctrl) {
  // Construct the command to get the moving state
  char cmd[1] = {0x13};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));

  // Read the response from the serial port
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

  // Return the moving state
  return response != 0;
}

// Run a specific subroutine in the currently active script on the Maestro
void runScriptSub(Controller *ctrl, int subNumber) {
  // Construct the command to run the subroutine
  char cmd[2] = {0x27, (char)subNumber};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));
}

// Stop the currently running script on the Maestro
void stopScript(Controller *ctrl) {
  // Construct the command to stop the script
  char cmd[1] = {0x24};

  // Send the command
  sendCmd(ctrl, cmd, sizeof(cmd));
}

// Initialize and configure the serial port, and return a new Controller
// instance
Controller *createController(const char *ttyStr, int device) {
  // Allocate memory for the Controller structure
  Controller *ctrl = (Controller *)malloc(sizeof(Controller));
  if (ctrl == NULL) {
    perror("Failed to allocate memory for Controller");
    exit(1);
  }

  // Open the serial port
  ctrl->usb = open(ttyStr, O_RDWR | O_NOCTTY | O_NDELAY);
  if (ctrl->usb == -1) {
    perror("Failed to open the port");
    free(ctrl);
    exit(1);
  }

  // Configure the serial port
  struct termios options;
  if (tcgetattr(ctrl->usb, &options) < 0) {
    perror("Failed to get terminal attributes");
    close(ctrl->usb);
    free(ctrl);
    exit(1);
  }

  // Set the baud rate to 9600
  cfsetispeed(&options, B9600);
  cfsetospeed(&options, B9600);

  // Configure the serial port settings
  options.c_cflag |= (CLOCAL | CREAD); // Enable receiver and set local mode
  options.c_cflag &= ~PARENB;          // No parity
  options.c_cflag &= ~CSTOPB;          // 1 stop bit
  options.c_cflag &= ~CSIZE;           // Clear current character size mask
  options.c_cflag |= CS8;              // 8 data bits

  // Configure input and output settings
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input mode
  options.c_iflag &= ~(IXON | IXOFF | IXANY);         // No flow control
  options.c_oflag &= ~OPOST;                          // Raw output mode

  // Flush the input and output buffers
  tcflush(ctrl->usb, TCIFLUSH);

  // Apply the settings to the serial port
  if (tcsetattr(ctrl->usb, TCSANOW, &options) < 0) {
    perror("Failed to set terminal attributes");
    close(ctrl->usb);
    free(ctrl);
    exit(1);
  }

  // Set the file status flags to blocking mode
  fcntl(ctrl->usb, F_SETFL, 0);

  // Set the Pololu command header (0xAA and device number)
  ctrl->PololuCmd[0] = 0xAA;
  ctrl->PololuCmd[1] = device;

  // Initialize the Targets, Mins, and Maxs arrays
  for (int i = 0; i < MAX_SERVOS; ++i) {
    ctrl->Targets[i] = 0;
    ctrl->Mins[i] = 0;
    ctrl->Maxs[i] = 0;
  }

  return ctrl;
}

int main() {
  // Create and configure the Controller instance
  Controller *controller = createController("/dev/ttyACM0", 0x0C);

  // Example usage of the controller
  setTarget(controller, 0, 1500); // Set target position for servo 0
  setSpeed(controller, 0, 0);     // Set speed for servo 0
  setAccel(controller, 0, 0);     // Set acceleration for servo 0

  // Get and print the current position of the servo
  int position = getPosition(controller, 0);
  if (position != -1) {
    printf("Position: %d\n", position);
  } else {
    printf("Failed to get position\n");
  }

  // Check if the servo is moving
  if (isMoving(controller, 0)) {
    printf("Servo is moving.\n");
  } else {
    printf("Servo is not moving.\n");
  }

  // Clean up by closing the controller and freeing memory
  closeController(controller);
  free(controller);

  return 0;
}
