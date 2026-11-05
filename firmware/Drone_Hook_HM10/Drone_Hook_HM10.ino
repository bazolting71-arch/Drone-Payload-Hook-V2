/*
  Drone Payload Hook V2 - Arduino Nano + DSSERVO DS3235 + HM-10 Bluetooth
  ======================================================================

  Wiring (see docs/images/hm10_servo_wiring.png)
    D9   servo signal
    D10  <- HM-10 TXD
    D11  -> HM-10 RXD through a divider: D11 -[R]- RXD -[R]-[R]- GND (3 equal resistors, e.g. 10k)
    D4   <- HM-10 STATE (optional, HIGH when a phone is connected)
    5V   -> HM-10 VCC      GND -> HM-10 GND      HM-10 BRK: not connected
    Servo + / - go to a separate 6 V, 5 A supply; join its - to the Nano GND.

  Commands (phone app such as LightBlue, or the Serial Monitor at 9600 baud)
    o        open                    c        close (slow, extra slow near the lock)
    1-180    go to that angle        + / -    nudge 1 degree
    so       save current as OPEN    sc       save current as CLOSE
    r        reset to the defaults   ?        status
  LightBlue hex keypad: o=6F c=63 +=2B -=2D so=736F sc=7363 r=72 ?=3F, angle 175 = 313735

  Saved positions are kept in EEPROM, so they survive power-off.
*/

#include <Servo.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// ---- pins ----
const int SERVO_PIN = 9;   // servo signal
const int BT_RX_PIN = 10;  // Nano receives here  <- HM-10 TXD
const int BT_TX_PIN = 11;  // Nano sends here     -> HM-10 RXD (through the 3-resistor divider)
const int BT_STATE  = 4;   // HM-10 STATE (HIGH = phone connected). Optional

// ---- default positions (used until you save your own) ----
const int DEFAULT_OPEN  = 50;    // hook open
const int DEFAULT_CLOSE = 120;   // hook closed against the lock

// ---- speed (milliseconds per degree, bigger = slower) ----
const int OPEN_SPEED_MS  = 5;   // opening: fast
const int CLOSE_SPEED_MS = 15;  // closing: slow
const int FINAL_SPEED_MS = 40;  // last 10 degrees before the lock: extra slow
const int FINAL_ZONE_DEG = 10;

// ---- EEPROM storage ----
const int EE_MAGIC_ADDR = 0;    // marks that positions were saved
const int EE_OPEN_ADDR  = 1;
const int EE_CLOSE_ADDR = 2;
const byte EE_MAGIC     = 0x5C; // change this to make the board forget saved positions

Servo myServo;
SoftwareSerial bt(BT_RX_PIN, BT_TX_PIN);

int openPos  = DEFAULT_OPEN;
int closePos = DEFAULT_CLOSE;
int currentAngle;

// send a message to both the Serial Monitor and the phone
void say(const String &msg) {
  Serial.println(msg);
  bt.println(msg);
}

void loadPositions() {
  if (EEPROM.read(EE_MAGIC_ADDR) == EE_MAGIC) {
    openPos  = EEPROM.read(EE_OPEN_ADDR);
    closePos = EEPROM.read(EE_CLOSE_ADDR);
  }
}

void savePositions() {
  EEPROM.update(EE_OPEN_ADDR,  openPos);
  EEPROM.update(EE_CLOSE_ADDR, closePos);
  EEPROM.update(EE_MAGIC_ADDR, EE_MAGIC);
}

// move to an angle; going to the CLOSE position is slow, and extra slow at the end
void moveTo(int target) {
  target = constrain(target, 0, 180);
  bool closing = (target == closePos);
  while (currentAngle != target) {
    currentAngle += (target > currentAngle) ? 1 : -1;
    myServo.write(currentAngle);
    if (closing) {
      delay(abs(currentAngle - target) <= FINAL_ZONE_DEG ? FINAL_SPEED_MS : CLOSE_SPEED_MS);
    } else {
      delay(OPEN_SPEED_MS);
    }
  }
}

void status() {
  say("Angle " + String(currentAngle) +
      " | OPEN " + String(openPos) +
      " | CLOSE " + String(closePos) +
      (digitalRead(BT_STATE) == HIGH ? " | phone connected" : ""));
}

void help() {
  say("o=open c=close 1-180=angle +=up 1 -=down 1");
  say("so=save OPEN sc=save CLOSE r=reset ?=status");
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "o") {                 // open
    moveTo(openPos);
    say("OPEN (" + String(currentAngle) + ")");
  } else if (cmd == "c") {          // close slowly
    moveTo(closePos);
    say("CLOSED (" + String(currentAngle) + ")");
  } else if (cmd == "+") {          // nudge up 1 degree
    moveTo(currentAngle + 1);
    say("Angle " + String(currentAngle));
  } else if (cmd == "-") {          // nudge down 1 degree
    moveTo(currentAngle - 1);
    say("Angle " + String(currentAngle));
  } else if (cmd == "so") {         // save current angle as OPEN
    openPos = currentAngle;
    savePositions();
    say("Saved OPEN = " + String(openPos));
  } else if (cmd == "sc") {         // save current angle as CLOSE
    closePos = currentAngle;
    savePositions();
    say("Saved CLOSE = " + String(closePos));
  } else if (cmd == "r") {          // back to defaults
    openPos = DEFAULT_OPEN;
    closePos = DEFAULT_CLOSE;
    savePositions();
    say("Reset: OPEN " + String(openPos) + ", CLOSE " + String(closePos));
  } else if (cmd == "?") {
    status();
  } else {
    int a = cmd.toInt();
    if (a >= 1 && a <= 180) {       // go to any angle
      moveTo(a);
      say("Angle " + String(currentAngle));
    } else {
      help();
    }
  }
}

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  pinMode(BT_STATE, INPUT);

  loadPositions();
  currentAngle = openPos;
  myServo.write(openPos);      // start at the OPEN position
  myServo.attach(SERVO_PIN);
  delay(500);

  say("Hook ready.");
  help();
  status();
}

void loop() {
  if (bt.available())     handleCommand(bt.readStringUntil('\n'));
  if (Serial.available()) handleCommand(Serial.readStringUntil('\n'));
}
