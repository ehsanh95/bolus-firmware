#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "telemetry_window_service.h"

int main(void)
{
    bolus_runtime_config_t config;
    telemetry_window_service_t service = {0};
    bolus_telemetry_summary_v2_2_t summary = {0};
    event_episode_action_t action = {0};

    BolusRuntimeConfig_LoadDefaults(&config);
    config.radio.uplink_period_s = 300U;
    assert(BolusRuntimeConfig_Validate(&config));
    assert(TelemetryWindow_Init(&service, &config, 0U) == TELEMETRY_WINDOW_OK);

    /* A thermal-only Episode must not make the window motion-valid. */
    action.episode_started = true;
    TelemetryWindow_RecordEpisodeAction(&service, &action);
    TelemetryWindow_RecordTemperature(&service, 25000);
    TelemetryWindow_RecordTemperature(&service, 25100);
    assert(service.temperature_sample_count == 2U);

    assert(TelemetryWindow_FreezeSummaryV2_2(
               &service,
               &config,
               300000U,
               3864U,
               false,
               false,
               false,
               &summary) == TELEMETRY_WINDOW_OK);
    assert(summary.v2.episode_count == 1U);
    assert(summary.v2.accepted_pulse_count == 0U);
    assert(!summary.v2.motion_valid);

    /* A real accepted motion pulse must set motion_valid. */
    action = (event_episode_action_t){0};
    action.episode_started = true;
    action.pulse_accepted = true;
    action.pulse_count = 1U;
    TelemetryWindow_RecordEpisodeAction(&service, &action);

    assert(TelemetryWindow_FreezeSummaryV2_2(
               &service,
               &config,
               600000U,
               3864U,
               false,
               false,
               false,
               &summary) == TELEMETRY_WINDOW_OK);
    assert(summary.v2.accepted_pulse_count == 1U);
    assert(summary.v2.motion_valid);

    puts("PASS: telemetry window motion-valid semantics and temperature sample count");
    return 0;
}
