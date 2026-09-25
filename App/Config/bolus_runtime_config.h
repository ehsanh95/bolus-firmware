#ifndef BOLUS_RUNTIME_CONFIG_H
#define BOLUS_RUNTIME_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#include "../Application/bolus_types.h"
#define BOLUS_RUNTIME_CONFIG_VERSION  12U
typedef enum { BOLUS_ACQUISITION_LEVEL_0=0, BOLUS_ACQUISITION_LEVEL_1, BOLUS_ACQUISITION_LEVEL_2, BOLUS_ACQUISITION_LEVEL_3, BOLUS_ACQUISITION_LEVEL_4, BOLUS_ACQUISITION_LEVEL_5 } bolus_acquisition_level_t;
typedef enum { BOLUS_TEMP_STRATEGY_PERIODIC=0, BOLUS_TEMP_STRATEGY_HYBRID } bolus_temp_strategy_t;
typedef enum { BOLUS_BMA_ODR_6_25_HZ=0, BOLUS_BMA_ODR_12_5_HZ, BOLUS_BMA_ODR_25_HZ, BOLUS_BMA_ODR_50_HZ } bolus_bma_odr_t;
typedef enum { BOLUS_BMA_EVENT_SENSITIVITY_RAW=0, BOLUS_BMA_EVENT_SENSITIVITY_VERY_LOW, BOLUS_BMA_EVENT_SENSITIVITY_LOW, BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_1, BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_2, BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_3, BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_4, BOLUS_BMA_EVENT_SENSITIVITY_OFF } bolus_bma_event_sensitivity_t;
typedef enum { BOLUS_EVENT_RULES_REFERENCE_BENCHMARK=0, BOLUS_EVENT_RULES_FIELD_CALIBRATED } bolus_event_rule_source_t;
typedef struct { uint32_t sample_period_s; bolus_temp_strategy_t strategy; uint8_t averaging_samples; bool alert_enable; int16_t high_limit_centi_c; int16_t low_limit_centi_c; uint8_t conversion_cycle; } bolus_temperature_config_t;
typedef struct { bolus_bma_odr_t odr; uint8_t range_g; uint8_t averaging_samples; bool step_counter_enable; bolus_bma_step_sensitivity_t step_sensitivity; bool fifo_enable; bool motion_interrupt_enable; } bolus_bma_config_t;
typedef struct { uint32_t scheduled_period_s; uint16_t burst_duration_ms; uint16_t sample_rate_hz; uint8_t accel_range_g; uint16_t gyro_range_dps; bool event_trigger_enable; } bolus_mpu_config_t;
typedef struct { bool interrupt_enable; uint16_t threshold_mg; uint16_t duration_ms; uint16_t cooldown_s; } bolus_bma_event_settings_t;
typedef struct {
 bool enable; bolus_event_rule_source_t rule_source;
 bolus_bma_event_sensitivity_t bma_event_sensitivity_level; uint16_t bma_event_threshold_mg; uint16_t bma_event_duration_ms; uint16_t bma_event_cooldown_s;
 uint16_t episode_quiet_timeout_s; uint16_t episode_retrigger_guard_ms; uint16_t episode_max_duration_s;
 uint16_t episode_temp_followup_1_s; uint16_t episode_temp_followup_2_s; uint16_t episode_temp_followup_3_s; uint16_t episode_temp_followup_4_s;
 uint16_t drinking_drop_5min_mdeg_c; uint16_t drinking_drop_10min_mdeg_c; int32_t drinking_absolute_temp_reference_mdeg_c;
 uint16_t contraction_duration_min_ms; uint16_t contraction_duration_max_ms; uint16_t contraction_interval_min_s; uint16_t contraction_interval_max_s;
 int32_t hyperthermia_reference_mdeg_c; int32_t sara_risk_reference_mdeg_c;
} bolus_event_processing_config_t;
typedef struct { uint32_t uplink_period_s; int8_t tx_power_dbm; uint8_t spreading_factor; uint8_t bandwidth_index; uint8_t coding_rate; uint16_t tx_timeout_ms; uint16_t retry_delay_ms; uint8_t max_tx_attempts; } bolus_radio_config_t;
typedef struct {
 uint16_t version; bolus_operating_mode_t operating_mode; bolus_acquisition_level_t acquisition_level;
 bolus_temperature_config_t temperature; bolus_bma_config_t bma; bolus_mpu_config_t mpu; bolus_event_processing_config_t event_processing; bolus_radio_config_t radio;
} bolus_runtime_config_t;
void BolusRuntimeConfig_LoadDefaults(bolus_runtime_config_t *config);
bool BolusRuntimeConfig_Validate(const bolus_runtime_config_t *config);
bool BolusRuntimeConfig_ApplyAcquisitionLevel(bolus_runtime_config_t *config, bolus_acquisition_level_t level);
bool BolusRuntimeConfig_ResolveBmaEventSettings(const bolus_runtime_config_t *config, bolus_bma_event_settings_t *settings);
#endif
