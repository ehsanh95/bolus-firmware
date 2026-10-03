#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "downlink_management_service.h"
#include "event_episode_service.h"
static bolus_runtime_config_t config;
static uint8_t response[8], response_size;
uint32_t HAL_GetTick(void) { return 0; }
static void reset(void) {
    BolusRuntimeConfig_LoadDefaults(&config);
    assert(DownlinkManagementService_Init(&config));
}
static downlink_result_t send(const uint8_t *p, uint8_t n) {
    downlink_result_t result = DownlinkManagementService_HandleFrame(p,n,response,8,&response_size);
    assert(response_size == 8 && response[0] == 0xD2 && response[1] == 1);
    assert(response[3] == result);
    assert(response[6] == (DownlinkManagementService_IsInitialized() ? (uint8_t)config.version : 0));
    return result;
}
static void commands(void) {
    /* One valid value for every defined TLV, plus wrong-length and truncation. */
    const struct { uint8_t id,len; uint32_t value; uint16_t mask; } cases[] = {
        {1,1,2,1},{2,1,1,2},{3,2,3000,4},{4,2,180,4},
        {5,4,60,0},{6,2,300,8},{7,4,60,16},{8,1,0,5},{9,1,0,8},
        {10,1,12,32},{11,1,9,32},{12,1,1,32},{13,1,1,32},
        {14,2,4000,32},{15,2,1000,32},{16,1,2,32},{17,1,5,77},
        {18,1,0,64},{19,2,4200,64},{20,2,3400,64},{21,1,7,64}
    };
    for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        uint8_t p[11]={0xD1,1,42,1,cases[i].id,cases[i].len};
        for(unsigned j=0;j<cases[i].len;j++) p[6+j]=(uint8_t)(cases[i].value>>(8*j));
        reset();
        assert(send(p,6+cases[i].len)==(cases[i].mask?0:1));
        assert(DownlinkManagementService_GetPendingApplyMask()==cases[i].mask);
        bolus_runtime_config_t accepted=config;
        assert(send(p,6+cases[i].len)==2);
        assert(memcmp(&accepted,&config,sizeof(config))==0);
        DownlinkManagementService_MarkApplyResult(cases[i].mask,false);
        assert(downlink_management_diag.failed_apply_mask==cases[i].mask);
        DownlinkManagementService_MarkApplyResult(cases[i].mask,true);
        assert(!DownlinkManagementService_GetPendingApplyMask());
        assert(!downlink_management_diag.failed_apply_mask);
        reset(); accepted=config;
        assert(send(p,5+cases[i].len)==DOWNLINK_RESULT_ERROR_LENGTH);
        assert(memcmp(&accepted,&config,sizeof(config))==0);
        p[5]++;
        assert(send(p,6+p[5])==DOWNLINK_RESULT_ERROR_LENGTH);
    }
    puts("PASS: all 21 TLVs, masks, duplicates, apply failure/recovery, lengths");
}
static void malformed(void) {
    uint8_t p[]={0xD1,1,1,2,7,4,60,0,0,0,0xFF,1,0};
    reset(); bolus_runtime_config_t before=config;
    assert(send(p,sizeof(p))==DOWNLINK_RESULT_ERROR_COMMAND);
    assert(!memcmp(&before,&config,sizeof(config)));
    assert(downlink_management_diag.last_command_index==1);
    assert(downlink_management_diag.last_parse_offset==10);
    p[0]=0; assert(send(p,sizeof(p))==DOWNLINK_RESULT_ERROR_MAGIC);
    p[0]=0xD1;p[1]=2;assert(send(p,sizeof(p))==DOWNLINK_RESULT_ERROR_VERSION);
    assert(send(NULL,10)==DOWNLINK_RESULT_ERROR_PARAM);
    assert(!downlink_management_diag.last_transaction_valid);
    assert(send(p,2)==DOWNLINK_RESULT_ERROR_LENGTH);
    uint8_t temp[]={0xD1,1,2,1,19,2,0x30,0x75}; /* 300 C */
    assert(send(temp,sizeof(temp))==DOWNLINK_RESULT_ERROR_CONFIG);
    assert(!memcmp(&before,&config,sizeof(config)));
    uint8_t invalid_bool[]={0xD1,1,2,1,8,1,2};
    assert(send(invalid_bool,sizeof(invalid_bool))==DOWNLINK_RESULT_ERROR_VALUE);
    uint8_t period[]={0xD1,1,2,1,7,4,0,0,0,0};
    assert(send(period,sizeof(period))==DOWNLINK_RESULT_ERROR_CONFIG);
    period[6]=60;assert(send(period,sizeof(period))==0);
    assert(config.radio.uplink_period_s==60);
    period[2]=3; period[6]=90;assert(send(period,sizeof(period))==0);
    assert(config.radio.uplink_period_s==90);
    assert(DownlinkManagementService_HandleFrame(period,10,response,7,&response_size)==DOWNLINK_RESULT_ERROR_PARAM);
    assert(response_size==0);
    assert(!DownlinkManagementService_Init(NULL));
    assert(send(period,10)==DOWNLINK_RESULT_ERROR_NOT_INITIALIZED);
    puts("PASS: malformed frames, atomic rollback, invalid config, retry and API guards");
}
static void fuzz(void) {
    uint32_t random=0x12345678;
    for(unsigned i=0;i<20000;i++) {
        uint8_t p[255];
        for(unsigned j=0;j<sizeof(p);j++) {random=random*1664525U+1013904223U;p[j]=(uint8_t)(random>>24);}
        unsigned n=i%256;
        if(i&1){p[0]=0xD1;p[1]=1;}
        reset();bolus_runtime_config_t before=config;
        downlink_result_t result=send(p,n);
        if(result>=0x80) assert(!memcmp(&before,&config,sizeof(config)));
        else assert(BolusRuntimeConfig_Validate(&config));
    }
    puts("PASS: 20000 deterministic malformed/random frames; rollback invariant");
}
static void episodes(void) {
    event_episode_service_t s={0};event_episode_action_t a;
    reset(); assert(EventEpisodeService_Init(&s,&config)==0);
    assert(EventEpisodeService_OnMotionPulse(&s,0,&a)==0 && a.pulse_accepted);
    EventEpisodeService_OnMotionPulse(&s,100,&a);assert(a.pulse_suppressed_by_guard && s.pulse_count==1);
    EventEpisodeService_OnMotionPulse(&s,2000,&a);assert(a.pulse_accepted && a.inter_pulse_interval_ms==2000);
    EventEpisodeService_Poll(&s,5000,&a);assert(a.take_temperature_now);
    EventEpisodeService_Poll(&s,122000,&a);assert(a.episode_closed && !s.active);
    EventEpisodeService_Init(&s,&config);
    EventEpisodeService_OnThermalAlert(&s,1000,true,false,&a);assert(s.thermal_active);
    EventEpisodeService_OnMotionPulse(&s,20000,&a);
    assert(a.take_temperature_now && a.temperature_source==EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE);
    assert(s.followup_active && s.next_followup_due_ms==25000);
    EventEpisodeService_Poll(&s,25000,&a);assert(a.take_temperature_now && s.next_followup_due_ms==35000);
    EventEpisodeService_Poll(&s,901000,&a);assert(a.episode_closed);
    EventEpisodeService_Init(&s,&config);
    EventEpisodeService_OnMotionPulse(&s,UINT32_MAX-99,&a);
    EventEpisodeService_OnMotionPulse(&s,0,&a);assert(a.pulse_suppressed_by_guard);
    EventEpisodeService_OnMotionPulse(&s,2000,&a);assert(a.pulse_accepted);
    for(unsigned level=0;level<=5;level++) {
        BolusRuntimeConfig_ApplyAcquisitionLevel(&config,level);
        assert(BolusRuntimeConfig_Validate(&config));
        EventEpisodeService_Init(&s,&config);
        EventEpisodeService_OnMotionPulse(&s,100,&a);
        assert(a.take_mpu_burst_now==(level!=0));
    }
    puts("PASS: tick-zero guard, wraparound, quiet/max close, mixed thermal/motion followups, acquisition levels");
}
int main(void) { commands(); malformed(); fuzz(); episodes(); return 0; }
