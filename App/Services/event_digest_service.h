#ifndef EVENT_DIGEST_SERVICE_H
#define EVENT_DIGEST_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
#include "event_episode_service.h"
#include "../Application/telemetry_data.h"
#define EVENT_DIGEST_CAPACITY 16U
typedef struct{bolus_event_digest_t items[EVENT_DIGEST_CAPACITY];uint8_t count;uint32_t pushed_count;uint32_t overflow_count;bool overflow_latched;} event_digest_service_t;
void EventDigestService_Init(event_digest_service_t *service);
bool EventDigestService_PushClosedEpisode(event_digest_service_t *service,const event_episode_summary_t *summary,uint32_t window_start_ms);
uint8_t EventDigestService_SnapshotAndClear(event_digest_service_t *service,bolus_event_digest_t *destination,uint8_t capacity);
bool EventDigestService_ConsumeOverflow(event_digest_service_t *service);
#endif
