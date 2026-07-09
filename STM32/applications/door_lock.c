/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-20     32764       Door lock - relay control electromagnetic lock
 *
 * 门锁驱动 - 继电器控制电磁锁 + 舵机开门
 *   简单状态：LOCKED（锁定） <-> OPEN（解锁）
 *   开门：继电器吸合 -> 电磁锁通电 -> 解锁 -> 舵机推门
 *   关门：舵机先关门 -> 等待舵机到位 -> 继电器断开 -> 电磁锁断电 -> 锁定
 *   支持自动关门
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <stdint.h>
#include <string.h>
#include "door_lock.h"
#include "servo.h"
#include "app_def.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "door.lock"
#define DBG_LEVEL           DBG_LOG
#include <rtdbg.h>

#ifdef FINSH_USING_MSH
#include <finsh.h>

/* ========================= 事件标志 ========================= */
#define DOOR_EVENT_OPEN     (1 << 0)         /* 开门事件 (API 调用) */
#define DOOR_EVENT_CLOSE    (1 << 1)         /* 关门事件 (超时) */
#define DOOR_EVENT_STOP     (1 << 2)         /* 停止线程 */
#define DOOR_EVENT_KEY      (1 << 3)         /* 按键事件 (定时器驱动) */
#define DOOR_EVENT_OPEN_ALARM (1 << 4)       /* 报警触发开门 (不自动关门) */

/* ========================= 全局变量 ========================= */
static rt_thread_t  door_tid = RT_NULL;
static rt_event_t   door_event = RT_NULL;
static rt_timer_t   door_timer = RT_NULL;
static rt_mutex_t   door_mutex = RT_NULL;
static door_state_t g_door_state = DOOR_LOCKED;

/* 按键扫描线程: 独立线程, 简单释放等待逻辑, 最可靠
 * 按下 → 消抖 → 发事件 → 等释放 → 继续扫描
 */
#define KEY_SCAN_STACK     512
#define KEY_SCAN_MS        10
#define KEY_DEBOUNCE_MS    30

static rt_thread_t key_tid = RT_NULL;

static void key_scan_entry(void *parameter)
{
    while (1) {
        if (rt_pin_read(PIN_KEY_DOOR) == PIN_LOW) {
            rt_thread_mdelay(KEY_DEBOUNCE_MS);
            if (rt_pin_read(PIN_KEY_DOOR) == PIN_LOW) {
                rt_event_send(door_event, DOOR_EVENT_KEY);
                rt_kprintf("[Key] Pressed, unlocking\n");
                while (rt_pin_read(PIN_KEY_DOOR) == PIN_LOW) {
                    rt_thread_mdelay(KEY_SCAN_MS);
                }
                rt_kprintf("[Key] Released\n");
            }
        }
        rt_thread_mdelay(KEY_SCAN_MS);
    }
}

/* ========================= 继电器控制 ========================= */
static void relay_on(void)
{
    rt_pin_write(PIN_DOOR_RELAY, PIN_LOW);   /* 低电平吸合继电器，电磁锁通电解锁 */
    /* 继电器动作后，通知 MQ2 进入禁触窗口（过滤电磁干扰） */
    mq2_ignore_after_relay_op();
    servo_open_door();
    LOG_I("Door UNLOCKED (relay ON + servo open)");
}

static void relay_off(void)
{
    /* 安全第一：先断继电器锁门，再慢慢关舵机 */
    rt_pin_write(PIN_DOOR_RELAY, PIN_HIGH);  /* 高电平断开继电器，电磁锁断电锁定 */
    /* 继电器动作后，通知 MQ2 进入禁触窗口（过滤电磁干扰） */
    mq2_ignore_after_relay_op();
    LOG_I("Door LOCKED (relay OFF immediately)");
    servo_close_door();
    /* 舵机关门在 servo_sweep 中阻塞 2000ms，但锁已安全 */
    LOG_I("Door servo closed");
}

/* ========================= 定时器回调 ========================= */
static void door_timer_callback(void *parameter)
{
    LOG_I("Auto-close timer expired");
    rt_event_send(door_event, DOOR_EVENT_CLOSE);
}

/* ========================= 门锁线程 =========================
 * 按键由独立线程 (key_scan) 驱动, 线程仅处理事件
 * 优先级: CLOSE 先处理, KEY/OPEN 后处理 → 同时到达时开门胜出
 */
static void door_lock_entry(void *parameter)
{
    rt_uint32_t events;
    rt_bool_t need_close = RT_FALSE;
    rt_bool_t need_open = RT_FALSE;

    LOG_I("Door lock thread started");

    while (1) {
        iwdg_refresh();

        /* 阻塞等待事件 (按键由定时器独立驱动, 无需轮询超时) */
        rt_event_recv(door_event,
                      DOOR_EVENT_KEY | DOOR_EVENT_OPEN | DOOR_EVENT_OPEN_ALARM |
                      DOOR_EVENT_CLOSE | DOOR_EVENT_STOP,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_FOREVER,
                      &events);

        if (events & DOOR_EVENT_STOP) {
            LOG_I("Door thread stop requested");
            break;
        }

        need_close = RT_FALSE;
        need_open = RT_FALSE;

        /* 关门：先修改状态，再释放锁，然后执行耗时操作 */
        if (events & DOOR_EVENT_CLOSE) {
            rt_mutex_take(door_mutex, RT_WAITING_FOREVER);
            if (g_door_state == DOOR_OPEN) {
                g_door_state = DOOR_CLOSING;
                need_close = RT_TRUE;
            }
            rt_mutex_release(door_mutex);

            if (need_close) {
                relay_off();
                rt_timer_stop(door_timer);
                iwdg_refresh();
                rt_mutex_take(door_mutex, RT_WAITING_FOREVER);
                g_door_state = DOOR_LOCKED;
                rt_mutex_release(door_mutex);
            }
            /* 同批次 KEY/OPEN 事件重新发送，避免被 RT_EVENT_FLAG_CLEAR 清除后丢失 */
            if (events & (DOOR_EVENT_KEY | DOOR_EVENT_OPEN)) {
                rt_event_send(door_event, events & (DOOR_EVENT_KEY | DOOR_EVENT_OPEN));
            }
            continue;
        }

        /* 开门 (按键 或 API 调用) */
        if (events & (DOOR_EVENT_KEY | DOOR_EVENT_OPEN | DOOR_EVENT_OPEN_ALARM)) {
            rt_bool_t no_auto = (events & DOOR_EVENT_OPEN_ALARM) != 0;

            rt_mutex_take(door_mutex, RT_WAITING_FOREVER);
            if (g_door_state == DOOR_LOCKED) {
                g_door_state = DOOR_OPENING;
                need_open = RT_TRUE;
            } else {
                LOG_I("Door already open, resetting timer");
                if (!no_auto) {
                    rt_timer_stop(door_timer);
                    rt_timer_start(door_timer);
                }
            }
            rt_mutex_release(door_mutex);

            if (need_open) {
                relay_on();
                rt_mutex_take(door_mutex, RT_WAITING_FOREVER);
                g_door_state = DOOR_OPEN;
                rt_mutex_release(door_mutex);
                /* 报警触发开门不启动自动关门定时器 */
                if (!no_auto) {
                    rt_timer_stop(door_timer);
                    rt_timer_start(door_timer);
                } else {
                    rt_timer_stop(door_timer);
                    rt_kprintf("[Door] Alarm unlock: auto-close DISABLED\n");
                }
            }
        }
    }

    relay_off();
    LOG_I("Door lock thread exited");
}

/* ========================= 对外 API ========================= */

/**
 * 开门
 * 非阻塞
 */
rt_err_t door_lock_open(void)
{
    if (door_event == RT_NULL) {
        LOG_E("Door lock not initialized");
        return -RT_ERROR;
    }
    return rt_event_send(door_event, DOOR_EVENT_OPEN);
}

/**
 * 开门 (报警触发，不启用自动关门定时器)
 * 非阻塞
 */
rt_err_t door_lock_open_no_auto_close(void)
{
    if (door_event == RT_NULL) {
        LOG_E("Door lock not initialized");
        return -RT_ERROR;
    }
    return rt_event_send(door_event, DOOR_EVENT_OPEN_ALARM);
}

/**
 * 关门
 * 非阻塞
 */
rt_err_t door_lock_close(void)
{
    if (door_event == RT_NULL) {
        LOG_E("Door lock not initialized");
        return -RT_ERROR;
    }
    return rt_event_send(door_event, DOOR_EVENT_CLOSE);
}

/**
 * 获取门锁状态
 */
door_state_t door_lock_get_state(void)
{
    door_state_t state;
    if (door_mutex == RT_NULL) return DOOR_ERROR;
    rt_mutex_take(door_mutex, RT_WAITING_FOREVER);
    state = g_door_state;
    rt_mutex_release(door_mutex);
    return state;
}

rt_bool_t door_lock_is_open(void)
{
    rt_bool_t open;
    if (door_mutex == RT_NULL) return RT_FALSE;
    rt_mutex_take(door_mutex, RT_WAITING_FOREVER);
    open = (g_door_state == DOOR_OPEN);
    rt_mutex_release(door_mutex);
    return open;
}

/* ========================= 控制台命令 ========================= */

static int dl_rt(int argc, char **argv)
{
    rt_kprintf("\n┌──────── RT-Thread Features Used ────────┐\n");
    rt_kprintf("│ IPC:\n");
    rt_kprintf("│   √ Mutex     (door_mutex, state_mtx)\n");
    rt_kprintf("│   √ Event     (door_event, alarm_idle)\n");
    rt_kprintf("│   √ Timer     (auto-close timer)\n");
    rt_kprintf("│   √ MessageQ  (g_led_mq)\n");
    rt_kprintf("│ Kernel:\n");
    rt_kprintf("│   √ Thread    (7 threads)\n");
    rt_kprintf("│   √ INIT_EXPORT (BOARD + APP level)\n");
    rt_kprintf("│ Device:\n");
    rt_kprintf("│   √ ADC       (adc2, MQ2 sensor)\n");
    rt_kprintf("│   √ PWM       (pwm1, servo)\n");
    rt_kprintf("│   √ PIN       (GPIO interrupt)\n");
    rt_kprintf("│   √ UART      (uart3, K230)\n");
    rt_kprintf("│ Components:\n");
    rt_kprintf("│   √ OneNET    (MQTT upload)\n");
    rt_kprintf("│   √ cJSON     (JSON parse)\n");
    rt_kprintf("│   √ AT        (ESP8266)\n");
    rt_kprintf("│   √ Finsh     (MSH debug shell)\n");
    rt_kprintf("│   √ IWDG      (watchdog)\n");
    rt_kprintf("│ Tools:\n");
    rt_kprintf("│   √ LOG_I/W/E (rtdbg log)\n");
    rt_kprintf("│   √ rt_kprintf (formatted output)\n");
    rt_kprintf("│   √ list_device/list_thread\n");
    rt_kprintf("└──────────────────────────────────────────┘\n\n");
    return 0;
}
MSH_CMD_EXPORT(dl_rt, show RT-Thread features used in this project);

static int dl_help(int argc, char **argv)
{
    rt_kprintf("\n┌──────── Door Lock CMD ────────┐\n");
    rt_kprintf("│ dl_stat      show door status\n");
    rt_kprintf("│ dl_open      open the door\n");
    rt_kprintf("│ dl_close     close the door\n");
    rt_kprintf("│ dl_key_test  test key input\n");
    rt_kprintf("│ dl_rt        RT-Thread features\n");
    rt_kprintf("│ dl_help      this help\n");
    rt_kprintf("│ sys_info     system info (heap etc)\n");
    rt_kprintf("│ list_threads thread stack usage\n");
    rt_kprintf("└────────────────────────────────┘\n\n");
    return 0;
}
MSH_CMD_EXPORT(dl_help, show all door lock commands);

int dl_open(int argc, char **argv)
{
    rt_err_t ret = door_lock_open();
    if (ret == RT_EOK) {
        rt_kprintf("Door open command sent\n");
    } else {
        rt_kprintf("Failed to send door open command\n");
    }
    return 0;
}
MSH_CMD_EXPORT(dl_open, open the door);

int dl_close(int argc, char **argv)
{
    rt_err_t ret = door_lock_close();
    if (ret == RT_EOK) {
        rt_kprintf("Door close command sent\n");
    } else {
        rt_kprintf("Failed to send door close command\n");
    }
    return 0;
}
MSH_CMD_EXPORT(dl_close, close the door);

int dl_stat(int argc, char **argv)
{
    int key_val = rt_pin_read(PIN_KEY_DOOR);
    int relay_val = rt_pin_read(PIN_DOOR_RELAY);
    int servo_angle = servo_get_angle();
    
    rt_kprintf("=== Door Status ===\n");
    rt_kprintf("Door state: %s\n", g_door_state == DOOR_LOCKED ? "LOCKED" : "OPEN");
    rt_kprintf("Relay: %s\n", relay_val ? "OFF (LOCKED)" : "ON (UNLOCKED)");
    rt_kprintf("Servo: %d°\n", servo_angle);
    rt_kprintf("Key (PC2): %s (raw=%d)\n", key_val ? "RELEASED (HIGH)" : "PRESSED (LOW)", key_val);
    rt_kprintf("Event: %p\n", door_event);
    return 0;
}
MSH_CMD_EXPORT(dl_stat, show door status);

/* 按键调试命令 - 持续读取按键状态 */
int dl_key_test(int argc, char **argv)
{
    int count = 10;
    if (argc > 1) {
        count = atoi(argv[1]);
        if (count <= 0 || count > 100) count = 10;
    }
    
    rt_kprintf("=== Key Test (PC2) ===\n");
    rt_kprintf("Press the key and hold it for 50ms to test\n\n");
    
    for (int i = 0; i < count; i++) {
        int val = rt_pin_read(PIN_KEY_DOOR);
        rt_kprintf("[%2d] PC2=%d (%s)\n", i, val, val ? "HIGH=released" : "LOW=pressed");
        rt_thread_mdelay(100);
    }
    rt_kprintf("=== Test complete ===\n");
    return 0;
}
MSH_CMD_EXPORT(dl_key_test, test key input - dl_key_test [count]);

/* ========================= 初始化 ========================= */

int door_lock_init(void)
{
    rt_pin_mode(PIN_DOOR_RELAY, PIN_MODE_OUTPUT);
    relay_off();
    rt_pin_mode(PIN_KEY_DOOR, PIN_MODE_INPUT_PULLUP);

    /* 门锁互斥锁 */
    door_mutex = rt_mutex_create("door_mtx", RT_IPC_FLAG_PRIO);
    if (door_mutex == RT_NULL) {
        LOG_E("Failed to create door mutex");
        return -RT_ENOMEM;
    }

    /* 创建事件 */
    door_event = rt_event_create("door_evt", RT_IPC_FLAG_FIFO);
    if (door_event == RT_NULL) {
        LOG_E("Failed to create door event");
        rt_mutex_delete(door_mutex);
        door_mutex = RT_NULL;
        return -RT_ENOMEM;
    }

    /* 按键扫描线程 */
    key_tid = rt_thread_create("key_scan",
                                key_scan_entry,
                                RT_NULL,
                                KEY_SCAN_STACK,
                                PRIO_KEY_SCAN,
                                10);
    if (key_tid == RT_NULL) {
        LOG_E("Failed to create key scan thread");
        rt_event_delete(door_event);
        door_event = RT_NULL;
        rt_mutex_delete(door_mutex);
        door_mutex = RT_NULL;
        return -RT_ENOMEM;
    }
    rt_thread_startup(key_tid);

    /* 自动关门定时器 */
    door_timer = rt_timer_create("door_tmr",
                                  door_timer_callback,
                                  RT_NULL,
                                  rt_tick_from_millisecond(DOOR_OPEN_TIMEOUT_MS),
                                  RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
    if (door_timer == RT_NULL) {
        LOG_E("Failed to create door timer");
        rt_thread_delete(key_tid);
        key_tid = RT_NULL;
        rt_event_delete(door_event);
        door_event = RT_NULL;
        rt_mutex_delete(door_mutex);
        door_mutex = RT_NULL;
        return -RT_ENOMEM;
    }

    /* 门锁处理线程 */
    door_tid = rt_thread_create("door_lock",
                                 door_lock_entry,
                                 RT_NULL,
                                 STACK_DOOR_LOCK,
                                 PRIO_DOOR_LOCK,
                                 TICK_DOOR_LOCK);
    if (door_tid == RT_NULL) {
        LOG_E("Failed to create door lock thread");
        rt_thread_delete(key_tid);
        key_tid = RT_NULL;
        rt_timer_delete(door_timer);
        door_timer = RT_NULL;
        rt_event_delete(door_event);
        door_event = RT_NULL;
        rt_mutex_delete(door_mutex);
        door_mutex = RT_NULL;
        return -RT_ENOMEM;
    }

    rt_thread_startup(door_tid);

    g_door_state = DOOR_LOCKED;

    LOG_I("Door lock ready");
    rt_kprintf("=== Door Lock Ready ===\n");
    rt_kprintf("  Key: PC2, Relay: PE0, Servo: PE9, Auto-close: %ds\n", DOOR_OPEN_TIMEOUT_MS / 1000);

    return 0;
}
INIT_APP_EXPORT(door_lock_init);

#endif /* FINSH_USING_MSH */