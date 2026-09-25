// Bolus Telemetry V2/V2.1/V2.2 uplink decoder for ChirpStack v4 JavaScript codec.
//
// STATUS: DECODER IMPLEMENTED AND OFFLINE VECTOR-TESTED.
// The 32-byte Telemetry V2 wire contract remains frozen and V2.1 appends a
// native BMA456 snapshot without removing any V2 field. The LoRaWAN transport
// path itself is still IMPLEMENTED / UNTESTED on
// a real gateway/network server/hardware at this project checkpoint.
//
// Paste this complete file into the ChirpStack Device Profile codec:
// Codec -> JavaScript -> Decode uplink.
//
// ChirpStack provides input.bytes and input.fPort to decodeUplink(input).
//
// Bolus application uplinks:
//   FPort 2: Telemetry V2 (32B), V2.1 (42B), or compact V2.2 (38B) summary.
//   FPort 4: Downlink ACK/NACK control response, exactly 8 bytes.
//   FPort 3: Reserved for application downlinks; it is not an uplink payload.
//
// Telemetry V2 byte 0 = high nibble protocol version, low nibble message type.
// Summary headers are 0x21 for V2, 0x31 for V2.1, and 0x41 for V2.2.
// Multi-byte values are little-endian. In legacy V2/V2.1 there is no
// application CRC in byte 31; that byte holds event/reference flags.
//
// Legacy candidate counts and event-reference flags are staging/research
// fields. V2.2 omits them.

'use strict';

function bolusU16Le(bytes, offset) {
  return (bytes[offset] | (bytes[offset + 1] << 8)) >>> 0;
}

function bolusI16Le(bytes, offset) {
  var value = bolusU16Le(bytes, offset);
  return (value & 0x8000) ? value - 0x10000 : value;
}

function bolusU32Le(bytes, offset) {
  return (bytes[offset] |
    (bytes[offset + 1] << 8) |
    (bytes[offset + 2] << 16) |
    (bytes[offset + 3] << 24)) >>> 0;
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
        fault_present: !!(status & 0x10),
        health_degraded: !!(status & 0x20),
        health_critical: !!(status & 0x40),
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

function bolusDecodeTelemetryV2_1(bytes) {
  if (!bytes || bytes.length !== 42) {
    return { error: 'Telemetry V2.1 must be exactly 42 bytes.' };
  }

  var version = (bytes[0] >> 4) & 0x0F;
  var messageType = bytes[0] & 0x0F;
  if (version !== 3) {
    return { error: 'Unsupported Telemetry V2.1 wire version: ' + version + '.' };
  }
  if (messageType !== 1) {
    return { error: 'Unsupported Telemetry V2.1 message type: ' + messageType + '.' };
  }

  /* Decode bytes 1..31 through the unchanged V2 path. */
  var v2Prefix = bytes.slice(0, 32);
  v2Prefix[0] = 0x21;
  var decoded = bolusDecodeTelemetryV2(v2Prefix);
  if (decoded.error) {
    return decoded;
  }

  decoded.data.version = 'V2.1';
  decoded.data.protocol.version = version;
  decoded.data.protocol.version_name = 'V2.1';
  decoded.data.protocol.message_name = 'summary_v2_1';
  decoded.data.protocol.payload_size_bytes = 42;
  decoded.data.bma = {
    steps: bolusU32Le(bytes, 32),
    accel_x_mg: bolusI16Le(bytes, 36),
    accel_y_mg: bolusI16Le(bytes, 38),
    accel_z_mg: bolusI16Le(bytes, 40)
  };

  return decoded;
}

function bolusDecodeTelemetryV2_2(bytes) {
  if (!bytes || bytes.length !== 38) {
    return { error: 'Telemetry V2.2 must be exactly 38 bytes.' };
  }

  var version = (bytes[0] >> 4) & 0x0F;
  var messageType = bytes[0] & 0x0F;
  if (version !== 4) {
    return { error: 'Unsupported Telemetry V2.2 wire version: ' + version + '.' };
  }
  if (messageType !== 1) {
    return { error: 'Unsupported Telemetry V2.2 message type: ' + messageType + '.' };
  }

  var status = bytes[4];

  return {
    data: {
      version: 'V2.2',
      protocol: {
        version: version,
        version_name: 'V2.2',
        message_type: messageType,
        message_name: 'summary_v2_2',
        payload_size_bytes: 38,
        sequence: bolusU16Le(bytes, 1),
        runtime_config_version: bytes[3]
      },
      status: {
        raw: bolusHex(status, 2),
        temperature_valid: !!(status & 0x01),
        motion_valid: !!(status & 0x02),
        interval_valid: !!(status & 0x04),
        mpu_valid: !!(status & 0x08),
        fault_present: !!(status & 0x10),
        health_degraded: !!(status & 0x20),
        health_critical: !!(status & 0x40),
        staging_untested: !!(status & 0x80)
      },
      battery: {
        voltage_mv: bolusU16Le(bytes, 5)
      },
      temperature: {
        current_c: bolusI16Le(bytes, 7) / 100,
        min_c: bolusI16Le(bytes, 9) / 100,
        max_c: bolusI16Le(bytes, 11) / 100,
        max_negative_excursion_c: bolusI16Le(bytes, 13) / 100
      },
      episode: {
        count: bytes[15],
        accepted_pulse_count: bytes[16],
        suppressed_pulse_count: bytes[17],
        max_pulses_per_episode: bytes[18],
        mean_inter_pulse_interval_s: bytes[19],
        std_inter_pulse_interval_s: bytes[20]
      },
      mpu: {
        successful_burst_count: bytes[21],
        mean_dynamic_accel_rms_mg: bytes[22] * 20,
        peak_dynamic_accel_mg: bytes[23] * 20,
        mean_angular_velocity_rms_dps: bytes[24] * 10,
        peak_angular_velocity_dps: bytes[25] * 10,
        max_orientation_change_deg: bytes[26] * 2,
        total_angular_motion_deg: bytes[27] * 5
      },
      bma: {
        steps: bolusU32Le(bytes, 28),
        accel_x_mg: bolusI16Le(bytes, 32),
        accel_y_mg: bolusI16Le(bytes, 34),
        accel_z_mg: bolusI16Le(bytes, 36)
      }
    }
  };
}

function bolusI8(value) {
  return (value & 0x80) ? value - 0x100 : value;
}

function bolusDecodeV3Digest(bytes, offset) {
  var flags = bytes[offset + 10];
  return {
    start_offset_s: bolusU16Le(bytes, offset),
    duration_s: bytes[offset + 2] * 5,
    pulse_count: bytes[offset + 3],
    mean_inter_pulse_interval_s: bytes[offset + 4],
    temperature_change_c: bolusI8(bytes[offset + 5]) / 10,
    rms_dynamic_accel_mg: bytes[offset + 6] * 20,
    peak_dynamic_accel_mg: bytes[offset + 7] * 20,
    peak_angular_velocity_dps: bytes[offset + 8] * 10,
    orientation_change_deg: bytes[offset + 9] * 2,
    flags: {
      raw: bolusHex(flags, 2),
      motion: !!(flags & 0x01),
      temperature_high: !!(flags & 0x02),
      temperature_low: !!(flags & 0x04),
      mpu_used: !!(flags & 0x08),
      mpu_skipped_by_policy: !!(flags & 0x10),
      temperature_valid: !!(flags & 0x20),
      mpu_failed: !!(flags & 0x40),
      carry_from_previous_window: !!(flags & 0x80)
    }
  };
}

function bolusDecodeTelemetryV3(bytes) {
  if (!bytes || bytes.length < 5) {
    return { error: 'Telemetry V3 payload is too short.' };
  }

  var version = (bytes[0] >> 4) & 0x0F;
  var messageType = bytes[0] & 0x0F;
  if (version !== 5) {
    return { error: 'Unsupported Telemetry V3 wire version: ' + version + '.' };
  }

  if (messageType === 1) {
    if (bytes.length < 18 || ((bytes.length - 18) % 11) !== 0) {
      return { error: 'Telemetry V3 summary length is invalid.' };
    }
    var status = bytes[4];
    var recordCount = (bytes.length - 18) / 11;
    if (recordCount > 3) {
      return { error: 'Telemetry V3 summary contains too many Episode records.' };
    }
    var episodes = [];
    for (var i = 0; i < recordCount; i++) {
      episodes.push(bolusDecodeV3Digest(bytes, 18 + (i * 11)));
    }
    return {
      data: {
        version: 'V3',
        protocol: {
          version: 5,
          message_type: 1,
          message_name: 'summary_v3',
          payload_size_bytes: bytes.length,
          sequence: bolusU16Le(bytes, 1),
          runtime_config_version: bytes[3]
        },
        status: {
          raw: bolusHex(status, 2),
          temperature_valid: !!(status & 0x01),
          motion_valid: !!(status & 0x02),
          event_digest_overflow: !!(status & 0x04),
          fault_present: !!(status & 0x10),
          health_degraded: !!(status & 0x20),
          health_critical: !!(status & 0x40),
          continuation_expected: !!(status & 0x80)
        },
        acquisition_level: bytes[5] & 0x07,
        custom_profile: !!(bytes[5] & 0x80),
        battery: { voltage_mv: bolusU16Le(bytes, 6) },
        temperature: {
          current_c: bolusI16Le(bytes, 8) / 100,
          min_c: bolusI16Le(bytes, 10) / 100,
          max_c: bolusI16Le(bytes, 12) / 100
        },
        activity: {
          steps_delta: bolusU16Le(bytes, 14),
          closed_episode_count: bytes[16],
          suppressed_trigger_count: bytes[17]
        },
        episodes: episodes
      }
    };
  }

  if (messageType === 2) {
    if (bytes.length < 5 || ((bytes.length - 5) % 11) !== 0) {
      return { error: 'Telemetry V3 continuation length is invalid.' };
    }
    var declaredCount = bytes[4] & 0x7F;
    var continuationCount = (bytes.length - 5) / 11;
    if (declaredCount !== continuationCount || continuationCount > 4) {
      return { error: 'Telemetry V3 continuation Episode count mismatch.' };
    }
    var continuedEpisodes = [];
    for (var j = 0; j < continuationCount; j++) {
      continuedEpisodes.push(bolusDecodeV3Digest(bytes, 5 + (j * 11)));
    }
    return {
      data: {
        version: 'V3',
        protocol: {
          version: 5,
          message_type: 2,
          message_name: 'episode_continuation_v3',
          payload_size_bytes: bytes.length,
          sequence: bolusU16Le(bytes, 1)
        },
        continuation: {
          packet_index: bytes[3],
          more: !!(bytes[4] & 0x80),
          episode_count: declaredCount
        },
        episodes: continuedEpisodes
      }
    };
  }

  return { error: 'Unsupported Telemetry V3 message type: ' + messageType + '.' };
}

function bolusDecodeTelemetry(bytes) {
  if (!bytes || bytes.length === 0) {
    return { error: 'Telemetry payload is empty.' };
  }

  var version = (bytes[0] >> 4) & 0x0F;
  if (version === 2) {
    return bolusDecodeTelemetryV2(bytes);
  }
  if (version === 3) {
    return bolusDecodeTelemetryV2_1(bytes);
  }
  if (version === 4) {
    return bolusDecodeTelemetryV2_2(bytes);
  }
  if (version === 5) {
    return bolusDecodeTelemetryV3(bytes);
  }
  return { error: 'Unsupported telemetry version: ' + version + '.' };
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
  if (mask & (1 << 6)) pending.push('TMP_SENSOR');

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
    return bolusDecodeTelemetry(bytes);
  }
  if (fPort === 4) {
    return bolusDecodeControlUplink(bytes);
  }
  return { error: 'Unsupported Bolus uplink FPort: ' + fPort + '. Expected FPort 2 (Telemetry V2/V2.1/V2.2) or FPort 4 (ACK/NACK).' };
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
