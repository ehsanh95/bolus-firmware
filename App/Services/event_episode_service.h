#ifndef EVENT_EPISODE_SERVICE_H
#define EVENT_EPISODE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "../Config/bolus_runtime_config.h"

#define EVENT_EPISODE_TEMP_FOLLOWUP_COUNT  4U

typedef enum
{
    EVENT_EPISODE_OK = 0,
    EVENT_EPISODE_ERROR_PARAM,
    EVENT_EPISODE_ERROR_CONFIG,
    EVENT_EPISODE_ERROR_NOT_INITIALIZED
} event_episode_status_t;

typedef enum
{
    EVENT_EPISODE_TEMP_SOURCE_NONE = 0,
    EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE,
    EVENT_EPISODE_TEMP_SOURCE_FOLLOWUP,
    EVENT_EPISODE_TEMP_SOURCE_MOTION_PULSE
} event_episode_temperature_source_t;

/* Sensor-agnostic compact MPU result recorded for one accepted motion pulse. */
typedef struct
{
    uint16_t sample_count;
    uint16_t peak_dynamic_accel_mg;
    uint16_t rms_dynamic_accel_mg;
    uint16_t peak_angular_velocity_dps;
    uint16_t rms_angular_velocity_dps;
    uint32_t total_angular_motion_cdeg;
    bool orientation_change_valid;
    uint16_t orientation_change_cdeg;
} event_episode_mpu_features_t;

/*
 * One non-blocking action emitted by the episode state machine.
 * The service never reads a sensor and never sleeps the MCU itself.
 */
typedef struct
{
    bool pulse_accepted;
    bool pulse_suppressed_by_guard;
    bool episode_started;
    bool episode_closed;
    bool followup_schedule_cancelled;

    bool take_temperature_now;
    event_episode_temperature_source_t temperature_source;

    /* Version-10: every accepted pulse requests one short power-gated MPU burst. */
    bool take_mpu_burst_now;

    bool inter_pulse_interval_valid;
    uint32_t inter_pulse_interval_ms;

    uint32_t episode_sequence;
    uint16_t pulse_count;
} event_episode_action_t;

/*
 * Compact episode summary. It intentionally keeps statistics, not an unbounded
 * list of raw samples. Per-pulse detailed records remain a separate contract.
 */
typedef struct
{
    uint32_t episode_sequence;
    uint32_t start_time_ms;
    uint32_t last_pulse_time_ms;
    uint32_t close_deadline_ms;
    uint32_t active_span_ms;

    uint16_t pulse_count;
    uint16_t suppressed_retrigger_count;

    uint16_t temperature_sample_count;
    uint16_t pulse_temperature_count;
    uint16_t followup_temperature_count;
    uint16_t missed_followup_count;

    bool pulse_driven_temperature;
    bool temperature_valid;
    int32_t temperature_first_mdeg_c;
    int32_t temperature_current_mdeg_c;
    int32_t temperature_min_mdeg_c;
    int32_t temperature_max_mdeg_c;
    int32_t max_negative_excursion_mdeg_c;

    /* Aggregated MPU detail from accepted pulses in this episode. */
    uint16_t mpu_burst_count;
    uint32_t mpu_sample_count;
    uint16_t mpu_peak_dynamic_accel_mg;
    uint16_t mpu_mean_rms_dynamic_accel_mg;
    uint16_t mpu_peak_angular_velocity_dps;
    uint16_t mpu_mean_rms_angular_velocity_dps;
    uint32_t mpu_total_angular_motion_cdeg;
    uint16_t mpu_orientation_valid_count;
    uint16_t mpu_max_orientation_change_cdeg;
} event_episode_summary_t;

typedef struct
{
    bool initialized;
    bool active;

    uint32_t quiet_timeout_ms;
    uint32_t retrigger_guard_ms;
    uint32_t temp_followup_offset_ms[EVENT_EPISODE_TEMP_FOLLOWUP_COUNT];

    uint32_t sequence_counter;
    uint32_t active_sequence;
    uint32_t start_time_ms;
    uint32_t last_pulse_time_ms;
    uint32_t close_deadline_ms;

    uint16_t pulse_count;
    uint16_t suppressed_retrigger_count;

    bool followup_active;
    uint8_t next_followup_index;
    uint32_t next_followup_due_ms;
    bool pulse_driven_temperature;

    uint16_t temperature_sample_count;
    uint16_t pulse_temperature_count;
    uint16_t followup_temperature_count;
    uint16_t missed_followup_count;

    bool temperature_valid;
    int32_t temperature_first_mdeg_c;
    int32_t temperature_current_mdeg_c;
    int32_t temperature_min_mdeg_c;
    int32_t temperature_max_mdeg_c;
    int32_t max_negative_excursion_mdeg_c;

    uint16_t mpu_burst_count;
    uint32_t mpu_sample_count;
    uint16_t mpu_peak_dynamic_accel_mg;
    uint64_t mpu_rms_dynamic_accel_sum_mg;
    uint16_t mpu_peak_angular_velocity_dps;
    uint64_t mpu_rms_angular_velocity_sum_dps;
    uint64_t mpu_total_angular_motion_cdeg;
    uint16_t mpu_orientation_valid_count;
    uint16_t mpu_max_orientation_change_cdeg;

    bool last_closed_summary_valid;
    event_episode_summary_t last_closed_summary;
} event_episode_service_t;

/* Initialize from RuntimeConfig; no HAL/timebase dependency is retained. */
event_episode_status_t EventEpisodeService_Init(
    event_episode_service_t *service,
    const bolus_runtime_config_t *config);

/*
 * Feed one BMA motion pulse with an externally supplied monotonic timestamp.
 * During bench integration this may be HAL_GetTick(). Production STOP2 code
 * must supply an RTC/LPTIM-backed timebase that continues across deep sleep.
 */
event_episode_status_t EventEpisodeService_OnMotionPulse(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_action_t *action);

/*
 * Poll scheduled work at the current monotonic time. Returns at most one sensor
 * action per call. Missed historical follow-up slots are skipped rather than
 * causing a burst of back-to-back TMP conversions.
 */
event_episode_status_t EventEpisodeService_Poll(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_action_t *action);

/* Record acquisition results requested by the returned action. */
event_episode_status_t EventEpisodeService_RecordTemperature(
    event_episode_service_t *service,
    event_episode_temperature_source_t source,
    int32_t temperature_mdeg_c);

event_episode_status_t EventEpisodeService_RecordMpuBurst(
    event_episode_service_t *service,
    const event_episode_mpu_features_t *features);

bool EventEpisodeService_IsActive(const event_episode_service_t *service);

/* Copy the current active summary or the most recently closed summary. */
event_episode_status_t EventEpisodeService_GetActiveSummary(
    const event_episode_service_t *service,
    event_episode_summary_t *summary);

event_episode_status_t EventEpisodeService_GetLastClosedSummary(
    const event_episode_service_t *service,
    event_episode_summary_t *summary);

#endif /* EVENT_EPISODE_SERVICE_H */
