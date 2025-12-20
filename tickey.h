// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Zhijian Yan

#ifndef __TICKEY_H
#define __TICKEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#define tkey_malloc(size) malloc(size)
#define tkey_free(ptr) free(ptr)
#define TKEY_MAX_TICKS (0xFFFF)
#define TKEY_MAX_COUNT (0xFF)

/**
 * @brief Enumeration of key events
 */
typedef enum {
    TKEY_EVENT_NULL = 0x00,         /**< Key null event */
    TKEY_EVENT_PRESS = 0x01,        /**< Key press event */
    TKEY_EVENT_RELEASE = 0x02,      /**< Key release event after single press */
    TKEY_EVENT_LONG_PRESS = 0x04,   /**< Long press event */
    TKEY_EVENT_LONG_RELEASE = 0x08, /**< Key release event after long press */
    TKEY_EVENT_MULTI_PRESS = 0x10,  /**< Multiple press event */
    TKEY_EVENT_MULTI_RELEASE =
        0x20, /**< Key release event after multiple presses */
    TKEY_EVENT_PRESS_TIMEOUT =
        0x40, /**< Multi-press detection timeout in pressed state */
    TKEY_EVENT_RELEASE_TIMEOUT =
        0x80, /**< Multi-press detection timeout in released state */
    TKEY_EVENT_ALL_PRESS =
        (TKEY_EVENT_PRESS | TKEY_EVENT_LONG_PRESS | TKEY_EVENT_MULTI_PRESS),
    TKEY_EVENT_ALL_RELEASE = (TKEY_EVENT_RELEASE | TKEY_EVENT_LONG_RELEASE |
                              TKEY_EVENT_MULTI_RELEASE),
    TKEY_EVENT_DEFAULT_PRESS = (TKEY_EVENT_PRESS | TKEY_EVENT_MULTI_PRESS),
    TKEY_EVENT_DEFAULT_RELEASE = TKEY_EVENT_ALL_RELEASE,
} tkey_event_t;

/**
 * @brief Opaque handle type for key instances
 */
typedef struct tkey *tkey_handle_t;

/**
 * @brief Key event callback function type
 *
 * @param key Handle of the key that triggered the event
 * @param event Type of key event that occurred
 * @param multi_press_count Multi-press count (2 for double, 3 for triple, etc.)
 * @param user_data User-defined data passed to both event and detection
 * callbacks
 */
typedef void (*tkey_event_cb_t)(tkey_handle_t key, tkey_event_t event,
                                uint8_t multi_press_count, void *user_data);

/**
 * @brief Key detection callback function type
 *
 * @param user_data User-defined data passed to both event and detection
 * callbacks
 * @return
 * Pressed level if key is pressed, otherwise released level
 */
typedef int (*tkey_detect_cb_t)(void *user_data);

/**
 * @brief Key configuration structure
 */
typedef struct {
    tkey_event_cb_t event_cb;   /**< Event callback function */
    tkey_detect_cb_t detect_cb; /**< Key detection callback function */
    void *user_data; /**< User-defined data passed to both event and detection
                        callbacks */
    uint16_t hold_ticks;                 /**< Long press duration in ticks */
    uint16_t debounce_ticks;             /**< Debounce time in ticks */
    uint16_t multi_press_interval_ticks; /**< Multi-press interval in ticks */
} tkey_config_t;

/**
 * @brief Create a key instance with default configuration
 * @note Default key configration:
 * debounce_ticks:              10ms@100Hz
 * hold_ticks:                  500ms@100Hz
 * multi_press_interval_ticks:  300ms@100Hz
 *
 * @param event_cb Event callback function
 * @param detect_cb Detection callback function
 * @param user_data User-defined data passed to both event and detection
 * callbacks
 * @return
 * Key handle on success, NULL on failure
 */
tkey_handle_t tkey_create_default(tkey_event_cb_t event_cb,
                                  tkey_detect_cb_t detect_cb, void *user_data);

/**
 * @brief Create a key instance with custom configuration
 *
 * @param config Key configuration parameters
 * @return
 * Key handle on success, NULL on failure
 */
tkey_handle_t tkey_create(tkey_config_t *config);

/**
 * @brief Delete a key instance and release its resources
 * @note This operation can only be performed when the key is disabled
 *
 * @param key Handle of the target key
 */
void tkey_delete(tkey_handle_t key);

/**
 * @brief Enable key processing
 * @note This function adds the key to the active key list
 *
 * @param key Handle of the target key
 */
void tkey_enable(tkey_handle_t key);

/**
 * @brief Disable key processing
 * @note This function removes the key from the active key list
 *
 * @param key Handle of the target key
 */
void tkey_disable(tkey_handle_t key);

/**
 * @brief Key event handler
 * @note This function should be called periodically at the configured frequency
 */
void tkey_handler(void);

/**
 * @brief Register or update callback functions for a key
 * @note Callbacks can only be registered when the key is not enabled
 *
 * @param key Handle of the target key
 * @param event_cb New event callback function
 * @param detect_cb New detection callback function
 * @param user_data User-defined data passed to both event and detection
 * callbacks
 */
void tkey_register_callback(tkey_handle_t key, tkey_event_cb_t event_cb,
                            tkey_detect_cb_t detect_cb, void *user_data);

/**
 * @brief Set long press duration
 *
 * @param key Handle of the target key
 * @param hold_ticks Long press duration in system ticks
 */
void tkey_set_hold(tkey_handle_t key, uint16_t hold_ticks);

/**
 * @brief Set key debounce time
 *
 * @param key Handle of the target key
 * @param debounce_ticks Debounce time in system ticks
 */
void tkey_set_debounce(tkey_handle_t key, uint16_t debounce_ticks);

/**
 * @brief Set multi-press detection interval
 *
 * @param key Handle of the target key
 * @param multi_press_interval_ticks Multi-press interval in system ticks
 */
void tkey_set_multi_press_interval(tkey_handle_t key,
                                   uint16_t multi_press_interval_ticks);

/**
 * @brief Get the current pressed duration
 *
 * @param key Handle of the target key
 * @return
 * Current pressed duration in ticks
 */
uint16_t tkey_get_pressed_ticks(tkey_handle_t key);

/**
 * @brief Get the current multi-press interval time
 *
 * @param key Handle of the target key
 * @return
 * Current multi-press interval time in ticks
 */
uint16_t tkey_get_multi_press_ticks(tkey_handle_t key);

#ifdef __cplusplus
}
#endif

#endif
