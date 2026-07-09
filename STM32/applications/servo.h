/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-29     32764       Servo motor control for door
 */
#ifndef __SERVO_H__
#define __SERVO_H__

#include <rtthread.h>

/* 舵机角度范围 */
#define SERVO_ANGLE_CLOSED    0     /* 关门角度 */
#define SERVO_ANGLE_OPEN      90    /* 开门角度 */
#define SERVO_ANGLE_MIN       0     /* 最小角度 */
#define SERVO_ANGLE_MAX       180   /* 最大角度 */

/* 舵机通道选择 (兼容旧代码) */
#define SERVO_PWM_CHANNEL     SERVO_PWM_CHANNEL1   /* 默认舵机 = PE9 */

/* 舵机 API */
int  servo_init(void);
int  servo_set_angle(uint8_t angle);
int  servo_set_angle_ch(uint8_t channel, uint8_t angle);
int  servo_sweep(uint8_t target_angle, uint32_t duration_ms);
int  servo_sweep_ch(uint8_t channel, uint8_t target_angle, uint32_t duration_ms);
int  servo_open_door(void);
int  servo_close_door(void);
int  servo_get_angle(void);
int  servo_get_angle_ch(uint8_t channel);
int  servo_deinit(void);

#endif /* __SERVO_H__ */