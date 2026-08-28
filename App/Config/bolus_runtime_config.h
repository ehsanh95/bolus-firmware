#ifndef BOLUS_RUNTIME_CONFIG_H
#define BOLUS_RUNTIME_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "../Application/bolus_types.h"

/*
 * Runtime configuration contract.
 *
 * Compile-time values in bolus_config.h remain hardware/bring-up defaults.
 * This structure is the application-owned configuration that can later be
 * loaded from NVM and modified through LoRaWAN downlink without changing the
 * Sensor/Radio service APIs.
 */

#define BOLUS_RUNTIME_CONFIG_VERSION  2U

typedef enum
{
    BOLUS_TEMP_STRATEGY_PERIODIC = 0,
    BOLUS_TEMP_STRATEGY_HYBRID
} bolus_temp_strategy_t;

typedef enum
{
    BOLUS_BMA_ODR_6_25_HZ = 0,
    BOLUS_BMA_ODR_12_5_HZ,
    BOLUS_BMA_ODR_25_HZ,
    BOLUS_BMA_ODR_50_HZ
} bolus_bma_odr_t;

typedef struct
{
    uint32_t sample_period_s;
    bolus_temp_strategy_t strategy;
    uint8_t averaging_samples;
    bool alert_enable;
    int16_t high_limit_centi_c;
    int16_t low_limit_centi_c;
} bolus_temperature_config_t;

typedef struct
{
    bolus_bma_odr_t odr;
    uint8_t range_g;
    uint8_t averaging_samples;

    /*
     * The BMA456 stays powered during normal low-power operation.
     * Its native Step Counter is a primary candidate gastric-movement metric.
     */
    bool step_counter_enable;
    bolus_bma_step_sensitivity_t step_sensitivity;

    bool fifo_enable;
    bool motion_interrupt_enable;
} bolus_bma_config_t;

typedef struct
{
    uint32_t scheduled_period_s;
    uint16_t burst_duration_ms;
    uint16_t sample_rate_hz;
    uint8_t accel_range_g;
    uint16_t gyro_range_dps;
    bool event_trigger_enable;
} bolus_mpu_config_t;

typedef struct
{
    uint32_t uplink_period_s;
    int8_t tx_power_dbm;
    uint8_t spreading_factor;
    uint8_t bandwidth_index;
    uint8_t coding_rate;
} bolus_radio_config_t;

typedef struct
{
    uint16_t version;
    bolus_operating_mode_t operating_mode;

    bolus_temperature_config_t temperature;
    bolus_bma_config_t bma;
    bolus_mpu_config_t mpu;
    bolus_radio_config_t radio;
} bolus_runtime_config_t;

/* Load safe development defaults. These are tunable, not production-frozen. */
void BolusRuntimeConfig_LoadDefaults(bolus_runtime_config_t *config);

/* Validate a candidate config before applying it (including future downlink). */
bool BolusRuntimeConfig_Validate(const bolus_runtime_config_t *config);

#endif /* BOLUS_RUNTIME_CONFIG_H */
