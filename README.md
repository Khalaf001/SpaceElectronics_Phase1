# CubeSat Low-Power Control Unit (LPCU)
**AESS Sustainability Hackathon 2026 — Challenge 1**

## Subsystem overview

The LPCU is an Attitude & Health Monitor Control Unit for a 3U CubeSat in Low Earth Orbit (LEO). It reads IMU and temperature data, performs basic health checks, and transmits a telemetry packet — all using an ESP32 with aggressive deep sleep duty cycling.

**Core sustainability claim:** Deep sleep + 3.3% duty cycle reduces average system power from **277.1 mW (baseline)** to **9.5 mW (optimized)** — a **96.6% reduction**, extending subsystem battery life from 0.15 days to 4.40 days on a 1 Wh budget.

---

## Repository structure

```
cubesat_lpcu/
├── README.md
├── src/
│   └── lpcu_firmware.ino       ESP32 Arduino firmware (deep sleep + duty cycle)
├── simulation/
│   └── power_model.py          Python power budget model (baseline vs optimized)
├── results/
│   └── power_results.json      Simulation output data
├── docs/
│   └── technical_design.pdf    Full technical design document
└── hardware/
    └── (block diagram, BOM)    System architecture notes
```

---

## How to run the simulation

```bash
python3 simulation/power_model.py
```

Outputs a printed comparison table and writes `results/power_results.json`.

**Requirements:** Python 3.x, no external libraries needed.

---

## How to flash the firmware

1. Install [Arduino IDE](https://www.arduino.cc/en/software) with ESP32 board support.
2. Open `src/lpcu_firmware.ino`.
3. Select **ESP32 Dev Module** as board.
4. Connect MPU-6050 (SDA→GPIO21, SCL→GPIO22) and TMP117 (same I2C bus).
5. Upload and open Serial Monitor at 115200 baud.

---

## Power budget summary

| Component | Baseline (mW) | Optimized (mW) | Reduction |
|-----------|:---:|:---:|:---:|
| ESP32 MCU | 264.0 | 8.8 | 96.7% |
| MPU-6050 IMU | 11.2 | 0.4 | 96.4% |
| TMP117 Temp | 1.6 | 0.1 | 93.7% |
| TPS63021 VReg | 0.2 | 0.2 | 0% |
| **Total** | **277.1** | **9.5** | **96.6%** |

| Metric | Baseline | Optimized |
|--------|:---:|:---:|
| Energy / orbit | 0.4156 Wh | 0.0142 Wh |
| Energy / day | 6.65 Wh | 0.23 Wh |
| Days on 1 Wh battery | **0.15 days** | **4.40 days** |
| Lifetime gain | — | **+2827%** |

---

## Key engineering decisions

- **ESP32 chosen** for its hardware deep sleep (10 µA) and ULP co-processor.
- **MPU-6050** supports cycle mode (5 µA standby) — sensor sleeps between measurements.
- **TMP117** uses one-shot mode — draws power only during the 15.5 ms conversion.
- **2s / 60s duty cycle** matches LEO health-monitoring requirements; attitude data once per minute is sufficient for passive-stabilised CubeSats.
- **No hardware required** — the simulation alone satisfies the challenge's evidence requirement.

---

## Sustainability argument

In LEO, every milliwatt saved extends mission life and reduces the thermal load on the structure. A subsystem drawing 277 mW continuously would drain a 1 Wh budget in 3.6 hours. The same subsystem at 9.5 mW lasts 4.4 days — enough to survive an eclipse anomaly and attempt recovery. That margin is the difference between a successful mission and a lost satellite.

---

*AI tools (Claude) were used for documentation drafting. All power calculations and firmware were validated by the team.*
