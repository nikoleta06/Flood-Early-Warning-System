/**
 * Flood Early Warning System — Slave Arduino (Upstream Station)
 *
 * Responsibilities:
 *   - Continuously measures upstream water level with PING))) sensor
 *   - Responds to I2C data requests from Master
 *   - Handles local alerts independently (LED + buzzer)
 *
 * The Slave is a passive node. It does not initiate communication —
 * it simply stores the latest measurement and responds when asked.
 * This keeps the I2C callback fast and non-blocking.
 *
 * Wiring:
 *   PING))) SIG → Digital 3
 *   Buzzer      → Digital 5
 *   Local LED   → Digital 6  (with 220Ω resistor)
 *   I2C SDA     → Analog A4  (connects to Master A4)
 *   I2C SCL     → Analog A5  (connects to Master A5)
 *   GND         → GND        (must share ground with Master)
 *
 * Author: github.com/yourhandle
 * License: MIT
 */

#include <Wire.h>

// ── Pin definitions ──────────────────────────────────────────
#define PING_PIN      3   // PING))) single-wire signal pin
#define BUZZER_PIN    5   // Local piezo buzzer
#define LOCAL_LED     6   // Local danger LED

// ── I2C ──────────────────────────────────────────────────────
// This address must match SLAVE_ADDRESS in master.ino
#define SLAVE_ADDRESS 0x08

// ── Danger threshold ─────────────────────────────────────────
// Local alert fires when distance drops below this value (in mm*10)
// 120 = 12cm
#define LOCAL_DANGER_THRESHOLD 120

// ── Shared state ─────────────────────────────────────────────
// volatile because it is written in loop() and read in the I2C
// interrupt callback sendDistanceData(). Without volatile, the
// compiler might cache a stale value in a register.
volatile int lastDistanceMM = 9999;   // Stored as mm * 10 for integer precision

// ─────────────────────────────────────────────────────────────
void setup() {
  // Register as I2C Slave with address 0x08
  Wire.begin(SLAVE_ADDRESS);

  // Register the callback that fires when Master requests data.
  // This runs inside an interrupt — keep it short.
  Wire.onRequest(sendDistanceData);

  pinMode(PING_PIN,   OUTPUT);   // Direction changes inside measureDistance()
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LOCAL_LED,  OUTPUT);

  Serial.begin(9600);
  Serial.println("Slave ONLINE — upstream station ready");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // Measure and store — the I2C callback reads this value asynchronously
  lastDistanceMM = (int)(measureDistance() * 10);

  // Local alert: if upstream is already in danger, warn immediately
  // without waiting for the Master to ask
  bool localDanger = lastDistanceMM < LOCAL_DANGER_THRESHOLD;
  digitalWrite(LOCAL_LED, localDanger);

  if (localDanger) {
    tone(BUZZER_PIN, 1000, 200);
  }

  Serial.print("Upstream: ");
  Serial.print(lastDistanceMM / 10.0);
  Serial.println(" cm");

  delay(800);
}

// ─────────────────────────────────────────────────────────────
/**
 * I2C callback — called automatically when Master requests data.
 *
 * Sends lastDistanceMM as two bytes: high byte first, then low byte.
 * The Master reassembles them with: (high << 8) | low
 *
 * Why two bytes? I2C transfers one byte at a time. An int (0–9999)
 * needs 14 bits, which doesn't fit in 8 bits. Splitting into
 * high/low bytes is the standard approach.
 */
void sendDistanceData() {
  Wire.write(lastDistanceMM >> 8);        // High byte (bits 15–8)
  Wire.write(lastDistanceMM & 0xFF);      // Low byte  (bits 7–0)
}

// ─────────────────────────────────────────────────────────────
/**
 * Measures distance using the Parallax PING))) sensor.
 *
 * The PING))) uses a single pin for both sending and receiving.
 * We switch the pin direction: OUTPUT for the trigger pulse,
 * then INPUT to capture the echo.
 *
 * Returns distance in centimeters.
 */
float measureDistance() {
  // Send trigger pulse
  pinMode(PING_PIN, OUTPUT);
  digitalWrite(PING_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(PING_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(PING_PIN, LOW);

  // Read echo duration
  pinMode(PING_PIN, INPUT);
  long duration = pulseIn(PING_PIN, HIGH);

  // Convert to centimeters: speed of sound = 29 μs/cm, ÷2 for round trip
  return duration / 29.0 / 2.0;
}
