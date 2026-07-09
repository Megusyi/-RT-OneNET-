/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-15     32764       the first version
 * 2026-06-21     32764       use centralized app_def.h
 */
#include <stdlib.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <math.h>
#include "board.h"
#include "app_def.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "mq2.sensor"
#if ONENET_DEBUG
#define DBG_LEVEL           DBG_LOG
#else
#define DBG_LEVEL           DBG_INFO
#endif
#include <rtdbg.h>

/* 全局气体浓度，供 docker.c / onenet_sample.c / 系统监控 读取 */
float g_current_ppm = 0.0f;

/* MQ2 DO 硬件触发标志 (ISR 设置, 线程消费) */
static volatile rt_bool_t g_mq2_do_triggered = RT_FALSE;
static rt_tick_t g_mq2_do_last_trigger = 0;
static rt_tick_t g_mq2_last_alarm_trigger = 0;
/* 继电器操作后禁触窗口：X 秒内忽略 DO 触发，过滤电磁干扰 */
static volatile rt_tick_t g_relay_last_op_tick = 0;
#define RELAY_OP_IGNORE_MS  2000    /* 继电器操作后 2 秒内忽略触发 */

/* DO 恢复正常计数器：连续 N 次读取到 HIGH 才认为确实恢复正常 */
#define DO_NORMAL_CONFIRM_CNT  5     /* 5 次 × 2s 采样间隔 = 10s 确认 */
static int g_do_normal_cnt = 0;

/* 由 door_lock.c 调用，告知 MQ2 刚执行过继电器操作 */
void mq2_ignore_after_relay_op(void)
{
    g_relay_last_op_tick = rt_tick_get();
}

/**
 * Calculate MQ2 sensor resistance Rs based on voltage
 * Vout = VC * Rs / (Rs + RL)
 * Rs = (VC - Vout) * RL / Vout
 *
 * @param vout output voltage in volts
 * @return sensor resistance in kilo ohms
 */
static float calculate_rs(float vout)
{
    if (vout < 0.001f) {
        return 0.0f;  /* avoid division by zero */
    }
    return ((VC_VOLTAGE - vout) * RL_VALUE) / vout;
}

/**
 * Calculate gas concentration in PPM
 * Using MQ2 sensor characteristic curve for LPG
 * ppm = 983.2 * (Rs/Ro)^(-2.146)
 *
 * @param rs sensor resistance in kilo ohms
 * @return gas concentration in PPM
 */
static float calculate_ppm(float rs)
{
    float ratio;
    float ppm;

    if (rs < 0.001f) {
        return 0.0f;
    }

    ratio = rs / RO_CLEAN_AIR;

    /* MQ2 characteristic curve for LPG: ppm = 983.2 * ratio^(-2.146) */
    ppm = 983.2f * pow(ratio, -2.146f);

    return ppm;
}

static rt_adc_device_t g_adc_dev = RT_NULL;

/* MQ2 DO 引脚中断回调 - ISR 上下文, 只设标志位 */
static void mq2_do_irq_callback(void *args)
{
    rt_tick_t now = rt_tick_get();
    if (now - g_mq2_do_last_trigger < rt_tick_from_millisecond(MQ2_DO_DEBOUNCE_MS)) {
        return;
    }
    g_mq2_do_last_trigger = now;
    g_mq2_do_triggered = RT_TRUE;
}

int read_mq2_ppm(float *ppm)
{
    rt_uint32_t value, vol;
    float voltage, rs;

    if (g_adc_dev == RT_NULL)
        return -1;

    value = rt_adc_read(g_adc_dev, ADC_DEV_CHANNEL);
    vol = value * REFER_VOLTAGE / CONVERT_BITS;
    voltage = (float)vol / 100.0f;
    rs = calculate_rs(voltage);
    *ppm = calculate_ppm(rs);
    return 0;
}

void adc1_5_entry(void *parameters)
{
    rt_uint32_t value;
    rt_uint32_t vol;
    float voltage;
    float rs;
    float ppm;
    rt_tick_t now;

    g_adc_dev = (rt_adc_device_t)rt_device_find(ADC_DEV_NAME);
    if (g_adc_dev == RT_NULL) {
        rt_kprintf("[MQ2] ERROR: ADC device '%s' not found!\n", ADC_DEV_NAME);
        return;
    }
    rt_adc_enable(g_adc_dev, ADC_DEV_CHANNEL);

    while (1)
    {
        rt_thread_mdelay(MQ2_SAMPLE_INTERVAL_MS);

        /* 检查 DO 硬件触发标志（中断中设置） */
        if (g_mq2_do_triggered) {
            g_mq2_do_triggered = RT_FALSE;
            now = rt_tick_get();

            /* 禁触窗口：继电器操作后 2 秒内，完全忽略 DO 触发 */
            if (g_relay_last_op_tick != 0 &&
                now - g_relay_last_op_tick < rt_tick_from_millisecond(RELAY_OP_IGNORE_MS)) {
                LOG_I("MQ2 DO triggered but ignored (relay operation within %dms)",
                      RELAY_OP_IGNORE_MS);
                continue;
            }

            /* 防抖：报警触发后 10 秒内不允许再次触发 */
            /* 过滤继电器开关等电磁干扰产生的误触发 */
            if (now - g_mq2_last_alarm_trigger >= rt_tick_from_millisecond(10000)) {
                g_mq2_last_alarm_trigger = now;
                LOG_W("MQ2 DO triggered! Gas concentration exceeded threshold!");
                rt_kprintf("[MQ2] WARNING: DO triggered, starting alarm!\n");
                alarm_start();
                /* 报警触发后自动解锁（不启用自动关门），方便人员疏散 */
                door_lock_open_no_auto_close();
                rt_kprintf("[MQ2] ALARM: door unlocked for evacuation (auto-close disabled)\n");
                g_do_normal_cnt = 0;
            } else {
                LOG_W("MQ2 DO triggered but ignored (debounce: triggered within 10s)");
            }
        }

        value = rt_adc_read(g_adc_dev, ADC_DEV_CHANNEL);
        vol = value * REFER_VOLTAGE / CONVERT_BITS;
        voltage = (float)vol / 100.0f;
        rs = calculate_rs(voltage);
        ppm = calculate_ppm(rs);

        rt_mutex_take(g_state_mutex, RT_WAITING_FOREVER);
        g_current_ppm = ppm;
        rt_mutex_release(g_state_mutex);

        LOG_D("ADC: %d, Voltage: %d.%02dV, Rs: %d.%02d kohm, PPM: %d.%02d",
                   value, vol / 100, vol % 100,
                   (int)rs, (int)((rs - (int)rs) * 100),
                   (int)ppm, (int)((ppm - (int)ppm) * 100));

        /* DO 恢复正常后自动停止报警 */
        if (gas_check_alarm()) {
            if (rt_pin_read(PIN_MQ2_DO) == PIN_HIGH) {
                g_do_normal_cnt++;
                if (g_do_normal_cnt >= DO_NORMAL_CONFIRM_CNT) {
                    g_do_normal_cnt = 0;
                    LOG_I("MQ2 DO returned to normal for %d cycles, auto-stopping alarm",
                          DO_NORMAL_CONFIRM_CNT);
                    rt_kprintf("[MQ2] DO normal, auto-stopping alarm\n");
                    alarm_stop();
                }
            } else {
                g_do_normal_cnt = 0;
            }
        }
    }
}

static int adc_read_volt_sample(void)
{
    static rt_thread_t rt_thread_adc1_5;

    /* ──── 初始化 MQ2 DO 引脚中断 (EXTI0, 下降沿) ──── */
    rt_pin_mode(PIN_MQ2_DO, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(PIN_MQ2_DO, PIN_IRQ_MODE_FALLING,
                      mq2_do_irq_callback, RT_NULL);
    rt_pin_irq_enable(PIN_MQ2_DO, PIN_IRQ_ENABLE);
    rt_kprintf("[MQ2] DO interrupt ready (PD0, EXTI0, falling)\n");

    rt_thread_adc1_5 = rt_thread_create("mq2_sensor",
                                         adc1_5_entry,
                                         RT_NULL,
                                         STACK_MQ2_SENSOR,
                                         PRIO_MQ2_SENSOR,
                                         TICK_MQ2_SENSOR);
    if (rt_thread_adc1_5 != RT_NULL)
    {
        rt_thread_startup(rt_thread_adc1_5);
        rt_kprintf("[MQ2] Sensor thread started (PRIO=%d, STACK=%d)\n",
                   PRIO_MQ2_SENSOR, STACK_MQ2_SENSOR);
    }
    return RT_EOK;
}
INIT_APP_EXPORT(adc_read_volt_sample);