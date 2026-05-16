// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __TICKEY_H
#define __TICKEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline int tkey_lock(void) {
    /* Disable interrupts if needed */
    return 0;
}

static inline void tkey_unlock(int tkey_lock_state) {
    /* Restore interrupt state */
    (void)tkey_lock_state;
}

#define TKEY_DEFAULT_DEBOUNCE (1)
#define TKEY_DEFAULT_HOLD (50)
#define TKEY_DEFAULT_MULTI_PRESS_INTERVAL (30)
#define TKEY_QUEUE_SIZE (16)
#if (TKEY_QUEUE_SIZE > 256)
#error "TKEY_QUEUE_SIZE must be <= 256"
#endif
#if (TKEY_QUEUE_SIZE & (TKEY_QUEUE_SIZE - 1)) != 0
#error "TKEY_QUEUE_SIZE must be power of 2"
#endif
#define TKEY_MAX_TICKS (0xFFFF)
#define TKEY_MAX_COUNT (0xFF)
#define TKEY_EINVAL 22
#define TKEY_EAGAIN 11

typedef struct tkey tkey_t;

typedef enum {
    TKEY_EVENT_NULL = 0,
    TKEY_EVENT_PRESS = (1 << 0),
    TKEY_EVENT_RELEASE = (1 << 1),
    TKEY_EVENT_LONG_PRESS = (1 << 2),
    TKEY_EVENT_LONG_RELEASE = (1 << 3),
    TKEY_EVENT_MULTI_PRESS = (1 << 4),
    TKEY_EVENT_MULTI_RELEASE = (1 << 5),
    TKEY_EVENT_PRESS_TIMEOUT = (1 << 6),
    TKEY_EVENT_RELEASE_TIMEOUT = (1 << 7),
    TKEY_EVENT_ALL_PRESS =
        (TKEY_EVENT_PRESS | TKEY_EVENT_LONG_PRESS | TKEY_EVENT_MULTI_PRESS),
    TKEY_EVENT_ALL_RELEASE = (TKEY_EVENT_RELEASE | TKEY_EVENT_LONG_RELEASE |
                              TKEY_EVENT_MULTI_RELEASE),
    TKEY_EVENT_DEFAULT_PRESS = (TKEY_EVENT_PRESS | TKEY_EVENT_MULTI_PRESS),
    TKEY_EVENT_DEFAULT_RELEASE = TKEY_EVENT_ALL_RELEASE,
} tkey_event_t;

typedef void (*tkey_event_cb_t)(tkey_t *key, tkey_event_t event,
                                uint8_t multi_press_count, void *user_data);

typedef int (*tkey_detect_cb_t)(void *user_data);

struct tkey {
    tkey_event_cb_t event_cb;
    tkey_detect_cb_t detect_cb;
    void *user_data;
    volatile uint16_t hold_ticks;
    volatile uint16_t debounce_ticks;
    volatile uint16_t multi_press_interval_ticks;
    volatile uint16_t pressed_ticks;
    volatile uint16_t multi_press_ticks;
    volatile uint8_t multi_press_count;
    volatile uint8_t press_state;
    volatile uint8_t flag_long_pressed;
};

int tkey_init(tkey_t *key, tkey_event_cb_t event_cb, tkey_detect_cb_t detect_cb,
              void *user_data);
int tkey_scan(tkey_t key_arr[], uint32_t num);
void tkey_dispatch(uint8_t max_event_num);
int tkey_set_hold(tkey_t *key, uint16_t hold_ticks);
int tkey_set_debounce(tkey_t *key, uint16_t debounce_ticks);
int tkey_set_multi_press_interval(tkey_t *key,
                                  uint16_t multi_press_interval_ticks);

#ifdef __cplusplus
}
#endif

#endif
