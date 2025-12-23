// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "tickey.h"
#include <string.h>

#define TKEY_STATE_UNPRESSED 0
#define TKEY_STATE_PRESSED 1

static struct tkey {
    tkey_event_cb_t event_cb;
    tkey_detect_cb_t detect_cb;
    void *user_data;
    uint16_t hold_ticks;
    uint16_t debounce_ticks;
    uint16_t multi_press_interval_ticks;
    uint16_t pressed_ticks;
    uint16_t multi_press_ticks;
    uint8_t multi_press_count;
    uint8_t press_state : 1;
    uint8_t flag_long_pressed : 1;
    uint8_t enabled : 1;
    tkey_handle_t prev;
    tkey_handle_t next;
} *head;

volatile static uint32_t flag_critical;

tkey_handle_t tkey_create(tkey_config_t *config) {
    tkey_handle_t key;
    if (!config)
        return NULL;
    key = (tkey_handle_t)tkey_malloc(sizeof(struct tkey));
    if (!key)
        return NULL;
    memset(key, 0, sizeof(struct tkey));
    key->event_cb = config->event_cb;
    key->detect_cb = config->detect_cb;
    key->user_data = config->user_data;
    key->hold_ticks = config->hold_ticks;
    key->debounce_ticks = config->debounce_ticks;
    key->multi_press_interval_ticks = config->multi_press_interval_ticks;
    key->pressed_ticks = 0;
    key->multi_press_ticks = 0;
    key->multi_press_count = 0;
    key->press_state = TKEY_STATE_UNPRESSED;
    key->flag_long_pressed = 0;
    key->enabled = 0;
    return key;
}

void tkey_delete(tkey_handle_t key) {
    if (!key || key->enabled)
        return;
    tkey_free(key);
}

tkey_handle_t tkey_create_default(tkey_event_cb_t event_cb,
                                  tkey_detect_cb_t detect_cb, void *user_data) {
    tkey_config_t config;
    config.debounce_ticks = 1;
    config.detect_cb = detect_cb;
    config.event_cb = event_cb;
    config.hold_ticks = 50;
    config.multi_press_interval_ticks = 30;
    config.user_data = user_data;
    return tkey_create(&config);
}

static void tkey_list_add(tkey_handle_t key) {
    ++flag_critical;
    if (head == NULL) {
        head = key;
        head->prev = key;
    } else {
        key->prev = head->prev;
        head->prev->next = key;
        head->prev = key;
    }
    key->next = NULL;
    --flag_critical;
}

static void tkey_list_del(tkey_handle_t key) {
    ++flag_critical;
    if (key == head) {
        if (key->next) {
            key->next->prev = head->prev;
            head = key->next;
        } else
            head = NULL;
    } else {
        key->prev->next = key->next;
        if (key->next)
            key->next->prev = key->prev;
        else
            head->prev = key->prev;
    }
    key->prev = NULL;
    key->next = NULL;
    --flag_critical;
}

void tkey_enable(tkey_handle_t key) {
    if (!key || key->enabled)
        return;
    key->enabled = 1;
    if (!key->detect_cb || !key->event_cb)
        return;
    tkey_list_add(key);
}

void tkey_disable(tkey_handle_t key) {
    if (!key || !key->enabled)
        return;
    tkey_list_del(key);
    key->enabled = 0;
}

static void tkey_key_handler(tkey_handle_t key) {
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
        if (key->detect_cb(key->user_data)) {
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
        if (!key->detect_cb(key->user_data)) {
            key->press_state = TKEY_STATE_UNPRESSED;
            if (key->flag_long_pressed) {
                key->flag_long_pressed = 0;
                event = TKEY_EVENT_LONG_RELEASE;
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
    if (event) {
        key->event_cb(key, event, key->multi_press_count, key->user_data);
        if (event & (TKEY_EVENT_PRESS_TIMEOUT | TKEY_EVENT_RELEASE_TIMEOUT))
            key->multi_press_count = 0;
    }
}

void tkey_handler(void) {
    tkey_handle_t temp = head;
    tkey_handle_t next;
    if (flag_critical)
        return;
    while (temp) {
        next = temp->next;
        tkey_key_handler(temp);
        temp = next;
    }
}

void tkey_register_callback(tkey_handle_t key, tkey_event_cb_t event_cb,
                            tkey_detect_cb_t detect_cb, void *user_data) {
    if (!key || key->enabled)
        return;
    key->user_data = user_data;
    key->event_cb = event_cb;
    key->detect_cb = detect_cb;
}

void tkey_set_hold(tkey_handle_t key, uint16_t hold_ticks) {
    if (!key)
        return;
    key->hold_ticks = hold_ticks;
}

void tkey_set_debounce(tkey_handle_t key, uint16_t debounce_ticks) {
    if (!key)
        return;
    key->debounce_ticks = debounce_ticks;
}

void tkey_set_multi_press_interval(tkey_handle_t key,
                                   uint16_t multi_press_interval_ticks) {
    if (!key)
        return;
    key->multi_press_interval_ticks = multi_press_interval_ticks;
}

uint16_t tkey_get_pressed_ticks(tkey_handle_t key) {
    if (!key)
        return 0;
    return key->pressed_ticks;
}

uint16_t tkey_get_multi_press_ticks(tkey_handle_t key) {
    if (!key)
        return 0;
    return key->multi_press_ticks;
}
