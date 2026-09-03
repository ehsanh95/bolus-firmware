// Bolus Telemetry V2 uplink decoder for ChirpStack v4 JavaScript codec.
//
// STATUS: DECODER IMPLEMENTED AND OFFLINE VECTOR-TESTED.
// The 32-byte Telemetry V2 wire contract is frozen for decoder integration,
// but the LoRaWAN transport path itself is still IMPLEMENTED / UNTESTED on
// a real gateway/network server/hardware at this project checkpoint.
//
// Paste this complete file into the ChirpStack Device Profile codec:
// Codec -> JavaScript -> Decode uplink.
//
// ChirpStack provides input.bytes and input.fPort to decodeUplink(input).
//
// Bolus application uplinks:
//   FPort 2: Telemetry V2 summary, exactly 32 bytes.
//   FPort 4: Downlink ACK/NACK control response, exactly 8 bytes.
//   FPort 3: Reserved for application downlinks; it is not an uplink payload.
//
// Telemetry V2 byte 0 = high nibble protocol version, low nibble message type.
// Current summary header is 0x21 (version 2, message type 1).
// Multi-byte values are little-endian. There is no application CRC in byte 31;
// byte 31 is the low 8 bits of the event/reference flags.
//
// Candidate counts and event-reference flags are staging/research fields. They
// must not be interpreted as validated physiological diagnoses.

'use strict';

function bolusU16Le(bytes, offset) {
  return (bytes[offset] | (bytes[offset + 1] << 8)) >>> 0;
}

function bolusI16Le(bytes, offset) {
  var value = bolusU16Le(bytes, offset);
  return (value & 0x8000) ? value - 0x10000 : value;
}

function bolusHex(value, width) {
  return '0x' + (value >>> 0).toString(16).toUpperCase().padStart(width, '0');
}

function bolusDecodeTelemetryV2(bytes) {
  if (!bytes || bytes.length !== 32) {
    return { error: 'Telemetry V2 must be exactly 32 bytes.' };
  }

  var version = (bytes[0] >> 4) & 0x0F;
  var messageType = bytes[0] & 0x0F;
  if (version !== 2) {
    return { error: 'Unsupported telemetry version: ' + version + '.' };
  }
  if (messageType !== 1) {
    return { error: 'Unsupported Telemetry V2 message type: ' + messageType + '.' };
  }

  var status = bytes[4];
  var eventFlags = bytes[31];

  return {
    data: {
      protocol: {
        version: version,
        message_type: messageType,
        message_name: 'summary_v2',
        payload_size_bytes: 32,
        sequence: bolusU16Le(bytes, 1),
        runtime_config_version: bytes[3]
      },
      status: {
        raw: bolusHex(status, 2),
        temperature_valid: !!(status & 0x01),
        motion_valid: !!(status & 0x02),
        interval_valid: !!(status & 0x04),
        mpu_valid: !!(status & 0x08),
        health_fault: !!(status & 0x40),
        staging_untested: !!(status & 0x80)
      },
      battery: {
        percent: bytes[5],
        voltage_mv: bolusU16Le(bytes, 6),
        voltage_v: bolusU16Le(bytes, 6) / 1000
      },
      temperature: {
        current_centi_c: bolusI16Le(bytes, 8),
        current_c: bolusI16Le(bytes, 8) / 100,
        min_centi_c: bolusI16Le(bytes, 10),
        min_c: bolusI16Le(bytes, 10) / 100,
        max_centi_c: bolusI16Le(bytes, 12),
        max_c: bolusI16Le(bytes, 12) / 100,
        max_negative_excursion_centi_c: bolusI16Le(bytes, 14),
        max_negative_excursion_c: bolusI16Le(bytes, 14) / 100
      },
      episode: {
        count: bytes[16],
        accepted_pulse_count: bytes[17],
        suppressed_pulse_count: bytes[18],
        max_pulses_per_episode: bytes[19],
        mean_inter_pulse_interval_s: bytes[20],
        std_inter_pulse_interval_s: bytes[21]
      },
      mpu: {
        successful_burst_count: bytes[22],
        mean_dynamic_accel_rms_mg: bytes[23] * 20,
        peak_dynamic_accel_mg: bytes[24] * 20,
        mean_angular_velocity_rms_dps: bytes[25] * 10,
        peak_angular_velocity_dps: bytes[26] * 10,
        max_orientation_change_deg: bytes[27] * 2,
        total_angular_motion_deg: bytes[28] * 5
      },
      candidates: {
        contraction_count: bytes[29],
        rotation_count: bytes[30],
        note: 'Reserved/staging classifier fields; do not interpret as validated diagnoses.'
      },
      event_reference_flags: {
        raw: bolusHex(eventFlags, 2),
        drinking_reference: !!(eventFlags & 0x01),
        contraction_reference: !!(eventFlags & 0x02),
        hyperthermia_reference: !!(eventFlags & 0x04),
        sara_reference: !!(eventFlags & 0x08),
        rotation_reference: !!(eventFlags & 0x10),
        note: 'Reference/staging markers only; not validated physiological or diagnostic claims.'
      }
    }
  };
}

function bolusControlResultName(code) {
  var names = {
    0x00: 'ACCEPTED_PENDING_APPLY',
    0x01: 'ACCEPTED_NO_LIVE_RECONFIG',
    0x02: 'DUPLICATE_TRANSACTION',
    0x80: 'ERROR_PARAM',
    0x81: 'ERROR_MAGIC',
    0x82: 'ERROR_VERSION',
    0x83: 'ERROR_LENGTH',
    0x84: 'ERROR_COMMAND',
    0x85: 'ERROR_VALUE',
    0x86: 'ERROR_CONFIG',
    0x87: 'ERROR_NOT_INITIALIZED'
  };
  return names[code] || ('UNKNOWN_' + bolusHex(code, 2));
}

function bolusDecodeControlUplink(bytes) {
  if (!bytes || bytes.length !== 8) {
    return { error: 'Control ACK/NACK must be exactly 8 bytes.' };
  }
  if (bytes[0] !== 0xD2) {
    return { error: 'Invalid control response magic; expected 0xD2.' };
  }
  if (bytes[1] !== 1) {
    return { error: 'Unsupported control protocol version: ' + bytes[1] + '.' };
  }

  var mask = bolusU16Le(bytes, 4);
  var pending = [];
  if (mask & (1 << 0)) pending.push('BMA_EVENT');
  if (mask & (1 << 1)) pending.push('BMA_SENSOR');
  if (mask & (1 << 2)) pending.push('EVENT_EPISODE');
  if (mask & (1 << 3)) pending.push('MPU_SENSOR');
  if (mask & (1 << 4)) pending.push('TELEMETRY_WINDOW');
  if (mask & (1 << 5)) pending.push('RADIO_POLICY');

  return {
    data: {
      protocol: {
        version: bytes[1],
        message_name: 'downlink_ack_nack',
        payload_size_bytes: 8
      },
      transaction_id: bytes[2],
      result_code: bytes[3],
      result: bolusControlResultName(bytes[3]),
      apply_mask: {
        raw: bolusHex(mask, 4),
        pending_subsystems: pending
      },
      runtime_config_version: bolusU16Le(bytes, 6)
    }
  };
}

function bolusDecodeUplink(fPort, bytes) {
  if (fPort === 2) {
    return bolusDecodeTelemetryV2(bytes);
  }
  if (fPort === 4) {
    return bolusDecodeControlUplink(bytes);
  }
  return { error: 'Unsupported Bolus uplink FPort: ' + fPort + '. Expected FPort 2 (Telemetry V2) or FPort 4 (ACK/NACK).' };
}

// ChirpStack calls this function for every application uplink.
function decodeUplink(input) {
  var decoded = bolusDecodeUplink(input.fPort, input.bytes);
  if (decoded.error) {
    return {
      data: {},
      warnings: [],
      errors: [decoded.error]
    };
  }
  return {
    data: decoded.data,
    warnings: [],
    errors: []
  };
}
