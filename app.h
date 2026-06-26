/* ============================================================================
 *  app.h  —  모듈 간 공유 선언 (통합 글루)
 * ========================================================================== */
#pragma once

#include <stdbool.h>
#include "audio_config.h"
#include "fft_map.h"
#include "renderer.h"

typedef enum { AUDIO_SPECTRUM, AUDIO_TUNER } audio_mode_t;
void         audio_set_mode(audio_mode_t mode);
audio_mode_t audio_get_mode(void);
void         audio_set_viz_mode(viz_mode_t mode);

/* UI가 렌더링하는 동안 오디오 태스크가 재사용할 수 없는 일관된 스냅샷. */
typedef struct {
    float bars[VIZ_POINTS];
    float peaks[VIZ_POINTS];
    float level;
} audio_viz_snapshot_t;
void audio_viz_snapshot_get(audio_viz_snapshot_t *out);

/* 출력 뮤트 (소프트 램프는 HW RC) */
void mute_set(int on);
int  mute_get(void);

/* 화면 매니저 공개 API */
typedef enum {
    EV_UP,
    EV_DOWN,
    EV_LEFT,
    EV_RIGHT,
    EV_OK,
    EV_HOME,
    EV_HOME_HOLD,
    EV_FOOTSW,
    EV_FOOTSW_HOLD,
} ui_event_t;
void sm_init(void);
void sm_on_event(ui_event_t ev);
void sm_render(void);
int  sm_current(void);

/* 씬/템포 훅: UI 태스크 안에서만 호출 */
void  sm_load_scene(int content_idx, int theme_idx, int renderer_idx);
void  sm_load_scene_named(int content_idx, int theme_idx, const char *renderer_name);
void  sm_set_tempo(float bpm);
float sm_get_tempo(void);

/* MIDI/UART 등 비-UI 태스크가 사용하는 thread-safe 진입점. */
bool ui_post_event(ui_event_t ev);
bool ui_post_scene(int content_idx, int theme_idx, const char *renderer_name);
bool ui_post_tempo(float bpm);
bool ui_post_mute_toggle(void);

