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

#define BOLUS_RUNTIME_CONFIG_VERSION  5U

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
 * - BMA event detection is independent from the native Step Counter settings.
 * - bma_event_threshold_mg and bma_event_duration_ms are the actual hardware
 *   Any-Motion controls that a future downlink can tune. A value of 0 means
 *   "keep the Bosch feature-image value". This is the safest development
 *   default because the cattle literature does not provide a portable BMA456
 *   interrupt-amplitude threshold.
 * - bma_event_sensitivity_level remains a product/UI profile identifier for
 *   later field-calibrated mappings. Level 0 means raw/default mode.
 * - Published numerical values below are reference benchmarks, not universal
 *   cattle thresholds. V1 event outputs must preserve rule_source so the
 *   backend can distinguish literature-reference matches from field-calibrated
 *   decisions.
 */
typedef struct
{
    bool enable;
    bolus_event_rule_source_t rule_source;

    uint8_t bma_event_sensitivity_level;
    uint16_t bma_event_threshold_mg;
    uint16_t bma_event_duration_ms;
    uint16_t bma_event_cooldown_s;

    /*
     * Published drinking references.
     * - trajectory rules are the preferred evidence path;
     * - 38.1 C absolute temperature is retained only as a weaker secondary
     *   published rule and must never override trajectory evidence.
     */
    uint16_t drinking_drop_5min_mdeg_c;
    uint16_t drinking_drop_10min_mdeg_c;
    int32_t drinking_absolute_temp_reference_mdeg_c;

    /* Direct intrareticular contraction timing evidence. */
    uint16_t contraction_duration_min_ms;
    uint16_t contraction_duration_max_ms;
    uint16_t contraction_interval_min_s;
    uint16_t contraction_interval_max_s;

    /*
     * Published health-risk references only. Neither value is a diagnosis.
     * 40.0 C: cohort-level hyperthermia reference.
     * 39.4 C: reported association with time at low ruminal pH / SARA risk.
     */
    int32_t hyperthermia_reference_mdeg_c;
    int32_t sara_risk_reference_mdeg_c;
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
