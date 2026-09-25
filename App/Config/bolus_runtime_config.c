#include "bolus_runtime_config.h"
#include <string.h>
static bool IsPowerOfTwo(uint8_t v){return v && ((v&(uint8_t)(v-1U))==0U);}
static bool IsValidatedBmaStepProfile(bolus_bma_step_sensitivity_t p){return p==BOLUS_BMA_STEP_SENSITIVITY_DEFAULT||p==BOLUS_BMA_STEP_SENSITIVITY_LEVEL_1||p==BOLUS_BMA_STEP_SENSITIVITY_LEVEL_7;}
static bool IsSupportedMpuSampleRate(uint16_t r){if(r<10U||r>1000U||(1000U%r)!=0U)return false;return ((1000U/r)-1U)<=255U;}
bool BolusRuntimeConfig_ApplyAcquisitionLevel(bolus_runtime_config_t *c, bolus_acquisition_level_t l){
 if(!c||l>BOLUS_ACQUISITION_LEVEL_5)return false; c->acquisition_level=l;
 c->operating_mode=(l<=BOLUS_ACQUISITION_LEVEL_2)?BOLUS_MODE_LOW_POWER:
                   (l>=BOLUS_ACQUISITION_LEVEL_4)?BOLUS_MODE_HIGH_ACTIVITY:
                                                  BOLUS_MODE_NORMAL;
 c->event_processing.bma_event_threshold_mg=0U;c->event_processing.bma_event_duration_ms=0U;c->event_processing.bma_event_cooldown_s=0U;
 switch(l){
 case BOLUS_ACQUISITION_LEVEL_0:c->temperature.strategy=BOLUS_TEMP_STRATEGY_PERIODIC;c->temperature.alert_enable=false;c->temperature.conversion_cycle=7U;c->bma.motion_interrupt_enable=false;c->event_processing.enable=false;c->event_processing.bma_event_sensitivity_level=BOLUS_BMA_EVENT_SENSITIVITY_OFF;c->mpu.event_trigger_enable=false;break;
 case BOLUS_ACQUISITION_LEVEL_1:c->temperature.strategy=BOLUS_TEMP_STRATEGY_HYBRID;c->temperature.alert_enable=true;c->temperature.conversion_cycle=7U;c->bma.motion_interrupt_enable=true;c->event_processing.enable=true;c->event_processing.bma_event_sensitivity_level=BOLUS_BMA_EVENT_SENSITIVITY_VERY_LOW;c->mpu.event_trigger_enable=true;break;
 case BOLUS_ACQUISITION_LEVEL_2:c->temperature.strategy=BOLUS_TEMP_STRATEGY_HYBRID;c->temperature.alert_enable=true;c->temperature.conversion_cycle=7U;c->bma.motion_interrupt_enable=true;c->event_processing.enable=true;c->event_processing.bma_event_sensitivity_level=BOLUS_BMA_EVENT_SENSITIVITY_LOW;c->mpu.event_trigger_enable=true;break;
 case BOLUS_ACQUISITION_LEVEL_3:c->temperature.strategy=BOLUS_TEMP_STRATEGY_HYBRID;c->temperature.alert_enable=true;c->temperature.conversion_cycle=6U;c->bma.motion_interrupt_enable=true;c->event_processing.enable=true;c->event_processing.bma_event_sensitivity_level=BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_2;c->mpu.event_trigger_enable=true;break;
 case BOLUS_ACQUISITION_LEVEL_4:c->temperature.strategy=BOLUS_TEMP_STRATEGY_HYBRID;c->temperature.alert_enable=true;c->temperature.conversion_cycle=5U;c->bma.motion_interrupt_enable=true;c->event_processing.enable=true;c->event_processing.bma_event_sensitivity_level=BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_3;c->mpu.event_trigger_enable=true;break;
 case BOLUS_ACQUISITION_LEVEL_5:c->temperature.strategy=BOLUS_TEMP_STRATEGY_HYBRID;c->temperature.alert_enable=true;c->temperature.conversion_cycle=4U;c->bma.motion_interrupt_enable=true;c->event_processing.enable=true;c->event_processing.bma_event_sensitivity_level=BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_4;c->mpu.event_trigger_enable=true;break;
 default:return false;} return true;
}
void BolusRuntimeConfig_LoadDefaults(bolus_runtime_config_t *c){
 if(!c)return; memset(c,0,sizeof(*c)); c->version=BOLUS_RUNTIME_CONFIG_VERSION;c->operating_mode=BOLUS_MODE_NORMAL;
 c->temperature.sample_period_s=600U;c->temperature.averaging_samples=1U;c->temperature.high_limit_centi_c=4100;c->temperature.low_limit_centi_c=3500;
 c->bma.odr=BOLUS_BMA_ODR_12_5_HZ;c->bma.range_g=4U;c->bma.averaging_samples=4U;c->bma.step_counter_enable=true;c->bma.step_sensitivity=BOLUS_BMA_STEP_SENSITIVITY_DEFAULT;c->bma.fifo_enable=true;
 c->mpu.scheduled_period_s=900U;c->mpu.burst_duration_ms=250U;c->mpu.sample_rate_hz=100U;c->mpu.accel_range_g=4U;c->mpu.gyro_range_dps=500U;
 c->event_processing.rule_source=BOLUS_EVENT_RULES_REFERENCE_BENCHMARK;c->event_processing.episode_quiet_timeout_s=120U;c->event_processing.episode_retrigger_guard_ms=2000U;c->event_processing.episode_max_duration_s=900U;
 c->event_processing.episode_temp_followup_1_s=5U;c->event_processing.episode_temp_followup_2_s=15U;c->event_processing.episode_temp_followup_3_s=35U;c->event_processing.episode_temp_followup_4_s=65U;
 c->event_processing.drinking_drop_5min_mdeg_c=500U;c->event_processing.drinking_drop_10min_mdeg_c=500U;c->event_processing.drinking_absolute_temp_reference_mdeg_c=38100L;
 c->event_processing.contraction_duration_min_ms=8000U;c->event_processing.contraction_duration_max_ms=10000U;c->event_processing.contraction_interval_min_s=40U;c->event_processing.contraction_interval_max_s=60U;c->event_processing.hyperthermia_reference_mdeg_c=40000L;c->event_processing.sara_risk_reference_mdeg_c=39400L;
 c->radio.uplink_period_s=900U;c->radio.tx_power_dbm=10;c->radio.spreading_factor=7U;c->radio.bandwidth_index=0U;c->radio.coding_rate=1U;c->radio.tx_timeout_ms=3000U;c->radio.retry_delay_ms=2000U;c->radio.max_tx_attempts=3U;
 (void)BolusRuntimeConfig_ApplyAcquisitionLevel(c,BOLUS_ACQUISITION_LEVEL_3);
}
bool BolusRuntimeConfig_Validate(const bolus_runtime_config_t *c){
 uint32_t q;
 if(!c||c->version!=BOLUS_RUNTIME_CONFIG_VERSION||c->operating_mode>BOLUS_MODE_CUSTOM||c->acquisition_level>BOLUS_ACQUISITION_LEVEL_5)return false;
 if(!c->temperature.sample_period_s||c->temperature.sample_period_s>86400U||c->temperature.strategy>BOLUS_TEMP_STRATEGY_HYBRID||c->temperature.conversion_cycle>7U)return false;
 if(c->temperature.averaging_samples!=1U&&c->temperature.averaging_samples!=8U&&c->temperature.averaging_samples!=32U&&c->temperature.averaging_samples!=64U)return false;
 if(c->temperature.high_limit_centi_c<=c->temperature.low_limit_centi_c)return false;
 if(c->bma.odr>BOLUS_BMA_ODR_50_HZ||(c->bma.range_g!=2U&&c->bma.range_g!=4U&&c->bma.range_g!=8U&&c->bma.range_g!=16U)||!IsPowerOfTwo(c->bma.averaging_samples)||c->bma.averaging_samples>64U||!IsValidatedBmaStepProfile(c->bma.step_sensitivity))return false;
 if(c->mpu.burst_duration_ms<100U||c->mpu.burst_duration_ms>500U||!IsSupportedMpuSampleRate(c->mpu.sample_rate_hz))return false;
 if((c->mpu.accel_range_g!=2U&&c->mpu.accel_range_g!=4U&&c->mpu.accel_range_g!=8U&&c->mpu.accel_range_g!=16U)||(c->mpu.gyro_range_dps!=250U&&c->mpu.gyro_range_dps!=500U&&c->mpu.gyro_range_dps!=1000U&&c->mpu.gyro_range_dps!=2000U))return false;
 if(c->event_processing.rule_source>BOLUS_EVENT_RULES_FIELD_CALIBRATED||c->event_processing.bma_event_sensitivity_level>BOLUS_BMA_EVENT_SENSITIVITY_OFF)return false;
 if(c->event_processing.bma_event_sensitivity_level!=BOLUS_BMA_EVENT_SENSITIVITY_RAW&&(c->event_processing.bma_event_threshold_mg||c->event_processing.bma_event_duration_ms||c->event_processing.bma_event_cooldown_s))return false;
 if(c->event_processing.bma_event_threshold_mg>1000U||(c->event_processing.bma_event_duration_ms&&((c->event_processing.bma_event_duration_ms%20U)||c->event_processing.bma_event_duration_ms>60000U))||c->event_processing.bma_event_cooldown_s>3600U)return false;
 if(!c->event_processing.episode_quiet_timeout_s||c->event_processing.episode_quiet_timeout_s>3600U||!c->event_processing.episode_max_duration_s||c->event_processing.episode_max_duration_s>3600U||c->event_processing.episode_max_duration_s<c->event_processing.episode_quiet_timeout_s)return false;
 q=(uint32_t)c->event_processing.episode_quiet_timeout_s*1000UL;if(c->event_processing.episode_retrigger_guard_ms>60000U||(uint32_t)c->event_processing.episode_retrigger_guard_ms>=q)return false;
 if(!c->event_processing.episode_temp_followup_1_s||c->event_processing.episode_temp_followup_1_s>=c->event_processing.episode_temp_followup_2_s||c->event_processing.episode_temp_followup_2_s>=c->event_processing.episode_temp_followup_3_s||c->event_processing.episode_temp_followup_3_s>=c->event_processing.episode_temp_followup_4_s||c->event_processing.episode_temp_followup_4_s>=c->event_processing.episode_quiet_timeout_s)return false;
 if(!c->event_processing.drinking_drop_5min_mdeg_c||c->event_processing.drinking_drop_5min_mdeg_c>10000U||!c->event_processing.drinking_drop_10min_mdeg_c||c->event_processing.drinking_drop_10min_mdeg_c>10000U)return false;
 if(c->event_processing.drinking_absolute_temp_reference_mdeg_c< -55000L||c->event_processing.drinking_absolute_temp_reference_mdeg_c>150000L)return false;
 if(!c->event_processing.contraction_duration_min_ms||c->event_processing.contraction_duration_min_ms>=c->event_processing.contraction_duration_max_ms||c->event_processing.contraction_duration_max_ms>30000U)return false;
 if(!c->event_processing.contraction_interval_min_s||c->event_processing.contraction_interval_min_s>=c->event_processing.contraction_interval_max_s||c->event_processing.contraction_interval_max_s>600U)return false;
 if(c->event_processing.hyperthermia_reference_mdeg_c< -55000L||c->event_processing.hyperthermia_reference_mdeg_c>150000L||c->event_processing.sara_risk_reference_mdeg_c< -55000L||c->event_processing.sara_risk_reference_mdeg_c>150000L)return false;
 if(!c->radio.uplink_period_s||c->radio.uplink_period_s>86400U||
    c->radio.tx_power_dbm<2||c->radio.tx_power_dbm>16||
    ((c->radio.tx_power_dbm&1)!=0)||
    c->radio.spreading_factor<7U||c->radio.spreading_factor>12U||
    c->radio.bandwidth_index>1U||
    (c->radio.bandwidth_index==1U&&c->radio.spreading_factor!=7U)||
    c->radio.coding_rate!=1U||
    c->radio.tx_timeout_ms<500U||c->radio.tx_timeout_ms>10000U||
    c->radio.retry_delay_ms>60000U||
    !c->radio.max_tx_attempts||c->radio.max_tx_attempts>5U)return false;
 return true;
}
bool BolusRuntimeConfig_ResolveBmaEventSettings(const bolus_runtime_config_t *c, bolus_bma_event_settings_t *s){
 if(!c||!s||!BolusRuntimeConfig_Validate(c))return false;s->interrupt_enable=true;s->threshold_mg=0U;s->duration_ms=0U;s->cooldown_s=0U;
 switch(c->event_processing.bma_event_sensitivity_level){
 case BOLUS_BMA_EVENT_SENSITIVITY_RAW:s->threshold_mg=c->event_processing.bma_event_threshold_mg;s->duration_ms=c->event_processing.bma_event_duration_ms;s->cooldown_s=c->event_processing.bma_event_cooldown_s;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_VERY_LOW:s->threshold_mg=900U;s->duration_ms=800U;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_LOW:s->threshold_mg=750U;s->duration_ms=600U;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_1:s->threshold_mg=600U;s->duration_ms=500U;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_2:s->threshold_mg=500U;s->duration_ms=400U;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_3:s->threshold_mg=400U;s->duration_ms=300U;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_LEVEL_4:s->threshold_mg=300U;s->duration_ms=200U;break;
 case BOLUS_BMA_EVENT_SENSITIVITY_OFF:s->interrupt_enable=false;break; default:return false;}
 if(!c->bma.motion_interrupt_enable||!c->event_processing.enable)s->interrupt_enable=false;return true;
}
