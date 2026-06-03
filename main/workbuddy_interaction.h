#pragma once

#include <stddef.h>
#include <stdint.h>

#include "workbuddy_actions.h"
#include "workbuddy_launcher.h"

typedef struct {
    uint32_t weather_taps;
    uint32_t calendar_taps;
    uint32_t ai_taps;
    uint32_t pet_taps;
    uint32_t total_taps;
    int idle_min;
    int focus_min;
    int break_min;
    int focus_rounds;
    const char *study_state;
} workbuddy_interaction_snapshot_t;

void workbuddy_interaction_init(void);
void workbuddy_interaction_record_action(workbuddy_action_id_t action_id);
void workbuddy_interaction_record_screen(workbuddy_screen_id_t screen);
void workbuddy_interaction_get_snapshot(workbuddy_interaction_snapshot_t *snapshot);
void workbuddy_interaction_build_context(char *out, size_t out_size);
void workbuddy_interaction_status_text(char *out, size_t out_size);
