#ifndef BOLUS_TYPES_H
#define BOLUS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Shared application-level types for Bolus firmware.
 *
 * Important:
 * - These types describe product/application data, not hardware registers.
 * - Drivers must not depend on this header.
 * - bolus_telemetry_t is the internal telemetry model; it is NOT the final
 *   LoRa/LoRaWAN on-air byte layout.
 */

typedef enum
{
    BOLUS_STATE_BOOT = 0,
    BOLUS_STATE_SELF_TEST,
    BOLUS_STATE_ACQUIRE,
    BOLUS_STATE_PROCESS,
    BOLUS_STATE_TRANSMIT,
    BOLUS_STATE_SLEEP,
    BOLUS_STATE_FAULT
} bolus_state_t;

typedef enum
{
    BOLUS_MODE_NORMAL = 0,
    BOLUS_MODE_LOW_POWER,
    BOLUS_MODE_HIGH_ACTIVITY,
    BOLUS_MODE_DIAGNOSTIC,
    BOLUS_MODE_CUSTOM
} bolus_operating_mode_t;

typedef enum
{
    BOLUS_HEALTH_OK = 0,
    BOLUS_HEALTH_DEGRADED,
    BOLUS_HEALTH_CRITICAL
} bolus_health_status_t;

/* Sensor/data validity bitmap. */
typedef uint32_t bolus_validity_flags_t;

#define BOLUS_VALID_TEMPERATURE      (1UL << 0)
#define BOLUS_VALID_BATTERY          (1UL << 1)
#define BOLUS_VALID_BMA_MOTION       (1UL << 2)
#define BOLUS_VALID_MPU_ORIENTATION  (1UL << 3)
#define BOLUS_VALID_MPU_ROTATION     (1UL << 4)

/* Fault masks are defined by Fault Manager; telemetry only transports them. */
typedef uint32_t bolus_fault_mask_t;

/*
 * Low-power continuous motion summary from BMA456.
 * Values are deliberately engineering-unit based so higher layers do not
 * depend on BMA456 register encoding.
 */
typedef struct
{
    int16_t accel_x_mg;
    int16_t accel_y_mg;
    int16_t accel_z_mg;

    uint16_t rms_motion_mg;
    uint16_t peak_motion_mg;
    uint16_t activity_index;
    uint16_t motion_event_count;
} bolus_bma_motion_t;

/*
 * Short high-detail motion/orientation summary from MPU6050.
 * Angle values use centi-degrees (0.01 degree) to avoid float dependency in
 * the application data contract.
 */
typedef struct
{
    int16_t roll_cdeg;
    int16_t pitch_cdeg;

    uint16_t angular_motion_cdeg;
    uint16_t max_gyro_dps;
} bolus_mpu_motion_t;

/* One application acquisition result. */
typedef struct
{
    int32_t temperature_mdeg_c;

    /*
     * Battery voltage and estimated state-of-charge are both telemetry data.
     * battery_percent is an estimate and depends on the active battery model.
     */
    uint16_t battery_mv;
    uint8_t battery_percent;

    bolus_bma_motion_t bma;
    bolus_mpu_motion_t mpu;

    bolus_validity_flags_t validity;
    uint32_t timestamp_s;
} bolus_measurement_t;

/* Internal telemetry model. Packet serialization is a separate responsibility. */
typedef struct
{
    uint16_t config_version;
    bolus_operating_mode_t operating_mode;
    bolus_health_status_t health;

    bolus_measurement_t measurement;

    bolus_fault_mask_t active_faults;
    bolus_fault_mask_t fault_history;
    uint8_t last_fault_code;
} bolus_telemetry_t;

#endif /* BOLUS_TYPES_H */
