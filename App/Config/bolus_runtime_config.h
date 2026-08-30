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

#define BOLUS_RUNTIME_CONFIG_VERSION  7U

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

/*
 * Independent BMA456 Any-Motion sensitivity profiles.
 *
 * These are engineering/bench sweep profiles, NOT validated cattle
 * thresholds. Sensitivity increases in this order:
 * VERY_LOW -> LOW -> LEVEL_1 -> LEVEL_2 -> LEVEL_3 -> LEVEL_4.
 * RAW keeps direct threshold/duration/cooldown control for field calibration.
 * OFF disables Any-Motion events while leaving normal scheduled sensing intact.
 *
 * This type is intentionally separate from bolus_bma_step_sensitivity_t.
 */
typedef enum
{
    BOLUS_BMA_EVENT_SENSITIVITY_RAW = 0,
    BOLUS_BMA_EVENT_SENSITIVITY_VERY_LOW,
    BOLUS_BMA_EVENT_SENSITIVITY_LOW,
    BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_1,
    BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_2,
    BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_3,
    BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_4,
    BOLUS_BMA_EVENT_SENSITIVITY_OFF
} bolus_bma_event_sensitivity_t;

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

/* Effective BMA Any-Motion settings after resolving RAW/profile mode. */
typedef struct
{
    bool interrupt_enable;
    uint16_t threshold_mg;
    uint16_t duration_ms;
    uint16_t cooldown_s;
} bolus_bma_event_settings_t;

/*
 * Multi-timescale event-processing policy.
 *
 * IMPORTANT:
 * - BMA event detection is independent from the native Step Counter settings.
 * - In RAW mode, bma_event_threshold_mg, bma_event_duration_ms and
 *   bma_event_cooldown_s are used directly. A zero threshold or duration means
 *   "keep the Bosch feature-image value"; zero cooldown disables suppression.
 * - Bundled profile modes require the three raw fields to remain zero. This
 *   avoids ambiguous partial overrides and gives future downlink one clear
 *   profile selector.
 * - OFF disables BMA Any-Motion event generation only. It does not disable the
 *   BMA sensor or the normal scheduled acquisition path.
 * - Published numerical values below are reference benchmarks, not universal
 *   cattle thresholds. V1 event outputs must preserve rule_source so the
 *   backend can distinguish literature-reference matches from field-calibrated
 *   decisions.
 */
typedef struct
{
    bool enable;
    bolus_event_rule_source_t rule_source;

    bolus_bma_event_sensitivity_t bma_event_sensitivity_level;
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

/*
 * Resolve the active BMA Any-Motion hardware/cooldown settings. The returned
 * values are bench/engineering settings until rule_source becomes explicitly
 * field-calibrated from synchronized animal data.
 */
bool BolusRuntimeConfig_ResolveBmaEventSettings(
    const bolus_runtime_config_t *config,
    bolus_bma_event_settings_t *settings);

#endif /* BOLUS_RUNTIME_CONFIG_H */
