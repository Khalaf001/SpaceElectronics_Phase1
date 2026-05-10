"""
CubeSat Low-Power Control Unit (LPCU) — Power Model
AESS Sustainability Hackathon 2026 | Challenge 1

Subsystem: Attitude & Health Monitor Control Unit
Platform:  ESP32 (240 MHz dual-core, deep sleep capable)
Mission:   3U CubeSat in LEO, 90-min orbit period
"""

import json

COMPONENTS = {
    "mcu":          {"active_mA": 80.0,  "idle_mA": 10.0, "sleep_mA": 0.010, "voltage_V": 3.3},
    "imu":          {"active_mA":  3.4,                    "sleep_mA": 0.005, "voltage_V": 3.3},
    "temp_sensor":  {"active_mA":  0.5,                    "sleep_mA": 0.0015,"voltage_V": 3.3},
    "voltage_reg":  {"active_mA":  0.06,                                       "voltage_V": 3.3},
}

ORBIT_PERIOD_S   = 5400
SAMPLE_INTERVAL_S = 60
SAMPLE_DURATION_S =  2
DUTY_CYCLE        = SAMPLE_DURATION_S / SAMPLE_INTERVAL_S

def w(comp, mode):
    c = COMPONENTS[comp]
    return c[mode+"_mA"] * c["voltage_V"] / 1000

def baseline_power():
    p = w("mcu","active") + w("imu","active") + w("temp_sensor","active") + w("voltage_reg","active")
    return {"mcu": round(w("mcu","active"),4), "imu": round(w("imu","active"),4),
            "temp_sensor": round(w("temp_sensor","active"),4), "vreg": round(w("voltage_reg","active"),4),
            "total_W": round(p,4), "energy_orbit_Wh": round(p*ORBIT_PERIOD_S/3600,4),
            "energy_day_Wh": round(p*ORBIT_PERIOD_S/3600*16,4)}

def optimized_power():
    dc = DUTY_CYCLE
    p_mcu  = (COMPONENTS["mcu"]["active_mA"]*dc + COMPONENTS["mcu"]["sleep_mA"]*(1-dc))*3.3/1000
    p_imu  = (COMPONENTS["imu"]["active_mA"]*dc + COMPONENTS["imu"]["sleep_mA"]*(1-dc))*3.3/1000
    p_temp = (COMPONENTS["temp_sensor"]["active_mA"]*dc + COMPONENTS["temp_sensor"]["sleep_mA"]*(1-dc))*3.3/1000
    p_vreg = w("voltage_reg","active")
    p = p_mcu + p_imu + p_temp + p_vreg
    return {"mcu": round(p_mcu,4), "imu": round(p_imu,4),
            "temp_sensor": round(p_temp,4), "vreg": round(p_vreg,4),
            "total_W": round(p,4), "energy_orbit_Wh": round(p*ORBIT_PERIOD_S/3600,4),
            "energy_day_Wh": round(p*ORBIT_PERIOD_S/3600*16,4)}

if __name__ == "__main__":
    base = baseline_power()
    opt  = optimized_power()
    reduction = (1 - opt["total_W"] / base["total_W"]) * 100
    BATTERY_Wh = 1.0
    bs_days = BATTERY_Wh / base["energy_orbit_Wh"] * ORBIT_PERIOD_S / 3600 / 24
    op_days = BATTERY_Wh / opt["energy_orbit_Wh"]  * ORBIT_PERIOD_S / 3600 / 24

    print("="*58)
    print("  CubeSat LPCU Power Comparison")
    print("="*58)
    print(f"\n{'Metric':<30} {'Baseline':>10} {'Optimized':>10}")
    print("-"*58)
    for k in ["mcu","imu","temp_sensor","vreg","total_W","energy_orbit_Wh","energy_day_Wh"]:
        print(f"  {k:<28} {base[k]:>10.4f} {opt[k]:>10.4f}")
    print(f"  {'Days on 1 Wh battery':<28} {bs_days:>10.2f} {op_days:>10.2f}")
    print("-"*58)
    print(f"\n  Power reduction:    {reduction:.1f}%")
    print(f"  Lifetime gain:      {(op_days/bs_days-1)*100:.1f}%")
    print(f"  Duty cycle:         {DUTY_CYCLE*100:.1f}%")
    print("="*58)

    results = {"baseline": base, "optimized": opt,
               "reduction_pct": round(reduction,2),
               "lifetime_gain_pct": round((op_days/bs_days-1)*100,2),
               "duty_cycle_pct": round(DUTY_CYCLE*100,2),
               "bs_days": round(bs_days,3), "op_days": round(op_days,3)}
    with open("/home/claude/cubesat_lpcu/results/power_results.json","w") as f:
        json.dump(results, f, indent=2)
    print("Saved → results/power_results.json")
