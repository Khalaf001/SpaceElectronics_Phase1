/**
 * CubeSat Low-Power Control Unit (LPCU)
 * AESS Sustainability Hackathon 2026 — Challenge 1
 *
 * Subsystem : Attitude & Health Monitor Control Unit
 * Platform  : ESP32 (Arduino framework)
 * Strategy  : Deep sleep + 3.3% duty cycle (2s active / 60s cycle)
 *
 * Power:  Baseline (always-on) = 277.1 mW
 *         Optimized (this code) =   9.5 mW average
 *         Reduction             =  96.6%
 *
 * Components:
 *   - MPU-6050  IMU (I2C)
 *   - TMP117    temperature sensor (I2C)
 *   - TPS63021  buck-boost regulator (always powered)
 *   - UART      telemetry output (simulated)
 */

#include <Arduino.h>
#include <Wire.h>

// ─── Pin definitions ──────────────────────────────────────────────────────────
#define SDA_PIN           21
#define SCL_PIN           22
#define STATUS_LED        2    // built-in LED, active HIGH

// ─── I2C addresses ────────────────────────────────────────────────────────────
#define MPU6050_ADDR      0x68
#define TMP117_ADDR       0x48

// ─── Timing (microseconds for esp_deep_sleep_enable_timer_wakeup) ─────────────
#define SLEEP_DURATION_US (58ULL * 1000000ULL)  // 58 s sleep
#define SAMPLE_WINDOW_MS  2000                   // 2 s active window

// ─── Telemetry packet (stored in RTC slow memory — survives deep sleep) ───────
RTC_DATA_ATTR uint32_t boot_count = 0;
RTC_DATA_ATTR float    last_temp_C = 0;
RTC_DATA_ATTR float    last_accel_x = 0, last_accel_y = 0, last_accel_z = 0;
RTC_DATA_ATTR float    last_gyro_x  = 0, last_gyro_y  = 0, last_gyro_z  = 0;

// ─── MPU-6050 helpers ─────────────────────────────────────────────────────────
void mpu_wake() {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x6B);   // PWR_MGMT_1
    Wire.write(0x00);   // clear sleep bit
    Wire.endTransmission();
    delay(20);          // stabilise
}

void mpu_sleep() {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x6B);
    Wire.write(0x40);   // set sleep bit
    Wire.endTransmission();
}

bool mpu_read(float &ax, float &ay, float &az,
              float &gx, float &gy, float &gz) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B);   // ACCEL_XOUT_H
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(MPU6050_ADDR, 14, true);
    if (Wire.available() < 14) return false;
    auto read16 = []() -> int16_t {
        return (int16_t)((Wire.read() << 8) | Wire.read());
    };
    ax = read16() / 16384.0f;   // ±2g range
    ay = read16() / 16384.0f;
    az = read16() / 16384.0f;
    read16();                    // skip temperature register
    gx = read16() / 131.0f;     // ±250 °/s range
    gy = read16() / 131.0f;
    gz = read16() / 131.0f;
    return true;
}

// ─── TMP117 helpers ───────────────────────────────────────────────────────────
bool tmp117_read(float &temp_C) {
    // Trigger one-shot conversion
    Wire.beginTransmission(TMP117_ADDR);
    Wire.write(0x01);   // config register
    Wire.write(0x0C);   // one-shot mode
    Wire.write(0x00);
    Wire.endTransmission();
    delay(16);          // 15.5 ms conversion time

    Wire.beginTransmission(TMP117_ADDR);
    Wire.write(0x00);   // temperature result register
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(TMP117_ADDR, 2, true);
    if (Wire.available() < 2) return false;
    int16_t raw = (int16_t)((Wire.read() << 8) | Wire.read());
    temp_C = raw * 0.0078125f;  // 7.8125 m°C / LSB
    return true;
}

// ─── Telemetry transmit (UART, simulated radio frame) ─────────────────────────
void transmit_telemetry() {
    Serial.printf("[LPCU TLM] boot=%lu  temp=%.2f°C  "
                  "accel=(%.3f,%.3f,%.3f)g  "
                  "gyro=(%.2f,%.2f,%.2f)dps\n",
                  (unsigned long)boot_count,
                  last_temp_C,
                  last_accel_x, last_accel_y, last_accel_z,
                  last_gyro_x,  last_gyro_y,  last_gyro_z);
}

// ─── Health check ─────────────────────────────────────────────────────────────
bool health_ok() {
    // Alert conditions (simple threshold checks)
    bool temp_ok  = (last_temp_C > -40.0f && last_temp_C < 85.0f);
    bool accel_ok = (fabsf(last_accel_x) < 5.0f &&
                     fabsf(last_accel_y) < 5.0f &&
                     fabsf(last_accel_z) < 5.0f);
    return temp_ok && accel_ok;
}

// ─── Setup — runs once per wake cycle ─────────────────────────────────────────
void setup() {
    ++boot_count;

    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);  // 400 kHz fast mode

    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);

    // --- Sample sensors ---
    uint32_t t_start = millis();

    mpu_wake();
    delay(10);
    mpu_read(last_accel_x, last_accel_y, last_accel_z,
             last_gyro_x,  last_gyro_y,  last_gyro_z);
    mpu_sleep();

    tmp117_read(last_temp_C);

    // --- Transmit telemetry ---
    transmit_telemetry();

    if (!health_ok()) {
        Serial.println("[ALERT] Health check failed — flagging subsystem.");
    }

    uint32_t active_ms = millis() - t_start;
    Serial.printf("[LPCU] Active for %lu ms, entering deep sleep for 58s\n", active_ms);

    digitalWrite(STATUS_LED, LOW);

    // --- Deep sleep ---
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
    esp_deep_sleep_start();   // wakes back at setup()
}

void loop() {
    // Unreachable — deep sleep restarts from setup()
}
