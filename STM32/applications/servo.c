/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-29     32764       Servo motor control - door open/close
 * 2026-07-07                 Migrate to TIM1: PE9=CH1, PE11=CH2
 *
 * 舵机驱动 - 使用 RT-Thread PWM 设备框架
 *   硬件: TIM1_CH1=PE9, TIM1_CH2=PE11
 *   PWM 频率: 50Hz (周期 20ms)
 *   脉宽范围: 0.5ms ~ 2.5ms (角度 0° ~ 180°)
 *   开门角度: 90° (1.5ms)
 *   关门角度: 0°  (0.5ms)
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <stdint.h>
#include "servo.h"
#include "app_def.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "servo"
#define DBG_LEVEL           DBG_LOG
#include <rtdbg.h>

#ifdef FINSH_USING_MSH
#include <finsh.h>

/* ========================= 舵机 PWM 参数 =========================
 * 50Hz PWM: 周期 = 20,000,000 ns (20ms)
 * 0°   = 500,000 ns  (0.5ms, 占空比 2.5%)
 * 90°  = 1,500,000 ns (1.5ms, 占空比 7.5%)
 * 180° = 2,500,000 ns (2.5ms, 占空比 12.5%)
 */
#define SERVO_PERIOD_NS      20000000    /* 20ms = 50Hz */
#define SERVO_PULSE_MIN_NS   500000      /* 0.5ms = 0° */
#define SERVO_PULSE_MAX_NS   2500000     /* 2.5ms = 180° */

static rt_device_t servo_dev = RT_NULL;
static rt_mutex_t  servo_mutex = RT_NULL;
static uint8_t     g_servo_angle[2] = {0, 0};  /* CH1(PE9) / CH2(PE11) 当前角度 */
static volatile rt_bool_t g_servo_busy[2] = {RT_FALSE, RT_FALSE};

/* ========================= 角度转脉宽 ========================= */
static rt_uint32_t angle_to_pulse(uint8_t angle)
{
    rt_uint32_t range = SERVO_PULSE_MAX_NS - SERVO_PULSE_MIN_NS;
    rt_uint32_t pulse;

    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    pulse = SERVO_PULSE_MIN_NS + (range * angle) / SERVO_ANGLE_MAX;

    return pulse;
}

/* ========================= 设置指定通道舵机角度 (立即) ========================= */
int servo_set_angle_ch(uint8_t channel, uint8_t angle)
{
    rt_uint32_t pulse;
    int ch_idx;

    if (servo_dev == RT_NULL) {
        LOG_E("Servo not initialized");
        return -RT_ERROR;
    }

    if (channel == SERVO_PWM_CHANNEL1) {
        ch_idx = 0;
    } else if (channel == SERVO_PWM_CHANNEL2) {
        ch_idx = 1;
    } else {
        LOG_E("Invalid channel: %d", channel);
        return -RT_EINVAL;
    }

    if (angle > SERVO_ANGLE_MAX) {
        LOG_W("Angle %d > max %d, clamped", angle, SERVO_ANGLE_MAX);
        angle = SERVO_ANGLE_MAX;
    }

    pulse = angle_to_pulse(angle);

    rt_mutex_take(servo_mutex, RT_WAITING_FOREVER);
    rt_pwm_set(servo_dev, channel, SERVO_PERIOD_NS, pulse);
    rt_pwm_enable(servo_dev, channel);
    g_servo_angle[ch_idx] = angle;
    rt_mutex_release(servo_mutex);

    LOG_D("Servo CH%d angle: %d° (pulse=%d ns)", channel, angle, pulse);
    return RT_EOK;
}

/* ========================= 设置默认舵机 (CH1=PE9) 角度 ========================= */
int servo_set_angle(uint8_t angle)
{
    return servo_set_angle_ch(SERVO_PWM_CHANNEL, angle);
}

/* ========================= 缓慢转动指定通道到目标角度 ========================= */
int servo_sweep_ch(uint8_t channel, uint8_t target_angle, uint32_t duration_ms)
{
    int8_t dir;
    uint8_t cur, steps;
    uint32_t step_delay;
    int ch_idx;

    if (servo_dev == RT_NULL) {
        LOG_E("Servo not initialized");
        return -RT_ERROR;
    }

    if (channel == SERVO_PWM_CHANNEL1) {
        ch_idx = 0;
    } else if (channel == SERVO_PWM_CHANNEL2) {
        ch_idx = 1;
    } else {
        LOG_E("Invalid channel: %d", channel);
        return -RT_EINVAL;
    }

    if (target_angle > SERVO_ANGLE_MAX) target_angle = SERVO_ANGLE_MAX;

    /* 防止并发 sweep 操作导致角度状态混乱 */
    if (g_servo_busy[ch_idx]) {
        LOG_W("Servo CH%d is busy with another sweep, ignored", channel);
        return -RT_EBUSY;
    }
    g_servo_busy[ch_idx] = RT_TRUE;

    rt_mutex_take(servo_mutex, RT_WAITING_FOREVER);
    cur = g_servo_angle[ch_idx];
    rt_mutex_release(servo_mutex);

    if (cur == target_angle) {
        g_servo_busy[ch_idx] = RT_FALSE;
        return RT_EOK;
    }

    dir = (target_angle > cur) ? 1 : -1;
    steps = (dir > 0) ? (target_angle - cur) : (cur - target_angle);

    if (steps == 0) steps = 1;
    step_delay = duration_ms / steps;
    if (step_delay < 5) step_delay = 5;

    LOG_I("Servo CH%d sweep: %d° -> %d° (%d steps, %dms/step, ~%dms total)",
          channel, cur, target_angle, steps, step_delay, steps * step_delay);

    while (cur != target_angle) {
        cur += dir;
        servo_set_angle_ch(channel, cur);
        rt_thread_mdelay(step_delay);
    }

    g_servo_busy[ch_idx] = RT_FALSE;
    return RT_EOK;
}

/* ========================= 缓慢转动默认舵机 ========================= */
int servo_sweep(uint8_t target_angle, uint32_t duration_ms)
{
    return servo_sweep_ch(SERVO_PWM_CHANNEL, target_angle, duration_ms);
}

/* ========================= 开门 (默认 90°) ========================= */
int servo_open_door(void)
{
    LOG_I("Servo opening door -> %d°", SERVO_ANGLE_OPEN);
    return servo_set_angle(SERVO_ANGLE_OPEN);
}

/* ========================= 关门 (缓慢到 0°) ========================= */
int servo_close_door(void)
{
    LOG_I("Servo closing door slowly -> %d°", SERVO_ANGLE_CLOSED);
    return servo_sweep(SERVO_ANGLE_CLOSED, SERVO_SWEEP_TIME_MS);
}

/* ========================= 获取默认舵机当前角度 ========================= */
int servo_get_angle(void)
{
    return servo_get_angle_ch(SERVO_PWM_CHANNEL);
}

/* ========================= 获取指定通道当前角度 ========================= */
int servo_get_angle_ch(uint8_t channel)
{
    int angle, ch_idx;

    if (servo_mutex == RT_NULL) return -1;

    if (channel == SERVO_PWM_CHANNEL1) {
        ch_idx = 0;
    } else if (channel == SERVO_PWM_CHANNEL2) {
        ch_idx = 1;
    } else {
        return -1;
    }

    rt_mutex_take(servo_mutex, RT_WAITING_FOREVER);
    angle = g_servo_angle[ch_idx];
    rt_mutex_release(servo_mutex);
    return angle;
}

/* ========================= 停止 PWM 输出 ========================= */
int servo_deinit(void)
{
    if (servo_dev != RT_NULL) {
        rt_pwm_disable(servo_dev, SERVO_PWM_CHANNEL1);
        rt_pwm_disable(servo_dev, SERVO_PWM_CHANNEL2);
        rt_device_close(servo_dev);
        servo_dev = RT_NULL;
    }
    if (servo_mutex != RT_NULL) {
        rt_mutex_delete(servo_mutex);
        servo_mutex = RT_NULL;
    }
    g_servo_angle[0] = 0;
    g_servo_angle[1] = 0;
    LOG_I("Servo deinitialized");
    return RT_EOK;
}

/* ========================= MSH 控制台命令 ========================= */

int sv_angle(int argc, char **argv)
{
    int ch = SERVO_PWM_CHANNEL;

    if (argc < 2) {
        rt_kprintf("Usage: sv_angle <0-180> [ch=1]\n");
        rt_kprintf("  ch=1: PE9 (TIM1_CH1), ch=2: PE11 (TIM1_CH2)\n");
        rt_kprintf("Current CH1: %d°, CH2: %d°\n",
                   servo_get_angle_ch(SERVO_PWM_CHANNEL1),
                   servo_get_angle_ch(SERVO_PWM_CHANNEL2));
        return 0;
    }

    int angle = atoi(argv[1]);
    if (argc > 2) ch = atoi(argv[2]);

    if (ch < 1) ch = 1;
    if (ch > 2) ch = 2;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    if (servo_set_angle_ch((uint8_t)ch, (uint8_t)angle) == RT_EOK) {
        rt_kprintf("Servo CH%d set to %d°\n", ch, angle);
    } else {
        rt_kprintf("Failed to set servo angle\n");
    }
    return 0;
}
MSH_CMD_EXPORT(sv_angle, set servo angle - sv_angle <0-180> [ch=1|2]);

int sv_sweep(int argc, char **argv)
{
    int angle, duration, ch = SERVO_PWM_CHANNEL;

    if (argc < 2) {
        rt_kprintf("Usage: sv_sweep <0-180> [time_ms] [ch=1]\n");
        rt_kprintf("  Slowly sweep to target angle\n");
        rt_kprintf("  ch=1: PE9, ch=2: PE11\n");
        rt_kprintf("  Default time: %dms\n", SERVO_SWEEP_TIME_MS);
        rt_kprintf("Current CH1: %d°, CH2: %d°\n",
                   servo_get_angle_ch(SERVO_PWM_CHANNEL1),
                   servo_get_angle_ch(SERVO_PWM_CHANNEL2));
        return 0;
    }

    angle = atoi(argv[1]);
    duration = (argc > 2) ? atoi(argv[2]) : SERVO_SWEEP_TIME_MS;
    if (argc > 3) ch = atoi(argv[3]);

    if (ch < 1) ch = 1;
    if (ch > 2) ch = 2;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    if (duration < 100) duration = 100;

    if (servo_sweep_ch((uint8_t)ch, (uint8_t)angle, (uint32_t)duration) == RT_EOK) {
        rt_kprintf("Servo CH%d sweep to %d° in %dms done\n", ch, angle, duration);
    } else {
        rt_kprintf("Failed to sweep servo\n");
    }
    return 0;
}
MSH_CMD_EXPORT(sv_sweep, sweep servo slowly - sv_sweep <0-180> [time_ms] [ch=1|2]);

int sv_open(int argc, char **argv)
{
    if (servo_open_door() == RT_EOK) {
        rt_kprintf("Door opened (servo %d°)\n", SERVO_ANGLE_OPEN);
    } else {
        rt_kprintf("Failed to open door\n");
    }
    return 0;
}
MSH_CMD_EXPORT(sv_open, open door via servo);

int sv_close(int argc, char **argv)
{
    if (servo_close_door() == RT_EOK) {
        rt_kprintf("Door closed (servo %d°)\n", SERVO_ANGLE_CLOSED);
    } else {
        rt_kprintf("Failed to close door\n");
    }
    return 0;
}
MSH_CMD_EXPORT(sv_close, close door via servo);

int sv_stat(int argc, char **argv)
{
    rt_kprintf("=== Servo Status ===\n");
    rt_kprintf("Device: %s (TIM1)\n", SERVO_PWM_DEV_NAME);
    rt_kprintf("Frequency: 50Hz (period=%d ns)\n", SERVO_PERIOD_NS);
    rt_kprintf("CH1 (PE9): %d°\n", servo_get_angle_ch(SERVO_PWM_CHANNEL1));
    rt_kprintf("CH2 (PE11): %d°\n", servo_get_angle_ch(SERVO_PWM_CHANNEL2));
    rt_kprintf("PWM device: %s\n", servo_dev ? "open" : "closed");
    return 0;
}
MSH_CMD_EXPORT(sv_stat, show servo status);

/* ========================= 初始化 ========================= */

int servo_init(void)
{
    if (servo_dev != RT_NULL) {
        LOG_W("Servo already initialized");
        return RT_EOK;
    }

    /* 查找 PWM 设备 */
    servo_dev = rt_device_find(SERVO_PWM_DEV_NAME);
    if (servo_dev == RT_NULL) {
        LOG_E("Cannot find PWM device: %s", SERVO_PWM_DEV_NAME);
        rt_kprintf("[Servo] ERROR: %s not found, check PWM config\n", SERVO_PWM_DEV_NAME);
        return -RT_ERROR;
    }

    /* 创建互斥锁 */
    servo_mutex = rt_mutex_create("servo_mtx", RT_IPC_FLAG_PRIO);
    if (servo_mutex == RT_NULL) {
        LOG_E("Failed to create servo mutex");
        return -RT_ENOMEM;
    }

    /* 设置 PWM 周期和初始脉宽 (关门位置) */
    rt_pwm_set(servo_dev, SERVO_PWM_CHANNEL1, SERVO_PERIOD_NS, SERVO_PULSE_MIN_NS);
    rt_pwm_enable(servo_dev, SERVO_PWM_CHANNEL1);
    rt_pwm_set(servo_dev, SERVO_PWM_CHANNEL2, SERVO_PERIOD_NS, SERVO_PULSE_MIN_NS);
    rt_pwm_enable(servo_dev, SERVO_PWM_CHANNEL2);
    g_servo_angle[0] = 0;
    g_servo_angle[1] = 0;

    LOG_I("Servo ready: %s, CH1=PE9 CH2=PE11, 50Hz", SERVO_PWM_DEV_NAME);
    rt_kprintf("=== Servo Ready ===\n");
    rt_kprintf("  PWM: %s, 50Hz\n", SERVO_PWM_DEV_NAME);
    rt_kprintf("  CH1: PE9 | CH2: PE11\n");
    rt_kprintf("  Closed: %d° | Open: %d°\n", SERVO_ANGLE_CLOSED, SERVO_ANGLE_OPEN);

    return RT_EOK;
}
INIT_APP_EXPORT(servo_init);

#endif /* FINSH_USING_MSH */