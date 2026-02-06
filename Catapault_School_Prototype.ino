#include <Servo.h>

// =====================================================
// ARDUINO MEGA + HC-05 / HC-06 (Bluetooth on Serial1)
// NO laptop needed after upload
//
// COMMANDS (send via Serial Bluetooth Terminal):
//   F        -> Motor 1 FULL (tension)
//   S        -> Motor 1 RETRACT (no tension)
//   R        -> Motor 2 FULL (release)
//   H        -> Motor 2 RETRACT (block)
//   X        -> Fire once (full safe fire sequence)
//   E###     -> Repeat fire every ### seconds (E180 = every 3 min, E3600 = every hour)
//   D        -> Disable repeating
//
// Bluetooth wiring (Mega):
//   HC-05 TX -> RX1 (pin 19)
//   HC-05 RX -> TX1 (pin 18) THROUGH voltage divider
// =====================================================

// ================= SERVOS =================
Servo motor1;   // Tension motor
Servo motor2;   // Release motor

// Pins (your setup)
const byte MOTOR1_PIN = 53;
const byte MOTOR2_PIN = 52;

// Angle limits
const int M1_RETRACT = 180; // no tension
const int M1_FULL    = 0;   // max tension

const int M2_RETRACT = 120; // blocks / prevents release
const int M2_FULL    = 0;   // releases

// ================= SPEED / STRENGTH TUNING =================
const int STEP_DELAY_M1_MS      = 20; // slower = more effective torque
const int STEP_DELAY_M2_MS      = 10; // normal speed
const int STEP_DELAY_M2_FAST_MS = 2;  // fast release during X

// ================= SEQUENCE DELAYS =================
const int SAFE_SETTLE_MS        = 400;
const int HOLD_TENSION_MS       = 300;
const int TENSION_DELAY_MS      = 800;
const int RELEASE_DELAY_MS      = 80;
const int POST_RELEASE_HOLD_MS  = 700;

// ================= REPEAT SCHEDULING =================
bool repeatEnabled = false;
unsigned long repeatIntervalMs = 0;
unsigned long lastFireMs = 0;

// Track current positions
int m1Pos = M1_RETRACT;
int m2Pos = M2_RETRACT;

// -----------------------------------------------------
void moveServoSmoothCustom(Servo &s, int &currentPos, int targetPos, int stepDelayMs) {
  targetPos = constrain(targetPos, 0, 180);
  stepDelayMs = max(stepDelayMs, 0);

  if (currentPos < targetPos) {
    for (int p = currentPos; p <= targetPos; p++) {
      s.write(p);
      delay(stepDelayMs);
    }
  } else {
    for (int p = currentPos; p >= targetPos; p--) {
      s.write(p);
      delay(stepDelayMs);
    }
  }
  currentPos = targetPos;
}

// -----------------------------------------------------
void btPrint(const char* msg) {
  Serial1.println(msg);
}

// -----------------------------------------------------
void fireSequence() {
  btPrint("SAFE -> FIRE");

  // Ensure safe start
  moveServoSmoothCustom(motor1, m1Pos, M1_RETRACT, STEP_DELAY_M1_MS);
  moveServoSmoothCustom(motor2, m2Pos, M2_RETRACT, STEP_DELAY_M2_MS);
  delay(SAFE_SETTLE_MS);

  // Build tension (slow = strong)
  moveServoSmoothCustom(motor1, m1Pos, M1_FULL, STEP_DELAY_M1_MS);
  delay(HOLD_TENSION_MS);
  delay(TENSION_DELAY_MS);

  // Release (fast)
  moveServoSmoothCustom(motor2, m2Pos, M2_FULL, STEP_DELAY_M2_FAST_MS);
  delay(RELEASE_DELAY_MS);

  // Allow full release before retracting tension
  delay(POST_RELEASE_HOLD_MS);

  // Reset safe
  moveServoSmoothCustom(motor1, m1Pos, M1_RETRACT, STEP_DELAY_M1_MS);
  moveServoSmoothCustom(motor2, m2Pos, M2_RETRACT, STEP_DELAY_M2_MS);

  btPrint("DONE");
}

// -----------------------------------------------------
void handleCommand(char cmd) {
  cmd = toupper(cmd);

  switch (cmd) {
    case 'F':
      moveServoSmoothCustom(motor1, m1Pos, M1_FULL, STEP_DELAY_M1_MS);
      btPrint("M1 FULL");
      break;

    case 'S':
      moveServoSmoothCustom(motor1, m1Pos, M1_RETRACT, STEP_DELAY_M1_MS);
      btPrint("M1 RETRACT");
      break;

    case 'R':
      moveServoSmoothCustom(motor2, m2Pos, M2_FULL, STEP_DELAY_M2_MS);
      btPrint("M2 RELEASE");
      break;

    case 'H':
      moveServoSmoothCustom(motor2, m2Pos, M2_RETRACT, STEP_DELAY_M2_MS);
      btPrint("M2 BLOCK");
      break;

    case 'X':
      fireSequence();
      break;

    case 'E': { // Enable repeating
      long seconds = Serial1.parseInt();
      if (seconds > 0) {
        repeatIntervalMs = (unsigned long)seconds * 1000UL;
        lastFireMs = millis();
        repeatEnabled = true;

        Serial1.print("REPEAT EVERY ");
        Serial1.print(seconds);
        Serial1.println(" SECONDS");
      } else {
        btPrint("ERR: USE E###");
      }
      break;
    }

    case 'D':
      repeatEnabled = false;
      btPrint("REPEAT OFF");
      break;

    default:
      break;
  }
}

// -----------------------------------------------------
void setup() {
  // Bluetooth on Serial1 (pins 18/19)
  Serial1.begin(9600);

  motor1.attach(MOTOR1_PIN);
  motor2.attach(MOTOR2_PIN);

  // Start safe
  motor1.write(M1_RETRACT);
  motor2.write(M2_RETRACT);
  m1Pos = M1_RETRACT;
  m2Pos = M2_RETRACT;

  btPrint("READY: F S R H X | E### repeat(sec) | D stop");
}

// -----------------------------------------------------
void loop() {
  // Bluetooth input
  while (Serial1.available()) {
    char c = Serial1.read();
    handleCommand(c);
  }

  // Repeating scheduler
  if (repeatEnabled) {
    unsigned long now = millis();
    if (now - lastFireMs >= repeatIntervalMs) {
      lastFireMs = now;
      fireSequence();
    }
  }
}
