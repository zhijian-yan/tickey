// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#include "tickey.h"

#define TKEY_MAX_TICKS (0xFFFF)
#define TKEY_MAX_COUNT (0xFF)
#define TKEY_STATE_UNPRESSED 0
#define TKEY_STATE_PRESSED 1

struct tkey {
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
    uint8_t pressed_level : 1;
    uint8_t flag_long_pressed : 1;
    uint8_t flag_in_handler : 1;
    uint8_t flag_delete_in_handler : 1;
    uint8_t enabled : 1;
};

tkey_handle_t tkey_create(tkey_config_t *config) {
    if (!config)
        return NULL;
    tkey_handle_t tkey = (tkey_handle_t)tkey_malloc(sizeof(struct tkey));
    if (!tkey)
        return NULL;
    tkey->event_cb = config->event_cb;
    tkey->detect_cb = config->detect_cb;
    tkey->user_data = config->user_data;
    tkey->hold_ticks = config->hold_ticks;
    tkey->debounce_ticks = config->debounce_ticks;
    tkey->multi_press_interval_ticks = config->multi_press_interval_ticks;
    tkey->pressed_level = config->pressed_level;
    tkey->pressed_ticks = 0;
    tkey->multi_press_ticks = 0;
    tkey->multi_press_count = 0;
    tkey->press_state = TKEY_STATE_UNPRESSED;
    tkey->flag_long_pressed = 0;
    tkey->flag_in_handler = 0;
    tkey->flag_delete_in_handler = 0;
    tkey->enabled = 1;
    return tkey;
}

void tkey_delete(tkey_handle_t *pkey) {
    tkey_handle_t key = *pkey;
    if (!pkey || !key)
        return;
    if (!key->flag_in_handler) {
        *pkey = NULL;
        tkey_free(key);
        return;
    } else
        key->flag_delete_in_handler = 1;
}

tkey_handle_t tkey_create_default(tkey_event_cb_t event_cb,
                                  tkey_detect_cb_t detect_cb, void *user_data) {
    tkey_config_t config;
    config.debounce_ticks = 1;
    config.detect_cb = detect_cb;
    config.event_cb = event_cb;
    config.hold_ticks = 25;
    config.multi_press_interval_ticks = 15;
    config.pressed_level = 0;
    config.user_data = user_data;
    return tkey_create(&config);
}

void tkey_handler(tkey_handle_t *pkey) {
    tkey_event_t event = 0;
    tkey_handle_t key = *pkey;
    if (!pkey || !key)
        return;
    if (!key->enabled || !key->detect_cb || !key->event_cb)
        return;
    if (key->flag_in_handler)
        return;
    key->flag_in_handler = 1;
    if (key->multi_press_count > 0) {
        if (key->multi_press_ticks < TKEY_MAX_TICKS)
            key->multi_press_ticks++;
        if (key->multi_press_ticks > key->multi_press_interval_ticks) {
            if (key->press_state == TKEY_STATE_PRESSED)
                event |= TKEY_EVENT_PRESS_TIMEOUT;
            else
                event |= TKEY_EVENT_RELEASE_TIMEOUT;
            key->event_cb(key, event, key->multi_press_count, key->user_data);
            key->multi_press_count = 0;
            key->multi_press_ticks = 0;
        }
    }
    if (key->press_state == TKEY_STATE_UNPRESSED) {
        if (key->detect_cb(key->user_data) == key->pressed_level) {
            if (key->pressed_ticks >= key->debounce_ticks) {
                key->press_state = TKEY_STATE_PRESSED;
                key->pressed_ticks = 0;
                key->multi_press_ticks = 0;
                if (key->multi_press_count < TKEY_MAX_COUNT)
                    key->multi_press_count++;
                if (key->multi_press_count > 1)
                    event |= TKEY_EVENT_MULTI_PRESS;
                else
                    event |= TKEY_EVENT_PRESS;
                key->event_cb(key, event, key->multi_press_count,
                              key->user_data);
            }
            if (key->pressed_ticks < TKEY_MAX_TICKS)
                key->pressed_ticks++;
        }
    } else if (key->press_state == TKEY_STATE_PRESSED) {
        if (key->pressed_ticks < TKEY_MAX_TICKS)
            key->pressed_ticks++;
        if (key->detect_cb(key->user_data) != key->pressed_level) {
            key->press_state = TKEY_STATE_UNPRESSED;
            if (key->flag_long_pressed) {
                key->flag_long_pressed = 0;
                event = TKEY_EVENT_LONG_RELEASE;
            } else if (key->multi_press_count > 1)
                event |= TKEY_EVENT_MULTI_RELEASE;
            else
                event |= TKEY_EVENT_RELEASE;
            key->event_cb(key, event, key->multi_press_count, key->user_data);
            key->pressed_ticks = 0;
        } else if (key->pressed_ticks == key->hold_ticks) {
            key->flag_long_pressed = 1;
            event |= TKEY_EVENT_LONG_PRESS;
            key->event_cb(key, event, key->multi_press_count, key->user_data);
        }
    }
    if (key->flag_delete_in_handler) {
        *pkey = NULL;
        tkey_free(key);
        return;
    }
    key->flag_in_handler = 0;
}

void tkey_multi_handler(tkey_handle_t key[], uint32_t num) {
    while (num--)
        tkey_handler(&key[num]);
}

void tkey_register_callback(tkey_handle_t key, tkey_event_cb_t event_cb,
                            tkey_detect_cb_t detect_cb, void *user_data) {
    if (!key)
        return;
    key->user_data = user_data;
    key->event_cb = event_cb;
    key->detect_cb = detect_cb;
}

void tkey_set_pressed_level(tkey_handle_t key, uint8_t pressed_level) {
    if (!key)
        return;
    key->pressed_level = pressed_level;
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

void tkey_set_enabled(tkey_handle_t key, uint8_t enabled) {
    if (!key)
        return;
    key->enabled = enabled;
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
