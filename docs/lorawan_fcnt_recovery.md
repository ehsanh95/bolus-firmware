# Bolus LoRaWAN: FCnt durability and autonomous recovery (experimental)

This is an **unvalidated firmware change** for STM32L476RG (1 MiB Flash),
SX1276 / RFM95W, EU868, Class A, ABP / LoRaWAN 1.0.3. Do **not** deploy to
unattended devices until the hardware checklist below is completed.

## What changed

1. The final 4 KiB of internal Flash (`0x080FF000..0x080FFFFF`, bank 2 pages
   254 and 255) are reserved in `STM32L476RGTX_FLASH.ld`. Application code is
   limited to 1020 KiB. The Flash backend refuses any MCU reporting a Flash
   size other than 1024 KiB.
2. `fcnt_journal` stores a versioned, CRC-checked, committed **uplink counter
   high-water mark** redundantly in two Flash pages. Each record consumes 32
   bytes. The next 32 frame counters are durably reserved **before** the first
   corresponding `LoRaMacMcpsRequest()` can transmit. Normal uplinks incur no
   Flash write until the next block. If reset occurs, the MAC resumes at the
   high-water mark + 1; unused counters are intentionally skipped.
3. A failed / ambiguous Flash write, a corrupt first journal record, or a
   hardware density mismatch **blocks transmission** instead of resetting
   `FCntUp` to 1. Do not clear the journal to "repair" a deployed ABP device
   unless the network activation is re-provisioned as well.
4. Repeated `BUSY`/other request errors now trigger a soft MAC recovery after
   60 seconds; `NO_NETWORK_JOINED` also enters this recovery path. Three
   unsuccessful recovery attempts without a successful MCPS confirm trigger a
   controlled MCU reset. A 15-second in-flight timeout also contributes to
   that threshold. Consecutive MCU resets are capped at three with RTC backup
   register `BKP0R`, preventing a boot loop. `BKP1R = 0x4D414353` marks a
   software reset initiated by this MAC recovery path. A successful MCPS
   confirm clears the reset budget.
5. `STOP2` is refused when the IWDG option byte says the hardware watchdog
   **freezes in STOP**. This guards against a permanent sleep if RTC wakeup
   fails but **can significantly increase battery consumption** until the
   option byte is configured to keep IWDG running in STOP2. The existing
   watchdog timeout is ~32 seconds; STOP2 chunks are <20 seconds.

## What this does NOT solve

- This currently persists **FCntUp only**. FCntDown, MAC channel/ADR state,
  session keys, and queued telemetry are not yet persisted. A power reset
  reconstructs defaults and requires future downlinks to re-establish any
  dynamic EU868 channel configuration. Downlink replay hardening requires a
  separate persisted-counter/full-MAC-context design.
- A **full chip erase** by ST-Link / CubeIDE / deployment tooling erases the
  journal. Even a normal reflash may erase the reserved pages unless the
  erase mode is restricted to application sectors. Always check the
  programming tool's erase settings. Retaining FCnt across an **ordinary
  MCU reset** is supported by the code; retaining FCnt across a **mass erase**
  is physically impossible with this on-chip Flash implementation.
- An existing device with ChirpStack's server-side FCnt higher than the
  newly deployed empty journal needs a **one-time counter/session
  reconciliation**. Never intentionally roll the server counter backwards in
  a production network just to make a restarted device work.
- The journal is tied to the configured DevAddr, not to a cryptographically
  authenticated key identity. Changing ABP keys or cloning Flash across
  devices requires an explicit provisioning/migration procedure.
- RFM95W hardware defects, loose battery/NRST contacts, and failures during
  sensor initialization require separate investigation. A successful MAC
  callback is **not** a guarantee the gateway actually received an
  unconfirmed uplink.

## Hardware verification checklist before rollout

1. Confirm the exact MCU part and Flash size are STM32L476RG / 1024 KiB.
2. Read option bytes, especially `IWDG_STOP`; decide whether to enable the
   watchdog in STOP2 and measure the resulting current (do **not** modify
   option bytes automatically in a deployed device).
3. Build the Debug and Release configurations in **STM32CubeIDE** and verify
   the final linked image ends before `0x080FF000`. There is no STM32 cross
   compiler in the automated host-test environment.
4. Verify the programmer's page erase behavior preserves the reserved
   Flash pages on firmware upgrades; never mass-erase a live ABP session.
5. With a dedicated test DevAddr/session, send FCnt1..N; exercise the reset
   button, watchdog, battery removal, and power loss during each of the four
   doubleword programs and a page rotation. Confirm FCnt always increases in
   ChirpStack and no duplicate is emitted after a reset.
6. Induce MAC `BUSY` before transmission, lost TX callback, and a disabled
   gateway. Check that only **local MAC failure** triggers resets; gateway
   absence alone must not cause a reboot loop.
7. Leave the unit motionless for 12–24 hours while battery current, RTC
   wakeups, IWDG reset flags and actual uplinks are recorded.
8. Inspect debugger variables `lorawan_uplink_service_diag` (new counters),
   `reset_flags`, `low_power_reject_unprotected_stop_count`, and RTC backup
   registers after a test; do not attach a debugger in a way that resets the
   MCU before collecting its reset reason.

Host tests: `tests/test_fcnt_journal.c` simulates power loss during each Flash
program stage, page rotation, reboot and corrupt/foreign journal data. The
hardware Flash adapter and real LoRaMAC integration require bench validation.
