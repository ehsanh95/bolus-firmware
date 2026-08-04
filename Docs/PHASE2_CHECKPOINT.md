# Bolus Firmware - Phase 2 Checkpoint

Date: 2026-08-05
Task ID: 2A
Task: Repository and STM32CubeIDE baseline
Status: PARTIAL

## Repository

- Repository: bolus-firmware
- Branch: phase2/board-bringup
- Hardware: BOLUS REV_A
- MCU: STM32L476RGTX
- Toolchain: STM32CubeIDE / GNU Arm
- Firmware package: STM32Cube FW_L4 V1.18.0

## Completed

- Created and cloned the private GitHub repository.
- Created branch phase2/board-bringup.
- Added STM32CubeIDE .gitignore rules.
- Archived the original CubeMX file as:
  Docs/archive/bolus_rev_a_original.ioc
- Added the working CubeMX file as:
  Code.ioc
- Opened Code.ioc without firmware-package migration.
- Changed CubeMX target toolchain from EWARM V8.32 to STM32CubeIDE.
- Enabled Generate Under Root.
- Enabled generation of main.c.
- Generated the STM32CubeIDE baseline project.
- Generated Core, Drivers, linker scripts and Eclipse project files.
- Imported the project into STM32CubeIDE.
- Completed the first Debug build.

## Build Result

- Errors: 0
- Warnings: 1
- text: 18992 bytes
- data: 20 bytes
- bss: 2172 bytes
- total: 21184 bytes
- Flash baseline: 19012 bytes
- RAM baseline: 2192 bytes

## Open Issues

1. The single build warning has not yet been inspected or documented.
2. STM32CubeIDE displayed:
   Invalid Input: Must be IFileEditorInput
   This did not block the build, but must be investigated.
3. No firmware has been flashed to the physical board yet.
4. SWD, Verify, Breakpoint and NRST tests have not been performed.
5. Hardware power and current measurements have not started.
6. RTC, LSE, STOP2 and wake-up tests have not started.
7. Sensor and LoRaWAN code have not been added.

## Next Exact Action

1. Open the STM32CubeIDE Problems view.
2. Record the complete warning text.
3. Run Project > Clean and rebuild the Debug configuration.
4. Review or resolve all warnings.
5. Mark task 2A as PASS when the warning is understood.
6. Start task 2B: power, SWD, flash and debug validation.

## Scope Statement

This checkpoint contains only the generated STM32CubeIDE baseline.
It does not claim working sensors, radio, LoRaWAN, low-power operation,
watchdog, self-test or hardware validation.
