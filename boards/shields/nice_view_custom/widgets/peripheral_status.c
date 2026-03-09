#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <lvgl.h>

#include "peripheral_status.h"

// ---------------------------------------------------------------------------
// Frame declarations
// ---------------------------------------------------------------------------
LV_IMG_DECLARE(beasthead_001); LV_IMG_DECLARE(beasthead_002);
LV_IMG_DECLARE(beasthead_003); LV_IMG_DECLARE(beasthead_004);
LV_IMG_DECLARE(beasthead_005); LV_IMG_DECLARE(beasthead_006);
LV_IMG_DECLARE(beasthead_007); LV_IMG_DECLARE(beasthead_008);
LV_IMG_DECLARE(beasthead_009); LV_IMG_DECLARE(beasthead_010);
LV_IMG_DECLARE(beasthead_011); LV_IMG_DECLARE(beasthead_012);
LV_IMG_DECLARE(beasthead_013); LV_IMG_DECLARE(beasthead_014);
LV_IMG_DECLARE(beasthead_015); LV_IMG_DECLARE(beasthead_016);
LV_IMG_DECLARE(beasthead_017);
LV_IMG_DECLARE(beasthead_018); LV_IMG_DECLARE(beasthead_019);
LV_IMG_DECLARE(beasthead_020);
LV_IMG_DECLARE(beasthead_021); LV_IMG_DECLARE(beasthead_022);
LV_IMG_DECLARE(beasthead_023); LV_IMG_DECLARE(beasthead_024);
LV_IMG_DECLARE(beasthead_025); LV_IMG_DECLARE(beasthead_026);
LV_IMG_DECLARE(beasthead_027);

// ---------------------------------------------------------------------------
// Frame arrays per section
// ---------------------------------------------------------------------------
static const lv_img_dsc_t *intro_frames[] = {
    &beasthead_001, &beasthead_002, &beasthead_003, &beasthead_004,
    &beasthead_005, &beasthead_006, &beasthead_007, &beasthead_008,
    &beasthead_009, &beasthead_010, &beasthead_011, &beasthead_012,
    &beasthead_013, &beasthead_014, &beasthead_015, &beasthead_016,
};
#define INTRO_FRAME_COUNT 16

static const lv_img_dsc_t *blink_frames[] = {
    &beasthead_018, &beasthead_019, &beasthead_020,
};
#define BLINK_FRAME_COUNT 3

static const lv_img_dsc_t *outro_frames[] = {
    &beasthead_021, &beasthead_022, &beasthead_023, &beasthead_024,
    &beasthead_025, &beasthead_026, &beasthead_027,
};
#define OUTRO_FRAME_COUNT 7

// ---------------------------------------------------------------------------
// Timing constants (ms)
// ---------------------------------------------------------------------------
#define FRAME_DURATION_MS     200
#define BLINK_INTERVAL_MIN_MS 3000
#define BLINK_INTERVAL_MAX_MS 5000
#define OUTRO_INTERVAL_MIN_MS 10000
#define OUTRO_INTERVAL_MAX_MS 20000

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
typedef enum {
    STATE_INTRO,
    STATE_IDLE,
    STATE_BLINK,
    STATE_OUTRO,
} anim_state_t;

static anim_state_t anim_state = STATE_INTRO;
static int          current_frame = 0;
static lv_obj_t    *art_img = NULL;

static struct k_timer frame_timer;
static struct k_timer blink_timer;
static struct k_timer outro_timer;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint32_t rand_range(uint32_t min_ms, uint32_t max_ms) {
    uint32_t range = max_ms - min_ms;
    return min_ms + (sys_rand32_get() % range);
}

static void show_frame(const lv_img_dsc_t *frame) {
    if (art_img) {
        lv_img_set_src(art_img, frame);
    }
}

static void schedule_blink(void) {
    uint32_t delay = rand_range(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
    k_timer_start(&blink_timer, K_MSEC(delay), K_NO_WAIT);
}

static void schedule_outro(void) {
    uint32_t delay = rand_range(OUTRO_INTERVAL_MIN_MS, OUTRO_INTERVAL_MAX_MS);
    k_timer_start(&outro_timer, K_MSEC(delay), K_NO_WAIT);
}

static void enter_idle(void) {
    anim_state   = STATE_IDLE;
    current_frame = 0;
    k_timer_stop(&frame_timer);
    show_frame(&beasthead_017);
    schedule_blink();
    schedule_outro();
}

// ---------------------------------------------------------------------------
// Timer callbacks (run in ISR context — keep minimal, signal via work queue)
// ---------------------------------------------------------------------------
static struct k_work anim_work;

static void frame_timer_cb(struct k_timer *t) {
    k_work_submit(&anim_work);
}
static void blink_timer_cb(struct k_timer *t) {
    if (anim_state == STATE_IDLE) {
        anim_state    = STATE_BLINK;
        current_frame = 0;
        k_timer_start(&frame_timer, K_MSEC(FRAME_DURATION_MS), K_MSEC(FRAME_DURATION_MS));
    }
}
static void outro_timer_cb(struct k_timer *t) {
    if (anim_state == STATE_IDLE) {
        k_timer_stop(&blink_timer);
        anim_state    = STATE_OUTRO;
        current_frame = 0;
        k_timer_start(&frame_timer, K_MSEC(FRAME_DURATION_MS), K_MSEC(FRAME_DURATION_MS));
    }
}

// ---------------------------------------------------------------------------
// Work handler — runs in system work queue (safe to call LVGL)
// ---------------------------------------------------------------------------
static void anim_work_handler(struct k_work *work) {
    switch (anim_state) {

    case STATE_INTRO:
        show_frame(intro_frames[current_frame]);
        current_frame++;
        if (current_frame >= INTRO_FRAME_COUNT) {
            enter_idle();
        }
        break;

    case STATE_BLINK:
        show_frame(blink_frames[current_frame]);
        current_frame++;
        if (current_frame >= BLINK_FRAME_COUNT) {
            enter_idle();
        }
        break;

    case STATE_OUTRO:
        show_frame(outro_frames[current_frame]);
        current_frame++;
        if (current_frame >= OUTRO_FRAME_COUNT) {
            // Loop — restart from intro
            k_timer_stop(&frame_timer);
            anim_state    = STATE_INTRO;
            current_frame = 0;
            k_timer_start(&frame_timer, K_MSEC(FRAME_DURATION_MS), K_MSEC(FRAME_DURATION_MS));
        }
        break;

    case STATE_IDLE:
        // Nothing — idle is driven by blink/outro timers
        break;
    }
}

// ---------------------------------------------------------------------------
// Widget init (called once at boot)
// ---------------------------------------------------------------------------
static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    art_img = lv_img_create(widget->obj);
    lv_obj_align(art_img, LV_ALIGN_TOP_LEFT, 0, 0);

    // Init work queue handler
    k_work_init(&anim_work, anim_work_handler);

    // Init timers
    k_timer_init(&frame_timer, frame_timer_cb, NULL);
    k_timer_init(&blink_timer, blink_timer_cb, NULL);
    k_timer_init(&outro_timer, outro_timer_cb, NULL);

    // Start intro immediately
    anim_state    = STATE_INTRO;
    current_frame = 0;
    show_frame(intro_frames[0]);
    k_timer_start(&frame_timer, K_MSEC(FRAME_DURATION_MS), K_MSEC(FRAME_DURATION_MS));

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_peripheral_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) {
    return widget->obj;
}