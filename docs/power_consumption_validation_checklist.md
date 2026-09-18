# Bolus Power Consumption Validation Checklist

Baseline: `phase6/low-power-stop2` at commit `2561204c929fdb9011153585cd936cd10e545510`.

This checklist is the version-controlled source companion for the bench PDF. Expected values are either datasheet typical values or engineering targets; they are not all guaranteed device limits.

## Hardware covered

| Ref / Rail | Device | Expected operating role |
|---|---|---|
| J1 | STM32L476RGT6 | MCU, STOP2, RTC/IWDG |
| AC1 | BMA456 | Always-on sentinel, Any-Motion, Step Counter, XYZ |
| U2 | TMP117 | Shutdown + one-shot temperature |
| U1 | MPU6050 | Event-driven, physically power-gated burst |
| RF | RFM95W / SX1276 | LoRaWAN EU868 Class A |
| U4 | TPS631000DRLR | Buck-boost |
| U3 | STLQ015M33R | 3.3-V low-IQ LDO |
| U5 | LM66200DRLR | Ideal-diode / power path |

## Measurement preconditions

- Disconnect ST-LINK/debugger for final STOP2 numbers.
- Remove USB/UART paths that can back-power GPIOs.
- Keep LEDs off.
- Record battery/bench voltage and ambient temperature.
- Use a low-burden DMM for static microamp measurements.
- Use a shunt + oscilloscope or power profiler for MCU wake, MPU burst, TX and RX windows.

## Expected static currents

| Device / state | Reference / target |
|---|---:|
| TPS631000 no/light-load quiescent | ~8 uA typ |
| STLQ015 no-load IQ | ~1.0 uA typ |
| STLQ015 loaded IQ | ~1.4 uA typ |
| LM66200 active IQ | ~1.32 uA typ |
| STM32L476 STOP2 + RTC | ~1.4 uA typ MCU-only |
| STM32L476 practical MCU rail in STOP2 | ~2-5 uA target before external leakage |
| STM32L476 run near 80 MHz | ~10.2 mA typ MCU; practical 8-15 mA |
| TMP117 shutdown | ~0.15-0.5 uA |
| TMP117 active conversion | ~135 uA typ, <=220 uA; about 15.5 ms for AVG1 |
| MPU6050 rail OFF | ideally <1 uA; investigate >5-10 uA |
| MPU6050 gyro+accel active | ~3.8 mA typ |
| SX1276 sleep | ~0.2 uA chip; module target <2 uA |
| SX1276 standby | ~1.6 mA |
| SX1276 LoRa RX | ~10-13 mA |
| SX1276 TX 7 dBm | ~20 mA |
| SX1276 TX 13 dBm | ~28-29 mA |
| SX1276 TX 17 dBm PA_BOOST | ~87-90 mA |
| SX1276 TX 20 dBm PA_BOOST | ~120 mA |
| SOC 100k+100k divider | 15-21 uA at 3.0-4.2 V; 18.5 uA at 3.7 V |

## BMA456 mode check

The active Step Counter path forces 12.5-Hz ODR but preserves the Bosch baseline configuration otherwise.

Record:
- `bma456_diag_odr_code`
- `bma456_diag_bandwidth_code`
- `bma456_diag_perf_mode`
- `bma456_diag_advanced_power_save`

If the accelerometer is truly in low-power mode, the Bosch table gives about **9 uA at 12.5 Hz / AVG4**. If a performance/continuous mode remains active, current can be around **150 uA**. Treat this measurement as a validation of the actual silicon state, not only the requested RuntimeConfig.

## Whole-board targets

| Test | Engineering expectation |
|---|---|
| STOP2 idle, BMA truly low-power | component floor roughly 20-30 uA; target <=40 uA; investigate >60 uA |
| STOP2 idle, BMA near performance current | roughly 160-200 uA can be plausible |
| 15-min no-event cycle, long DR0-like airtime, low-power BMA | roughly 80-150 uA average, RF dependent |
| Same cycle with BMA around 150 uA | roughly 220-300 uA average |

At DR0/SF12, a 38-byte payload can approach about 2 s time-on-air. A 20-29 mA TX pulse alone contributes about 44-64 uA to the 15-minute average.

## Dynamic captures

- [ ] One complete FPort 2 TX -> RX1 -> RX2 waveform
- [ ] One transaction where a downlink is actually received
- [ ] BMA wake while radio behavior is observed
- [ ] One MPU6050 event burst: power-on -> settle -> init/wake -> 250-ms burst -> off
- [ ] One TMP117 one-shot pulse
- [ ] Gateway/network unavailable retry test; verify bounded retry and return to idle
- [ ] 15-minute no-event integrated charge
- [ ] 15-minute event-inclusive integrated charge

## Energy worksheet

Use:

`Q(uAh) = I(uA) * t(s) / 3600`

`Iavg(uA) over 15 min = 4 * Total_Q(uAh)`

`Battery life (days) ~= Capacity(mAh) / Iavg(mA) / 24`

Derate the final battery-life estimate for temperature, self-discharge and usable capacity.

## Datasheet references

- STMicroelectronics STM32L476xx datasheet
- Bosch Sensortec BMA456 datasheet
- Texas Instruments TMP117 datasheet
- MPU-6000/MPU-6050 Product Specification Rev. 3.4
- Semtech SX1276/77/78/79 datasheet
- Texas Instruments TPS631000 datasheet
- STMicroelectronics STLQ015 datasheet
- Texas Instruments LM66200 datasheet
