/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-16     32764       Emergency sound and light alarm thread
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <stdint.h>
#include <string.h>
#include "app_def.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "docker.alarm"
#if ONENET_DEBUG
#define DBG_LEVEL           DBG_LOG
#else
#define DBG_LEVEL           DBG_INFO
#endif
#include <rtdbg.h>

#ifdef FINSH_USING_MSH
#include <finsh.h>

#define ALARM_ON_CYCLES     5
#define ALARM_OFF_CYCLES    5

static rt_thread_t alarm_tid = RT_NULL;
static rt_event_t  g_alarm_idle_evt = RT_NULL;
static rt_mutex_t  g_alarm_mutex = RT_NULL;
static volatile rt_bool_t alarm_enabled = RT_FALSE;
static volatile rt_bool_t alarm_running = RT_FALSE;
static TIM_HandleTypeDef htim3_buzzer;

/* ========================= 蜂鸣器 PWM 初始化 (PE2 = TIM3_CH2) ========================= */
static void buzzer_pwm_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    htim3_buzzer.Instance = TIM3;
    htim3_buzzer.Init.Prescaler = 27;            /* 84MHz / 28 = 3MHz */
    htim3_buzzer.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3_buzzer.Init.Period = 999;              /* 3MHz / 1000 = 3kHz */
    htim3_buzzer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3_buzzer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3_buzzer);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 500;  /* 50% duty */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3_buzzer, &sConfigOC, TIM_CHANNEL_2);

    LOG_I("Buzzer PWM ready: TIM3_CH2 (PE2), %dHz", BUZZER_PWM_FREQ);
}

/* ========================= 蜂鸣器控制 ========================= */
static void buzzer_on(void)
{
    HAL_TIM_PWM_Start(&htim3_buzzer, TIM_CHANNEL_2);
}

static void buzzer_off(void)
{
    HAL_TIM_PWM_Stop(&htim3_buzzer, TIM_CHANNEL_2);
}

/* ========================= 报警灯控制 (PG6 独立引脚) ========================= */
static void alarm_led_on(void)
{
    rt_pin_write(PIN_ALARM_LED, PIN_HIGH);
}

static void alarm_led_off(void)
{
    rt_pin_write(PIN_ALARM_LED, PIN_LOW);
}

/* ========================= 应急声光报警循环 =========================
 * 模式：蜂鸣+LED闪烁 5 次 → 暂停 → 重复，直到报警取消
 * ========================= */
static void alarm_thread_entry(void *parameter)
{
    int cycle_count;

    LOG_I("Emergency alarm thread started");

    while (1) {
        if (alarm_enabled) {
            alarm_running = RT_TRUE;

            /* 鸣叫+闪烁循环 */
            for (cycle_count = 0; cycle_count < ALARM_ON_CYCLES; cycle_count++) {
                if (!alarm_enabled) break;

                buzzer_on();
                alarm_led_on();
                rt_thread_mdelay(ALARM_BLINK_INTERVAL_MS);

                buzzer_off();
                alarm_led_off();
                rt_thread_mdelay(ALARM_BLINK_INTERVAL_MS);
            }

            buzzer_off();

            for (cycle_count = 0; cycle_count < ALARM_OFF_CYCLES; cycle_count++) {
                if (!alarm_enabled) break;
                rt_thread_mdelay(ALARM_BLINK_INTERVAL_MS);
            }

            LOG_D("Alarm cycle completed, continuing...");
        } else {
            buzzer_off();
            alarm_running = RT_FALSE;
            rt_event_send(g_alarm_idle_evt, 1);
            rt_thread_mdelay(500);
        }
    }
}

/* ========================= 启动报警 ========================= */
int alarm_start(void)
{
    if (g_alarm_mutex == RT_NULL || alarm_tid == RT_NULL) {
        LOG_W("Alarm system not initialized yet, ignore start");
        return -1;
    }
    rt_mutex_take(g_alarm_mutex, RT_WAITING_FOREVER);

    if (alarm_enabled) {
        rt_mutex_release(g_alarm_mutex);
        LOG_W("Emergency alarm is already running");
        rt_kprintf("Emergency alarm is already running\n");
        return 0;
    }

    alarm_enabled = RT_TRUE;
    rt_mutex_release(g_alarm_mutex);

    LOG_I("Emergency alarm STARTED");
    rt_kprintf("!!! EMERGENCY ALARM ACTIVATED !!!\n");

    return 0;
}
MSH_CMD_EXPORT(alarm_start, start emergency sound-light alarm);

/* ========================= 停止报警 ========================= */
int alarm_stop(void)
{
    if (g_alarm_mutex == RT_NULL || alarm_tid == RT_NULL) {
        alarm_enabled = RT_FALSE;
        buzzer_off();
        alarm_led_off();
        LOG_W("Alarm system not initialized yet, force stop");
        return 0;
    }
    rt_mutex_take(g_alarm_mutex, RT_WAITING_FOREVER);

    if (!alarm_enabled) {
        rt_mutex_release(g_alarm_mutex);
        LOG_W("Emergency alarm is already stopped");
        rt_kprintf("Emergency alarm is already stopped\n");
        return 0;
    }

    alarm_enabled = RT_FALSE;
    rt_mutex_release(g_alarm_mutex);

    /* 立即停止蜂鸣器和报警灯，不阻塞等待报警线程确认 */
    buzzer_off();
    alarm_led_off();

    LOG_I("Emergency alarm STOPPED");
    rt_kprintf("Emergency alarm STOPPED\n");

    return 0;
}
MSH_CMD_EXPORT(alarm_stop, stop emergency sound-light alarm);

/* ========================= 查询报警状态 ========================= */
int alarm_stat(int argc, char **argv)
{
    rt_kprintf("Emergency Alarm Status: %s\n", alarm_enabled ? "ACTIVE" : "INACTIVE");
    rt_kprintf("  Thread running: %s\n", alarm_running ? "Yes" : "No");
    return 0;
}
MSH_CMD_EXPORT(alarm_stat, show emergency alarm status);

/* ========================= 自动气体检测触发检查
 * 由MQ-2传感器定期调用，浓度超标自动触发
 * ========================= */
rt_bool_t gas_check_alarm(void)
{
    return alarm_enabled;
}

/* ========================= OneNET远程控制报警
 * 在onenet_msg_cb中调用，云端下发alarm=true/false
 * ========================= */
void onenet_set_alarm_state(rt_bool_t enable)
{
    if (enable) {
        LOG_I("OneNET remote command: START alarm");
        rt_kprintf("OneNET remote command: START alarm\n");
        alarm_start();
    } else {
        LOG_I("OneNET remote command: STOP alarm");
        rt_kprintf("OneNET remote command: STOP alarm\n");
        alarm_stop();
    }
}

/* ========================= 报警线程初始化 ========================= */
int alarm_init(void)
{
    buzzer_pwm_init();
    rt_pin_mode(PIN_ALARM_LED, PIN_MODE_OUTPUT);

    buzzer_off();
    alarm_led_off();
    alarm_enabled = RT_FALSE;
    alarm_running = RT_FALSE;

    g_alarm_mutex = rt_mutex_create("alm_mtx", RT_IPC_FLAG_PRIO);
    if (g_alarm_mutex == RT_NULL) {
        LOG_E("Failed to create alarm mutex");
        return -1;
    }

    g_alarm_idle_evt = rt_event_create("alm_idle", RT_IPC_FLAG_FIFO);
    if (g_alarm_idle_evt == RT_NULL) {
        LOG_E("Failed to create alarm idle event");
        return -1;
    }

    alarm_tid = rt_thread_create("emg_alarm",
                                 alarm_thread_entry,
                                 RT_NULL,
                                 STACK_ALARM,
                                 PRIO_ALARM,
                                 TICK_ALARM);

    if (alarm_tid != RT_NULL) {
        rt_thread_startup(alarm_tid);
        LOG_I("Alarm system ready (PRIO=%d, STACK=%d)", PRIO_ALARM, STACK_ALARM);
        return 0;
    } else {
        LOG_E("Failed to create alarm thread");
        return -1;
    }
}
INIT_APP_EXPORT(alarm_init);
MSH_CMD_EXPORT(alarm_init, initialize emergency alarm system);

#endif /* FINSH_USING_MSH */