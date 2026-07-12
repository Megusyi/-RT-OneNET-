/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-16     32764       Door lock public header
 */
#ifndef __DOOR_LOCK_H__
#define __DOOR_LOCK_H__

#include <rtthread.h>

/* 门锁状态枚举 */
typedef enum {
    DOOR_LOCKED   = 0,
    DOOR_OPENING  = 1,
    DOOR_OPEN     = 2,
    DOOR_CLOSING  = 3,
    DOOR_ERROR    = 4,
} door_state_t;

/* 门锁 API */
rt_err_t   door_lock_open(void);
rt_err_t   door_lock_close(void);
rt_bool_t  door_lock_is_open(void);
door_state_t door_lock_get_state(void);

#endif /* __DOOR_LOCK_H__ */