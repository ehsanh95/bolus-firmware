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

#define BOLUS_RUNTIME_CONFIG_VERSION  3U

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

typedef enum
{
    /* Published literature values are used only as research benchmarks. */
    BOLUS_EVENT_RULES_REFERENCE_BENCHMARK = 0,

    /* Values have been replaced/tuned from synchronized Bolus field data. */
    BOLUS_EVENT_RULES_FIELD_CALIBRATED
} bolus_event_rule_source_t;

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
     * Its native Step Counter is an experimental generic activity metric.
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

/*
 * Multi-timescale event-processing policy.
 *
 * IMPORTANT:
 * - bma_event_sensitivity_level is independent from BMA Step Counter
 *   sensitivity. 0 means "leave the event detector at its hardware/default
 *   development setting"; 1..7 is the future Bolus/downlink calibration
 *   scale. The BMA driver mapping is deliberately implemented separately.
 * - Published numerical values below are reference benchmarks, not universal
 *   cattle thresholds. V1 event outputs must preserve the rule_source so the
 *   backend can distinguish literature-reference matches from field-calibrated
 *   decisions.
 */
typedef struct
{
    bool enable;
    bolus_event_rule_source_t rule_source;

    uint8_t bma_event_sensitivity_level;
    uint16_t bma_event_cooldown_s;

    /* Published drinking trajectory reference values. */
    uint16_t drinking_drop_5min_mdeg_c;
    uint16_t drinking_drop_10min_mdeg_c;

    /* Direct intrareticular contraction timing evidence. */
    uint16_t contraction_duration_min_ms;
    uint16_t contraction_duration_max_ms;
    uint16_t contraction_interval_min_s;
    uint16_t contraction_interval_max_s;

    /* Cohort/study benchmark only; not a disease diagnosis threshold. */
    int32_t hyperthermia_reference_mdeg_c;
} bolus_event_processing_config_t;

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
    bolus_event_processing_config_t event_processing;
    bolus_radio_config_t radio;
} bolus_runtime_config_t;

/* Load safe development defaults. These are tunable, not production-frozen. */
void BolusRuntimeConfig_LoadDefaults(bolus_runtime_config_t *config);

/* Validate a candidate config before applying it (including future downlink). */
bool BolusRuntimeConfig_Validate(const bolus_runtime_config_t *config);

#endif /* BOLUS_RUNTIME_CONFIG_H */
