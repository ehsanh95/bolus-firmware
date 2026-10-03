# Tests

Tests that are independent of the STM32 target runtime.

| File | Purpose |
|---|---|
| `test_telemetry_codec_v2_2.c` | Verifies the Telemetry V2.2 byte layout and encoder behavior |
| `test_uplink_decoders.js` | Tests uplink decoders using fixed vectors |

The main goal is to prevent accidental changes to the wire contract between the firmware and the ChirpStack decoder.

Any telemetry-field change should be accompanied by:
1. encoder tests,
2. decoder tests,
3. compatibility checks for frozen protocol versions.

## Functional host suite

Run `bash tests/run_host_tests.sh` with GCC and Node.js. Uses UBSan; the HAL tick is a host stub, not hardware simulation. See `docs/functional_validation_2026-10-03.md` for tested scope, bench vectors, debugger expressions and remaining gates.
