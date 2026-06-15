// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "tickey.h"
#include <string.h>

typedef struct {
    tkey_t *key;
    tkey_event_t event;
    uint8_t press_count;
} tkey_message_t;

typedef struct {
    tkey_message_t buffer[TKEY_QUEUE_SIZE];
    volatile uint8_t write_index;
    volatile uint8_t read_index;
} tkey_queue_t;

static tkey_queue_t tkey_queue;

static int tkey_queue_send(tkey_queue_t *queue, const tkey_message_t *message) {
    int ret = 0;
    uint8_t w;
    uint8_t next;
    w = queue->write_index;
    next = (w + 1) & (TKEY_QUEUE_SIZE - 1);
    if (next == queue->read_index) {
        ret = -TKEY_EAGAIN;
    } else {
        queue->buffer[w] = *message;
        queue->write_index = next;
    }
    return ret;
}

static int tkey_queue_receive(tkey_queue_t *queue, tkey_message_t *message) {
    int ret = 0;
    uint8_t r;
    r = queue->read_index;
    if (r == queue->write_index) {
        ret = -TKEY_EAGAIN;
    } else {
        *message = queue->buffer[r];
        queue->read_index = (r + 1) & (TKEY_QUEUE_SIZE - 1);
    }
    return ret;
}

int tkey_init(tkey_t *key, tkey_cb_mode_t cb_mode, tkey_event_cb_t event_cb,
              tkey_read_cb_t read_cb, void *user_data) {
    int ret = 0;
    if (!key || !event_cb || !read_cb) {
        ret = -TKEY_EINVAL;
    } else {
        memset(key, 0, sizeof(tkey_t));
        key->read_cb = read_cb;
        key->event_cb = event_cb;
        key->user_data = user_data;
        key->cb_mode = cb_mode;
        key->debounce_ticks = TKEY_DEFAULT_DEBOUNCE;
        key->long_press_duration_ticks = TKEY_DEFAULT_LONG_PRESS_THRESHOLD;
        key->multi_press_timeout_ticks = TKEY_DEFAULT_MULTI_PRESS_INTERVAL;
        key->state = TKEY_STATE_UNPRESSED;
    }
    return ret;
}

static tkey_event_t tkey_scan_key(tkey_t *key, int read_value,
                                  uint8_t *press_count) {
    tkey_event_t event = TKEY_EVENT_NULL;
    if (key->press_count) {
        if (key->multi_press_ticks < TKEY_MAX_TICKS) {
            ++key->multi_press_ticks;
        }
        if (key->multi_press_ticks >= key->multi_press_timeout_ticks) {
            if (key->state == TKEY_STATE_PRESSED) {
                event |= TKEY_EVENT_PRESS_TIMEOUT;
            } else {
                event |= TKEY_EVENT_RELEASE_TIMEOUT;
            }
            key->multi_press_ticks = 0;
        }
    }
    if (key->state == TKEY_STATE_UNPRESSED) {
        if (read_value) {
            if (key->press_ticks >= key->debounce_ticks) {
                key->state = TKEY_STATE_PRESSED;
                key->press_ticks = 0;
                key->multi_press_ticks = 0;
                if (key->press_count < TKEY_MAX_COUNT) {
                    ++key->press_count;
                }
                if (key->press_count > 1) {
                    event |= TKEY_EVENT_MULTI_PRESS;
                } else {
                    event |= TKEY_EVENT_PRESS;
                }
            } else if (key->press_ticks < TKEY_MAX_TICKS) {
                ++key->press_ticks;
            }
        }
    } else if (key->state == TKEY_STATE_PRESSED) {
        if (key->press_ticks < TKEY_MAX_TICKS) {
            ++key->press_ticks;
        }
        if (!read_value) {
            key->state = TKEY_STATE_UNPRESSED;
            if (key->long_press_triggered) {
                key->long_press_triggered = 0;
                event |= TKEY_EVENT_LONG_RELEASE;
            } else if (key->press_count > 1) {
                event |= TKEY_EVENT_MULTI_RELEASE;
            } else {
                event |= TKEY_EVENT_RELEASE;
            }
            key->press_ticks = 0;
        } else if (key->press_ticks == key->long_press_duration_ticks) {
            key->long_press_triggered = 1;
            event |= TKEY_EVENT_LONG_PRESS;
        }
    }
    *press_count = key->press_count;
    if (event & (TKEY_EVENT_PRESS_TIMEOUT | TKEY_EVENT_RELEASE_TIMEOUT)) {
        key->press_count = 0;
    }
    return event;
}

int tkey_scan(tkey_t keys[], uint32_t key_count) {
    int ret = 0;
    int read_value;
    int tkey_lock_state;
    tkey_message_t message;
    if (!keys) {
        ret = -TKEY_EINVAL;
    } else {
        while (key_count--) {
            message.key = &keys[key_count];
            read_value = message.key->read_cb(message.key->user_data);
            tkey_lock_state = tkey_lock();
            message.event =
                tkey_scan_key(message.key, read_value, &message.press_count);
            tkey_unlock(tkey_lock_state);
            if (message.event) {
                if (message.key->cb_mode == TKEY_CB_MODE_IMMEDIATE) {
                    message.key->event_cb(message.key, message.event,
                                          message.press_count,
                                          message.key->user_data);
                } else {
                    ret |= tkey_queue_send(&tkey_queue, &message);
                }
            }
        }
    }
    return ret;
}

void tkey_dispatch(uint8_t max_event_num) {
    tkey_message_t message;
    while (max_event_num-- && !tkey_queue_receive(&tkey_queue, &message)) {
        message.key->event_cb(message.key, message.event, message.press_count,
                              message.key->user_data);
    }
}

int tkey_set_debounce(tkey_t *key, uint16_t debounce_ticks) {
    int tkey_lock_state;
    int ret = 0;
    if (!key) {
        ret = -TKEY_EINVAL;
    } else {
        tkey_lock_state = tkey_lock();
        key->debounce_ticks = debounce_ticks;
        tkey_unlock(tkey_lock_state);
    }
    return ret;
}

int tkey_set_long_press_duration(tkey_t *key,
                                 uint16_t long_press_duration_ticks) {
    int tkey_lock_state;
    int ret = 0;
    if (!key) {
        ret = -TKEY_EINVAL;
    } else {
        tkey_lock_state = tkey_lock();
        key->long_press_duration_ticks = long_press_duration_ticks;
        tkey_unlock(tkey_lock_state);
    }
    return ret;
}

int tkey_set_multi_press_timeout(tkey_t *key,
                                 uint16_t multi_press_timeout_ticks) {
    int tkey_lock_state;
    int ret = 0;
    if (!key) {
        ret = -TKEY_EINVAL;
    } else {
        tkey_lock_state = tkey_lock();
        key->multi_press_timeout_ticks = multi_press_timeout_ticks;
        tkey_unlock(tkey_lock_state);
    }
    return ret;
}
