# 🌊 Flood Early Warning System
### Distributed IoT flood detection with cascade logic — built on Arduino

![Status](https://img.shields.io/badge/status-prototype-blue)
![Platform](https://img.shields.io/badge/platform-Arduino%20Uno-teal)
![License](https://img.shields.io/badge/license-MIT-green)
![Simulation](https://img.shields.io/badge/simulation-Tinkercad-orange)

---

## What is this?

Most flood sensors work like a smoke alarm — they go off only when the danger has already arrived. This system does something different: it watches **two points simultaneously** and warns the downstream station **before** the water gets there.

This is called **cascade logic**, and it's the core idea behind this project.

Two Arduino Uno boards communicate over I2C. The upstream station (Slave) measures water level and sends readings to the downstream station (Master). The Master combines both readings and decides the alert level. If the upstream is rising but the downstream is still safe — the system already raises a PRE_WARNING. You get reaction time.

```
[Upstream Station]          [Downstream Station]
  Arduino Slave    ←I2C→    Arduino Master
  PING))) sensor             PING))) sensor
  Local buzzer               LCD 16x2
  Local LED                  3x LEDs (green/yellow/red)
                             Buzzer
                             Servo (floodgate sim)
```

---

## How the cascade logic works

The system has 5 states. What makes it interesting is the PRE_WARNING state — it fires when upstream is in danger but downstream is still fine. No other low-cost DIY system does this.

| State | Condition | LED | Action |
|-------|-----------|-----|--------|
| SAFE | All distances > 30cm | 🟢 Green | Nothing |
| PRE_WARNING | Upstream < 12cm, Local > 20cm | 🟡 Yellow | Cascade alert — water is coming |
| WARNING | Any distance 12–20cm | 🟡 Yellow | Buzzer 1200Hz |
| DANGER | Local distance 6–12cm | 🔴 Red | Buzzer 1800Hz, Servo 90° |
| CRITICAL | Any distance < 6cm | 🔴 Red | Buzzer 2500Hz, Servo 120° |

---

## Hardware

### What you need

| Component | Qty | Notes |
|-----------|-----|-------|
| Arduino Uno R3 | 2 | One Master, one Slave |
| Parallax PING))) Ultrasonic | 2 | 3-pin sensor (GND, 5V, SIG) |
| LCD 16x2 | 1 | 4-bit parallel mode, no I2C module needed |
| Micro Servo | 1 | Simulates floodgate valve |
| Piezo Buzzer | 2 | One per Arduino |
| LED Green | 1 | SAFE indicator |
| LED Yellow | 1 | WARNING / PRE_WARNING indicator |
| LED Red | 1 | DANGER / CRITICAL indicator |
| Resistor 220Ω | 3 | One per LED |
| Jumper wires | — | Male-to-male |

### Wiring — I2C between the two Arduinos

This is the most important connection. Without it, cascade logic doesn't work.

| Slave Pin | Master Pin | Wire color | Function |
|-----------|------------|------------|----------|
| A4 (SDA) | A4 (SDA) | Blue | Data line |
| A5 (SCL) | A5 (SCL) | Yellow | Clock line |
| GND | GND | Black | Common ground — mandatory |

### Master Arduino pin mapping

| Component | Pin | Notes |
|-----------|-----|-------|
| PING))) SIG | Digital 3 | Signal (OUTPUT then INPUT) |
| LCD RS | Digital 12 | — |
| LCD EN | Digital 11 | — |
| LCD DB4–DB7 | Digital 7, 8, 9, 10 | 4-bit mode |
| LED Green | Digital 6 | — |
| LED Yellow | Digital 5 | — |
| LED Red | Digital 4 | — |
| Buzzer | Analog A0 | tone() works on analog pins |
| Servo signal | Digital 13 | — |

### Slave Arduino pin mapping

| Component | Pin | Notes |
|-----------|-----|-------|
| PING))) SIG | Digital 3 | Signal (OUTPUT then INPUT) |
| Buzzer | Digital 5 | Local alert |
| LED | Digital 6 | Local danger indicator |

---

## Simulation

This project was built and tested in [Tinkercad Circuits](https://www.tinkercad.com/circuits).

> 🔗 **[Open simulation in Tinkercad](#)** ← replace with your link

### How to test it

1. Start the simulation
2. Click on the **Slave PING)))** sensor → drag the blue dot close to the sensor (< 12cm)
3. Watch the Serial Monitor — you should see `State=PRE_WARN` while the Master distance is still safe
4. Drag the **Master PING)))** dot close too → `State=DANGER` or `CRITICAL`

### Serial Monitor output

```
Flood Monitor ONLINE
Local=336.76  Up=335.00  State=SAFE
Local=172.97  Up=7.90    State=PRE_WARN    ← cascade logic firing
Local=9.95    Up=335.00  State=DANGER
Local=4.20    Up=4.10    State=CRITICAL
```

---

## Code structure

```
firmware/
├── master/
│   └── master.ino    — measures local distance, requests upstream data,
│                       runs evaluateFloodState(), controls all actuators
└── slave/
    └── slave.ino     — measures upstream distance, responds to I2C
                        requests, handles local alerts
```

### Key function — evaluateFloodState()

```cpp
int evaluateFloodState(float local, float upstream) {
  if (local < LEVEL_CRITICAL || upstream < LEVEL_CRITICAL)
    return STATE_CRITICAL;
  if (local < LEVEL_DANGER)
    return STATE_DANGER;
  // Cascade logic: upstream in danger, local still safe
  if (upstream < LEVEL_DANGER && local >= LEVEL_WARNING)
    return STATE_PRE_WARNING;
  if (local < LEVEL_WARNING || upstream < LEVEL_WARNING)
    return STATE_WARNING;
  return STATE_SAFE;
}
```

### Libraries used

| Library | Purpose |
|---------|---------|
| `Wire.h` | I2C communication between the two Arduinos |
| `LiquidCrystal.h` | LCD control in 4-bit parallel mode |

---

## Roadmap

This is a working prototype. Here's where it goes next:

- [ ] **v0.1** — Tinkercad simulation ✅
- [ ] **v0.2** — Real hardware build with Arduino Uno
- [ ] **v0.3** — Replace Arduino with ESP32 (built-in Wi-Fi, lower power)
- [ ] **v0.4** — Telegram bot alerts when state changes
- [ ] **v0.5** — Solar panel + battery for off-grid deployment
- [ ] **v1.0** — Mesh network between nodes, web dashboard, ML-based prediction

---

## Why this matters

Greece has experienced devastating flash floods (Mandra 2017, Thessaly 2023) where the absence of early warning systems cost lives. Existing professional solutions cost thousands of euros and require technical expertise to install and maintain. This project explores whether low-cost hardware and simple cascade logic can bridge that gap.

---

## Contributing

This project is in early prototype stage. If you want to contribute, open an issue or submit a pull request. Ideas especially welcome around:
- ESP32 migration
- LoRa communication for longer range between nodes
- ML-based water level prediction

---

## License

MIT — do whatever you want with it, just give credit.

---

*Built with Arduino, Tinkercad, and a lot of patience with breadboards.*
