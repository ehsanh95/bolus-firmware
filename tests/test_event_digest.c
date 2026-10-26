#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "event_digest_service.h"

int main(void)
{
    event_digest_service_t service;
    event_episode_summary_t summary;

    EventDigestService_Init(&service);
    memset(&summary, 0, sizeof(summary));
    summary.start_time_ms = 1000U;
    summary.active_span_ms = 1000U;
    summary.close_reason = EVENT_EPISODE_CLOSE_TELEMETRY_BOUNDARY;

    for (uint32_t i = 0U; i < EVENT_DIGEST_CAPACITY; i++)
    {
        summary.start_time_ms = 1000U + (i * 1000U);
        assert(EventDigestService_PushClosedEpisode(&service, &summary, 0U));
    }

    assert(!EventDigestService_PushClosedEpisode(&service, &summary, 0U));
    assert(!EventDigestService_PushClosedEpisode(&service, &summary, 0U));
    assert(EventDigestService_ConsumeOverflow(&service));
    assert(EventDigestService_ConsumeDroppedCount(&service) == 2U);
    assert(!EventDigestService_ConsumeOverflow(&service));
    assert(EventDigestService_ConsumeDroppedCount(&service) == 0U);

    puts("PASS: Episode digest overflow and dropped-count accounting");
    return 0;
}
