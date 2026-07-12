/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-15     RT-Thread    first version
 * 2026-06-21     32764        system monitor thread
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include "app_def.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static rt_thread_t sys_mon_tid = RT_NULL;
rt_mutex_t g_state_mutex = RT_NULL;
static IWDG_HandleTypeDef hiwdg;

static void sys_monitor_entry(void *parameter)
{
    rt_uint32_t tick_start = rt_tick_get();
    rt_bool_t led_on;
    float ppm;
    int ppm_int, ppm_dec;

    rt_kprintf("\n");
    rt_kprintf("╔══════════════════════════════════════════════╗\n");
    rt_kprintf("║   Smart Home Security System               ║\n");
    rt_kprintf("║   MCU:  STM32F407  |  RTOS: RT-Thread      ║\n");
    rt_kprintf("║   Arch: ARM Cortex-M4  |  CPU: 168MHz      ║\n");
    rt_kprintf("╚══════════════════════════════════════════════╝\n");
    rt_kprintf("\n");
    rt_show_version();
    rt_kprintf("\n");

    LOG_I("System monitor started, all modules initializing...");

    while (1)
    {
        rt_thread_mdelay(SYS_MONITOR_INTERVAL_MS);

        HAL_IWDG_Refresh(&hiwdg);

        rt_uint32_t uptime = (rt_tick_get() - tick_start) / RT_TICK_PER_SECOND;

        rt_mutex_take(g_state_mutex, RT_WAITING_FOREVER);
        ppm = g_current_ppm;
        led_on = g_led_state;
        rt_mutex_release(g_state_mutex);

        ppm_int = (int)ppm;
        ppm_dec = (int)((ppm - ppm_int) * 100);
        if (ppm_dec < 0) ppm_dec = 0;
        if (ppm_dec >= 100) ppm_dec = 99;

        rt_kprintf("\n┌─── System Status [%ds] ───┐\n", uptime);
        rt_kprintf("│ LED:    %-20s │\n", led_on ? "ON" : "OFF");
        rt_kprintf("│ MQ2:    %-16d.%02d PPM │\n", ppm_int, ppm_dec);
        rt_kprintf("│ Alarm:  %-20s │\n",
                   gas_check_alarm() ? "ACTIVE!" : "inactive");
        rt_kprintf("│ Threads: %-18d │\n", rt_list_len(&(rt_object_get_information(RT_Object_Class_Thread)->object_list)));
        rt_kprintf("└──────────────────────────┘\n");
    }
}

/* IWDG 刷新函数，供门锁等关键线程调用 */
void iwdg_refresh(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

/* 在 BOARD 级别初始化互斥锁，确保在所有 APP 线程之前就绪 */
static int sys_mutex_init(void)
{
    g_state_mutex = rt_mutex_create("state_mtx", RT_IPC_FLAG_PRIO);
    if (g_state_mutex == RT_NULL) {
        rt_kprintf("[SYS] FATAL: failed to create state mutex!\n");
        return -RT_ERROR;
    }
    rt_kprintf("[SYS] Global mutex ready (BOARD level)\n");
    return RT_EOK;
}
INIT_BOARD_EXPORT(sys_mutex_init);

static int sys_monitor_init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = (IWDG_TIMEOUT_MS * 32 / 256) - 1;
    if (hiwdg.Init.Reload > 0xFFF) hiwdg.Init.Reload = 0xFFF;
    HAL_IWDG_Init(&hiwdg);
    rt_kprintf("[SYS] IWDG init: timeout=%dms, reload=%d\n",
               IWDG_TIMEOUT_MS, hiwdg.Init.Reload);

    sys_mon_tid = rt_thread_create("sys_mon",
                                    sys_monitor_entry,
                                    RT_NULL,
                                    STACK_SYS_MONITOR,
                                    PRIO_SYS_MONITOR,
                                    TICK_SYS_MONITOR);
    if (sys_mon_tid != RT_NULL)
    {
        rt_thread_startup(sys_mon_tid);
        return RT_EOK;
    }
    return -RT_ERROR;
}
INIT_APP_EXPORT(sys_monitor_init);

int main(void)
{
    return RT_EOK;
}

/* ========================= Finsh/MSH 调试命令 ========================= */
#ifdef RT_USING_FINSH
#include <finsh.h>

static int sys_info(int argc, char **argv)
{
    rt_uint32_t total = 0, used = 0, max_used = 0;

    rt_memory_info(&total, &used, &max_used);

    rt_kprintf("\n┌──────── System Information ────────┐\n");
    rt_kprintf("│ RT-Thread v%s\n", RT_VERSION_STRING);
    rt_kprintf("│ Tick:  %d Hz\n", RT_TICK_PER_SECOND);
    rt_kprintf("│ Heap:  %d B total\n", total);
    rt_kprintf("│        %d B used (%.1f%%)\n", used, used * 100.0f / total);
    rt_kprintf("│        %d B max used\n", max_used);
    rt_kprintf("│ Threads: %d\n",
               rt_list_len(&(rt_object_get_information(RT_Object_Class_Thread)->object_list)));
    rt_kprintf("│ Timers:  %d\n",
               rt_list_len(&(rt_object_get_information(RT_Object_Class_Timer)->object_list)));
    rt_kprintf("│ Mutex:   %d\n",
               rt_list_len(&(rt_object_get_information(RT_Object_Class_Mutex)->object_list)));
    rt_kprintf("│ Event:   %d\n",
               rt_list_len(&(rt_object_get_information(RT_Object_Class_Event)->object_list)));
    rt_kprintf("│ MQ:      %d\n",
               rt_list_len(&(rt_object_get_information(RT_Object_Class_MessageQueue)->object_list)));
    rt_kprintf("└────────────────────────────────────┘\n\n");

    return 0;
}
MSH_CMD_EXPORT(sys_info, show system information);

static int list_threads(int argc, char **argv)
{
    struct rt_object_information *info;
    struct rt_list_node *node;
    struct rt_thread *thread;

    rt_kprintf("\n%-16s %-5s %-5s %-8s %-8s %-8s %-6s\n",
               "Thread", "Prio", "Stat", "StackUsed", "StackMax", "StackFree", "Tick");
    rt_kprintf("───────────────────────────────────────────────────────────────\n");

    info = rt_object_get_information(RT_Object_Class_Thread);
    for (node = info->object_list.next; node != &info->object_list; node = node->next)
    {
        thread = rt_list_entry(node, struct rt_thread, list);
        rt_uint32_t stack_free = (rt_uint32_t)thread->sp - (rt_uint32_t)thread->stack_addr;
        rt_uint32_t stack_total = thread->stack_size;
        rt_uint32_t stack_used = stack_total - stack_free;

        rt_kprintf("%-16s %-5d %-5s %-8d %-8d %-8d %-6d\n",
                   thread->name,
                   thread->current_priority,
                   thread->stat == RT_THREAD_READY ? "RDY" :
                   thread->stat == RT_THREAD_SUSPEND ? "SUSP" :
                   thread->stat == RT_THREAD_RUNNING ? "RUN" :
                   thread->stat == RT_THREAD_CLOSE ? "CLS" : "INIT",
                   stack_used, stack_used, stack_free,
                   thread->remaining_tick);
    }
    rt_kprintf("\n");

    return 0;
}
MSH_CMD_EXPORT(list_threads, list all threads with stack usage);

#endif /* RT_USING_FINSH */