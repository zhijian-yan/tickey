// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "tickey.h"
#include <string.h>

typedef enum {
    TKEY_STATE_UNPRESSED = 0,
    TKEY_STATE_PRESSED,
} tkey_state_t;

typedef struct {
    tkey_t *key;
    tkey_event_t event;
    uint8_t press_count;
} tkey_message_t;

typedef struct {
    volatile uint8_t write_index;
    volatile uint8_t read_index;
    tkey_message_t buffer[TKEY_QUEUE_SIZE];
} tkey_queue_t;

static tkey_queue_t tkey_queue;

static int tkey_queue_send(tkey_queue_t *queue, const tkey_message_t *message) {
    uint8_t w;
    uint8_t next;
    w = queue->write_index;
    next = (w + 1) & (TKEY_QUEUE_SIZE - 1);
    if (next == queue->read_index)
        return -TKEY_EAGAIN;
    queue->buffer[w] = *message;
    queue->write_index = next;
    return 0;
}

static int tkey_queue_receive(tkey_queue_t *queue, tkey_message_t *message) {
    uint8_t r;
    r = queue->read_index;
    if (r == queue->write_index)
        return -TKEY_EAGAIN;
    *message = queue->buffer[r];
    queue->read_index = (r + 1) & (TKEY_QUEUE_SIZE - 1);
    return 0;
}

int tkey_init(tkey_t *key, tkey_event_cb_t event_cb, tkey_detect_cb_t detect_cb,
              void *user_data) {
    if (!key || !event_cb || !detect_cb)
        return -TKEY_EINVAL;
    memset(key, 0, sizeof(tkey_t));
    key->detect_cb = detect_cb;
    key->event_cb = event_cb;
    key->user_data = user_data;
    key->debounce_ticks = TKEY_DEFAULT_DEBOUNCE;
    key->hold_ticks = TKEY_DEFAULT_HOLD;
    key->multi_press_interval_ticks = TKEY_DEFAULT_MULTI_PRESS_INTERVAL;
    key->press_state = TKEY_STATE_UNPRESSED;
    return 0;
}

static tkey_event_t tkey_handler(tkey_t *key, int detect_value,
                                 uint8_t *press_count) {
    tkey_event_t event = TKEY_EVENT_NULL;
    if (key->multi_press_count) {
        if (key->multi_press_ticks < TKEY_MAX_TICKS)
            ++key->multi_press_ticks;
        if (key->multi_press_ticks >= key->multi_press_interval_ticks) {
            if (key->press_state == TKEY_STATE_PRESSED)
                event |= TKEY_EVENT_PRESS_TIMEOUT;
            else
                event |= TKEY_EVENT_RELEASE_TIMEOUT;
            key->multi_press_ticks = 0;
        }
    }
    if (key->press_state == TKEY_STATE_UNPRESSED) {
        if (detect_value) {
            if (key->pressed_ticks >= key->debounce_ticks) {
                key->press_state = TKEY_STATE_PRESSED;
                key->pressed_ticks = 0;
                key->multi_press_ticks = 0;
                if (key->multi_press_count < TKEY_MAX_COUNT)
                    ++key->multi_press_count;
                if (key->multi_press_count > 1)
                    event |= TKEY_EVENT_MULTI_PRESS;
                else
                    event |= TKEY_EVENT_PRESS;
            } else if (key->pressed_ticks < TKEY_MAX_TICKS)
                ++key->pressed_ticks;
        }
    } else if (key->press_state == TKEY_STATE_PRESSED) {
        if (key->pressed_ticks < TKEY_MAX_TICKS)
            ++key->pressed_ticks;
        if (!detect_value) {
            key->press_state = TKEY_STATE_UNPRESSED;
            if (key->flag_long_pressed) {
                key->flag_long_pressed = 0;
                event |= TKEY_EVENT_LONG_RELEASE;
            } else if (key->multi_press_count > 1)
                event |= TKEY_EVENT_MULTI_RELEASE;
            else
                event |= TKEY_EVENT_RELEASE;
            key->pressed_ticks = 0;
        } else if (key->pressed_ticks == key->hold_ticks) {
            key->flag_long_pressed = 1;
            event |= TKEY_EVENT_LONG_PRESS;
        }
    }
    *press_count = key->multi_press_count;
    if (event & (TKEY_EVENT_PRESS_TIMEOUT | TKEY_EVENT_RELEASE_TIMEOUT)) {
        key->multi_press_count = 0;
    }
    return event;
}

int tkey_scan(tkey_t key_arr[], uint32_t num) {
    int tkey_lock_state;
    int detect_value;
    int ret = 0;
    tkey_message_t message;
    if (!key_arr)
        return -TKEY_EINVAL;
    while (num--) {
        message.key = &key_arr[num];
        detect_value = message.key->detect_cb(message.key->user_data);
        tkey_lock_state = tkey_lock();
        message.event =
            tkey_handler(message.key, detect_value, &message.press_count);
        tkey_unlock(tkey_lock_state);
        if (message.event) {
            ret = tkey_queue_send(&tkey_queue, &message);
            if (ret)
                continue;
        }
    }
    return ret;
}

void tkey_dispatch(uint8_t max_event_num) {
    tkey_message_t message;
    while (max_event_num-- && !tkey_queue_receive(&tkey_queue, &message))
        message.key->event_cb(message.key, message.event, message.press_count,
                              message.key->user_data);
}

int tkey_set_hold(tkey_t *key, uint16_t hold_ticks) {
    int tkey_lock_state;
    if (!key)
        return -TKEY_EINVAL;
    tkey_lock_state = tkey_lock();
    key->hold_ticks = hold_ticks;
    tkey_unlock(tkey_lock_state);
    return 0;
}

int tkey_set_debounce(tkey_t *key, uint16_t debounce_ticks) {
    int tkey_lock_state;
    if (!key)
        return -TKEY_EINVAL;
    tkey_lock_state = tkey_lock();
    key->debounce_ticks = debounce_ticks;
    tkey_unlock(tkey_lock_state);
    return 0;
}

int tkey_set_multi_press_interval(tkey_t *key,
                                  uint16_t multi_press_interval_ticks) {
    int tkey_lock_state;
    if (!key)
        return -TKEY_EINVAL;
    tkey_lock_state = tkey_lock();
    key->multi_press_interval_ticks = multi_press_interval_ticks;
    tkey_unlock(tkey_lock_state);
    return 0;
}
