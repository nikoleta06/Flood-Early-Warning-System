/**
 * Flood Early Warning System — Master Arduino (Downstream Station)
 *
 * Responsibilities:
 *   - Measures local water level with PING))) ultrasonic sensor
 *   - Requests upstream readings from Slave via I2C
 *   - Runs cascade logic to determine flood state
 *   - Controls LCD, LEDs, buzzer and servo accordingly
 *
 * Wiring:
 *   PING))) SIG  → Digital 3
 *   LCD RS       → Digital 12
 *   LCD EN       → Digital 11
 *   LCD DB4-DB7  → Digital 7, 8, 9, 10
 *   LED Green    → Digital 6
 *   LED Yellow   → Digital 5
 *   LED Red      → Digital 4
 *   Buzzer       → Analog A0
 *   Servo signal → Digital 13
 *   I2C SDA      → Analog A4  (connects to Slave A4)
 *   I2C SCL      → Analog A5  (connects to Slave A5)
 *   GND          → GND        (must share ground with Slave)
 *
 * Author: github.com/yourhandle
 * License: MIT
 */

#include <Wire.h>
#include <LiquidCrystal.h>

// ── LCD: LiquidCrystal(RS, EN, DB4, DB5, DB6, DB7) ──────────
LiquidCrystal lcd(12, 11, 7, 8, 9, 10);

// ── Pin definitions ──────────────────────────────────────────
#define PING_PIN     3    // PING))) single-wire signal pin
#define LED_GREEN    6    // Safe state indicator
#define LED_YELLOW   5    // Warning / pre-warning indicator
#define LED_RED      4    // Danger / critical indicator
#define BUZZER_PIN   A0   // Piezo buzzer (tone() works on analog pins)
#define SERVO_PIN    13   // Servo signal — simulates floodgate valve

// ── Distance thresholds (cm) ─────────────────────────────────
// These define the boundaries between flood states.
// Adjust these values to match your real-world sensor placement.
#define LEVEL_WARNING   20   // Below this → WARNING
#define LEVEL_DANGER    12   // Below this → DANGER
#define LEVEL_CRITICAL   6   // Below this → CRITICAL

// ── I2C ──────────────────────────────────────────────────────
#define SLAVE_ADDRESS 0x08   // Must match Wire.begin(0x08) in slave.ino

// ── Flood states ─────────────────────────────────────────────
// Using #define instead of enum for Tinkercad compatibility
#define STATE_SAFE          0
#define STATE_PRE_WARNING   1
#define STATE_WARNING       2
#define STATE_DANGER        3
#define STATE_CRITICAL      4

// ── Global state ─────────────────────────────────────────────
int currentState = STATE_SAFE;
unsigned long lastAlarmTime = 0;   // Prevents buzzer from firing every loop

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  // Master mode — Wire.begin() without an address
  Wire.begin();

  // Initialize LCD: 16 columns, 2 rows
  lcd.begin(16, 2);

  // All actuator pins as outputs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(SERVO_PIN,  OUTPUT);

  // Startup message — stays for 2 seconds before main loop
  lcd.print("Flood Monitor");
  delay(2000);
  lcd.clear();

  Serial.println("Flood Monitor ONLINE");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // Step 1: Read local distance (downstream sensor)
  float local = measureDistance();

  // Step 2: Request upstream distance from Slave over I2C
  float upstream = requestUpstreamData();

  // Step 3: Determine flood state using both readings
  int newState = evaluateFloodState(local, upstream);

  // Step 4: Act on the state
  updateLEDs(newState);
  triggerAlarm(newState);
  updateDisplay(local, upstream, newState);
  printDebug(local, upstream, newState);

  currentState = newState;
  delay(1500);
}

// ─────────────────────────────────────────────────────────────
/**
 * Measures distance using the Parallax PING))) sensor.
 *
 * The PING))) uses a single pin for both sending and receiving.
 * We switch the pin direction: OUTPUT to send the trigger pulse,
 * then INPUT to read the echo duration.
 *
 * Returns distance in centimeters.
 */
float measureDistance() {
  // Send trigger pulse: LOW → HIGH (10μs) → LOW
  pinMode(PING_PIN, OUTPUT);
  digitalWrite(PING_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(PING_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(PING_PIN, LOW);

  // Switch to input and measure echo duration
  pinMode(PING_PIN, INPUT);
  long duration = pulseIn(PING_PIN, HIGH);

  // Speed of sound: 29 μs/cm, divide by 2 (round trip)
  return duration / 29.0 / 2.0;
}

// ─────────────────────────────────────────────────────────────
/**
 * Requests 2 bytes from the Slave Arduino over I2C.
 *
 * The Slave sends the distance as an integer in millimeters * 10,
 * packed as high byte + low byte. We reassemble and convert to cm.
 *
 * Returns upstream distance in centimeters, or 999.0 on failure.
 */
float requestUpstreamData() {
  Wire.requestFrom(SLAVE_ADDRESS, 2);

  if (Wire.available() == 2) {
    int high = Wire.read();
    int low  = Wire.read();
    // Reassemble the two bytes into one integer, then convert mm*10 → cm
    return ((high << 8) | low) / 10.0;
  }

  // Slave not responding — return a safe fallback value
  Serial.println("[WARN] Slave I2C timeout");
  return 999.0;
}

// ─────────────────────────────────────────────────────────────
/**
 * Core decision logic — determines flood state from both readings.
 *
 * The key insight is PRE_WARNING: if upstream is already in danger
 * but local is still safe, we raise an alert immediately. This gives
 * reaction time before the water physically arrives downstream.
 * That's the cascade logic.
 */
int evaluateFloodState(float local, float upstream) {
  // Either station in critical zone → immediate CRITICAL
  if (local < LEVEL_CRITICAL || upstream < LEVEL_CRITICAL)
    return STATE_CRITICAL;

  // Local station in danger zone
  if (local < LEVEL_DANGER)
    return STATE_DANGER;

  // CASCADE LOGIC:
  // Upstream is in danger but local is still safe.
  // Water is on its way — warn now, not when it arrives.
  if (upstream < LEVEL_DANGER && local >= LEVEL_WARNING)
    return STATE_PRE_WARNING;

  // Either station approaching danger
  if (local < LEVEL_WARNING || upstream < LEVEL_WARNING)
    return STATE_WARNING;

  return STATE_SAFE;
}

// ─────────────────────────────────────────────────────────────
/**
 * Updates the three status LEDs.
 * Only one LED is on at a time — traffic light style.
 */
void updateLEDs(int state) {
  digitalWrite(LED_GREEN,  state == STATE_SAFE);
  digitalWrite(LED_YELLOW, state == STATE_PRE_WARNING || state == STATE_WARNING);
  digitalWrite(LED_RED,    state == STATE_DANGER || state == STATE_CRITICAL);
}

// ─────────────────────────────────────────────────────────────
/**
 * Moves the servo to reflect the current flood state.
 * 0° = floodgate fully closed, 120° = fully open.
 *
 * We use manual PWM pulses instead of the Servo library
 * to avoid conflicts with tone() on some boards.
 */
void moveServo(int angle) {
  // Convert angle (0-180) to pulse width (1000-2000 μs)
  int pulseWidth = map(angle, 0, 180, 1000, 2000);
  for (int i = 0; i < 20; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(20000 - pulseWidth);
  }
}

// ─────────────────────────────────────────────────────────────
/**
 * Triggers buzzer alerts and moves servo.
 * Uses a 3-second cooldown to avoid continuous noise.
 * Different frequencies and patterns per state.
 */
void triggerAlarm(int state) {
  // Servo angles per state: closed → open progressively
  int angles[] = {0, 30, 60, 90, 120};
  moveServo(angles[state]);

  unsigned long now = millis();
  if (now - lastAlarmTime < 3000) return;   // Cooldown

  if      (state == STATE_WARNING)  { tone(BUZZER_PIN, 1200, 200); }
  else if (state == STATE_DANGER)   { tone(BUZZER_PIN, 1800, 300); }
  else if (state == STATE_CRITICAL) { tone(BUZZER_PIN, 2500, 800); }
  else                              { noTone(BUZZER_PIN); }

  lastAlarmTime = now;
}

// ─────────────────────────────────────────────────────────────
/**
 * Updates the LCD display with current readings and state.
 * Line 1: downstream and upstream distances
 * Line 2: current state label
 *
 * We overwrite characters in place instead of calling lcd.clear()
 * to avoid the visual flicker artifact in the Tinkercad simulator.
 */
void updateDisplay(float local, float upstream, int state) {
  const char* labels[] = {"SAFE", "PRE ", "WARN", "DNGR", "CRIT"};

  lcd.setCursor(0, 0);
  lcd.print("D:");
  lcd.print((int)local);
  lcd.print("    ");   // Pad to overwrite leftover digits
  lcd.setCursor(8, 0);
  lcd.print("U:");
  lcd.print((int)upstream);
  lcd.print("    ");

  lcd.setCursor(0, 1);
  lcd.print("St:");
  lcd.print(labels[state]);
  lcd.print("     ");
}

// ─────────────────────────────────────────────────────────────
/**
 * Prints a debug line to Serial Monitor every loop cycle.
 * Useful for verifying cascade logic behaviour during testing.
 */
void printDebug(float local, float upstream, int state) {
  const char* names[] = {"SAFE", "PRE_WARN", "WARNING", "DANGER", "CRITICAL"};
  Serial.print("Local=");   Serial.print(local);
  Serial.print("  Up=");    Serial.print(upstream);
  Serial.print("  State="); Serial.println(names[state]);
}
