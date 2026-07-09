/*
 * File      : onenet_sample.c
 * 数据上传模块 - 自动定时上传传感器数据到OneNET云平台
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <rtdevice.h>
#include <sensor.h>

#include <onenet.h>
#include "app_def.h"
#include "door_lock.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "onenet.sample"
#if ONENET_DEBUG
#define DBG_LEVEL           DBG_LOG
#else
#define DBG_LEVEL           DBG_INFO
#endif

#include <rtdbg.h>

#ifdef FINSH_USING_MSH
#include <finsh.h>

static rt_device_t dht11_dev = RT_NULL;

static int read_dht11_data(int *temp, int *humi)
{
    struct rt_sensor_data sensor_data;
    rt_size_t res;
    if (dht11_dev == RT_NULL) {
        dht11_dev = rt_device_find("temp_dht11");
        if (dht11_dev == RT_NULL) return -1;
        if (rt_device_open(dht11_dev, RT_DEVICE_FLAG_RDONLY) != RT_EOK) return -1;
    }
    res = rt_device_read(dht11_dev, 0, &sensor_data, 1);
    if (res != 1) return -1;
    if (sensor_data.data.temp >= 0) {
        *temp = (sensor_data.data.temp & 0xffff) >> 0;
        *humi = (sensor_data.data.temp & 0xffff0000) >> 16;
    }
    return 0;
}

static void onenet_upload_entry(void *parameter)
{
    float mq2 = 0.0f;
    int temp = 0, humi = 0;
    int ret;
    char json_buf[256];
    int mq2_int, mq2_dec;
    rt_bool_t led_on;
    rt_bool_t lock_open;

    LOG_I("OneNET upload thread started");

    while (1) {
        rt_mutex_take(g_state_mutex, RT_WAITING_FOREVER);
        mq2 = g_current_ppm;
        led_on = g_led_state;
        rt_mutex_release(g_state_mutex);

        ret = read_dht11_data(&temp, &humi);
        if (ret != 0) { temp = 0; humi = 0; }

        lock_open = door_lock_is_open();

        mq2_int = (int)mq2;
        mq2_dec = (int)((mq2 - mq2_int) * 100);
        if (mq2_dec < 0) mq2_dec = 0;
        if (mq2_dec >= 100) mq2_dec = 99;

        LOG_I("MQ2: %d.%02d, Temp: %d, Humi: %d, Lock: %d", mq2_int, mq2_dec, temp, humi, lock_open);

        const char *led_value = led_on ? "true" : "false";
        const char *lock_value = lock_open ? "false" : "true";

        rt_sprintf(json_buf,
            "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
            "\"MQ\":{\"value\":%d.%02d},"
            "\"DHT\":{\"value\":%d},"
            "\"humidity\":{\"value\":%d},"
            "\"led\":{\"value\":%s},"
            "\"lock\":{\"value\":%s}}}",
            mq2_int, mq2_dec, temp, humi, led_value, lock_value);

        if (onenet_mqtt_publish(ONENET_TOPIC_PROPERTY_POST,
                                (uint8_t*)json_buf, strlen(json_buf)) < 0) {
            LOG_E("upload failed");
        } else {
            LOG_D("upload success");
        }

        rt_thread_delay(rt_tick_from_millisecond(ONENET_UPLOAD_INTERVAL_MS));
    }
}

int onenet_upload_cycle(void)
{
    rt_thread_t tid;
    tid = rt_thread_create("on_upload",
                           onenet_upload_entry,
                           RT_NULL,
                           STACK_ONENET_UPLOAD,
                           PRIO_ONENET_UPLOAD,
                           TICK_ONENET_UPLOAD);
    if (tid) {
        rt_thread_startup(tid);
        rt_kprintf("[OneNET] Upload thread started (PRIO=%d)\n", PRIO_ONENET_UPLOAD);
        return 0;
    }
    return -1;
}
MSH_CMD_EXPORT(onenet_upload_cycle, start OneNET upload thread);

static int onenet_upload_auto_init(void)
{
    return onenet_upload_cycle();
}
INIT_APP_EXPORT(onenet_upload_auto_init);

int onenet_publish_digit(int argc, char **argv)
{
    if (argc != 3) {
        LOG_E("onenet_publish [datastream_id] [value]");
        return -1;
    }
    if (onenet_mqtt_upload_digit(argv[1], atoi(argv[2])) < 0) {
        LOG_E("upload digit data error!\n");
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(onenet_publish_digit, onenet_mqtt_publish_digit, send digit data);

int onenet_publish_string(int argc, char **argv)
{
    if (argc != 3) {
        LOG_E("onenet_publish [datastream_id] [string]");
        return -1;
    }
    if (onenet_mqtt_upload_string(argv[1], argv[2]) < 0) {
        LOG_E("upload string error!\n");
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(onenet_publish_string, onenet_mqtt_publish_string, send string data);

#endif /* FINSH_USING_MSH */