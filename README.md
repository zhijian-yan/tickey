# tickey

A lightweight event-driven embedded key/button library designed for:

* Bare-metal systems
* RTOS environments
* ISR-safe input scanning
* Real-time applications
* Low resource MCUs

`tickey` focuses on deterministic timing, low interrupt latency, and minimal runtime overhead.

---

# Features

* Event-driven button handling
* Debounce support
* Long press detection
* Multi-press detection
* ISR-safe scanning
* Deferred callback dispatch
* Lock-free single-producer single-consumer queue
* No dynamic memory allocation
* No dependency on RTOS
* Portable C implementation
* Deterministic execution time

---

# Design Overview

`tkey` uses a split architecture:

| Layer            | Responsibility                        |
| ---------------- | ------------------------------------- |
| ISR / timer tick | Key scanning and state machine update |
| Event queue      | Deferred event transport              |
| Main loop        | Callback dispatch                     |
| User callback    | Application logic                     |

This architecture ensures:

* Fast interrupt execution
* Stable key scanning interval
* Reduced ISR latency
* Better real-time responsiveness

---

# Architecture

```text
+-------------------+
| Hardware Timer ISR|
+-------------------+
          |
          v
+-------------------+
|   tkey_scan()     |
| state machine     |
+-------------------+
          |
          v
+-------------------+
| lock-free queue   |
+-------------------+
          |
          v
+-------------------+
| tkey_dispatch()   |
+-------------------+
          |
          v
+-------------------+
| user callback     |
+-------------------+
```

---

# Requirements

* C99 compatible compiler
* Periodic tick source
* User-defined critical section implementation

---

# Real-Time Considerations

`tkey_scan()` should be called periodically with a stable interval.

Example:

* 1 ms tick
* 5 ms tick
* 10 ms tick

All timing parameters are based on scan ticks.

Example:

```c
hold_ticks = 50;
```

If scan period is:

```text
10 ms
```

Then:

```text
50 * 10 ms = 500 ms
```

---

# Queue Model

`tkey` uses a lock-free:

```text
SPSC ring buffer
```

Meaning:

* Single producer
* Single consumer

Producer:

```text
ISR / scan context
```

Consumer:

```text
main loop / task context
```

Queue operations are O(1).

---

# Thread Safety

The library supports:

* ISR + main loop
* RTOS task + ISR
* Bare-metal systems

Critical sections are implemented through:

```c
static inline int tkey_lock(void);
static inline void tkey_unlock(int state);
```

Users should provide platform-specific implementations.

---

# Configuration

## Queue Size

```c
#define TKEY_QUEUE_SIZE 16
```

Requirements:

* Must be power of two
* Must not exceed queue index type range

---

# Event Types

| Event                      | Description            |
| -------------------------- | ---------------------- |
| TKEY_EVENT_PRESS           | Key pressed            |
| TKEY_EVENT_RELEASE         | Key released           |
| TKEY_EVENT_LONG_PRESS      | Long press detected    |
| TKEY_EVENT_LONG_RELEASE    | Long press released    |
| TKEY_EVENT_MULTI_PRESS     | Multi-press detected   |
| TKEY_EVENT_MULTI_RELEASE   | Multi-release detected |
| TKEY_EVENT_PRESS_TIMEOUT   | Press timeout          |
| TKEY_EVENT_RELEASE_TIMEOUT | Release timeout        |

---

# Basic Usage

## 1. Define Key Object

```c
static tkey_t key;
```

---

## 2. Implement Detect Callback

```c
static int key_detect(void *user_data)
{
    return gpio_read(KEY_GPIO);
}
```

Return:

* non-zero: pressed
* zero: released

---

## 3. Implement Event Callback

```c
static void key_event(
    tkey_t *key,
    tkey_event_t event,
    uint8_t press_count,
    void *user_data)
{
    switch (event) {
    case TKEY_EVENT_PRESS:
        break;

    case TKEY_EVENT_LONG_PRESS:
        break;

    default:
        break;
    }
}
```

---

## 4. Initialize Key

```c
int ret;

ret = tkey_init(
    &key,
    key_event,
    key_detect,
    NULL);
```

---

## 5. Periodic Scan

Example using timer ISR:

```c
void timer_isr(void)
{
    tkey_scan(&key, 1);
}
```

---

## 6. Dispatch Events

```c
while (1) {
    tkey_dispatch(8);
}
```

---

# API

## tkey_init

```c
int tkey_init(
    tkey_t *key,
    tkey_event_cb_t event_cb,
    tkey_detect_cb_t detect_cb,
    void *user_data);
```

Initialize key object.

Returns:

* 0 on success
* negative error code on failure

---

## tkey_scan

```c
int tkey_scan(
    tkey_t key_arr[],
    uint32_t size);
```

Run key scanning and update internal state machine.

Should be called periodically.

ISR-safe.

---

## tkey_dispatch

```c
void tkey_dispatch(uint32_t max_event_num);
```

Dispatch queued events.

Should be called in:

* main loop
* low-priority task

This function may execute user callbacks.

---

## tkey_set_hold

```c
int tkey_set_hold(
    tkey_t *key,
    uint16_t hold_ticks);
```

Set long press threshold.

---

## tkey_set_debounce

```c
int tkey_set_debounce(
    tkey_t *key,
    uint16_t debounce_ticks);
```

Set debounce duration.

---

## tkey_set_multi_press_interval

```c
int tkey_set_multi_press_interval(
    tkey_t *key,
    uint16_t interval_ticks);
```

Set multi-press timeout interval.

---

# Error Codes

`tkey` uses Linux-style negative error codes.

Example:

```c
return -TKEY_EINVAL;
```

| Error       | Description            |
| ----------- | ---------------------- |
| TKEY_EINVAL | Invalid parameter      |
| TKEY_EAGAIN | Queue full / try again |

---

# Porting

Users must provide:

```c
static inline int tkey_lock(void)
{
    return 0;
}

static inline void tkey_unlock(int state)
{
    (void)state;
}
```

Example using interrupt disable:

```c
static inline int tkey_lock(void)
{
    int state = irq_save();
    return state;
}

static inline void tkey_unlock(int state)
{
    irq_restore(state);
}
```

---

# Timing Recommendations

Recommended scan interval:

| Tick  | Recommendation      |
| ----- | ------------------- |
| 1 ms  | High responsiveness |
| 5 ms  | Recommended         |
| 10 ms | Low CPU usage       |

---

# ISR Guidelines

Recommended:

* Run `tkey_scan()` in timer ISR
* Run `tkey_dispatch()` outside ISR
* Keep callbacks lightweight

Avoid:

* Blocking operations inside callbacks
* Heavy rendering inside ISR
* Long critical sections

---

# Memory Usage

Characteristics:

* No malloc
* No heap allocation
* Static memory friendly
* Predictable memory usage

---

# Reentrancy

`tkey_scan()` is not designed for concurrent reentrant access on the same key object.

Typical usage:

* Single ISR producer
* Single consumer dispatcher

---

# Supported Platforms

Typical targets:

* STM32
* ESP32
* AVR
* NRF52
* GD32
* Bare-metal ARM Cortex-M

---

# Example Main Loop

```c
int main(void)
{
    system_init();

    tkey_init(
        &key,
        key_event,
        key_detect,
        NULL);

    timer_start();

    while (1) {
        tkey_dispatch(8);
    }
}
```

---

# License

MIT License
