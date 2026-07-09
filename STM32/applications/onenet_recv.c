/*
 * File      : onenet_recv.c
 * OneNET 命令接收 - 自动初始化 MQTT 并订阅物模型主题
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <onenet.h>
#include <cJSON.h>
#include <paho_mqtt.h>
#include "door_lock.h"
#include "app_def.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "onenet.recv"
#if ONENET_DEBUG
#define DBG_LEVEL           DBG_LOG
#else
#define DBG_LEVEL           DBG_INFO
#endif
#include <rtdbg.h>

#ifdef FINSH_USING_MSH
#include <finsh.h>

rt_bool_t g_led_state = RT_FALSE;
static rt_mq_t g_led_mq = RT_NULL;

extern int onenet_mqtt_subscribe(const char *topic, enum QoS qos);

/* ====================== LED 控制线程 ====================== */
static void led_thread_entry(void *parameter)
{
    rt_pin_mode(PIN_LED, PIN_MODE_OUTPUT);
    rt_pin_write(PIN_LED, PIN_LOW);  /* 默认熄灭 */

    while (1) {
        int cmd;
        /* 阻塞等待消息，回调死锁不影响这里 */
        if (rt_mq_recv(g_led_mq, &cmd, sizeof(cmd), RT_WAITING_FOREVER) == RT_EOK) {
            if (cmd) {
                rt_pin_write(PIN_LED, PIN_HIGH);  /* 高电平点亮 */
                LOG_I("LED ON");
            } else {
                rt_pin_write(PIN_LED, PIN_LOW);   /* 低电平熄灭 */
                LOG_I("LED OFF");
            }
        }
    }
}

void set_led_status(int status)
{
    if (g_state_mutex != RT_NULL) {
        rt_mutex_take(g_state_mutex, RT_WAITING_FOREVER);
    }
    g_led_state = (status != 0) ? RT_TRUE : RT_FALSE;
    if (g_state_mutex != RT_NULL) {
        rt_mutex_release(g_state_mutex);
    }

    /* 投递消息到队列（非阻塞，回调后续死锁不影响） */
    if (g_led_mq != RT_NULL) {
        int cmd = g_led_state ? 1 : 0;
        rt_mq_send(g_led_mq, &cmd, sizeof(cmd));
    }
}

/* ====================== 命令处理回调 ====================== */
static void onenet_cmd_handler(uint8_t *recv_data, size_t recv_size,
                               uint8_t **resp_data, size_t *resp_size)
{
    cJSON *root = NULL;
    cJSON *params = NULL;
    cJSON *item = NULL;
    char res_buf[256] = {0};

    rt_kprintf("\n=== OneNET Command Received ===\n");
    rt_kprintf("%.*s\n", (int)recv_size, recv_data);

    /* 确保 null 结尾，cJSON_Parse 需要 */
    char json_buf[512];
    if (recv_size >= sizeof(json_buf)) {
        rt_kprintf("JSON too large: %d bytes\n", recv_size);
        rt_snprintf(res_buf, sizeof(res_buf), "{\"code\":-1,\"msg\":\"too large\"}");
        goto __response;
    }
    memcpy(json_buf, recv_data, recv_size);
    json_buf[recv_size] = '\0';

    root = cJSON_Parse(json_buf);
    if (root == NULL) {
        rt_kprintf("JSON parse failed\n");
        rt_snprintf(res_buf, sizeof(res_buf), "{\"code\":-1,\"msg\":\"parse error\"}");
        goto __response;
    }

    /* 提取 id 用于响应 */
    char id_str[64] = "";
    cJSON *msg_id = cJSON_GetObjectItem(root, "id");
    if (msg_id && cJSON_IsString(msg_id)) {
        strncpy(id_str, msg_id->valuestring, sizeof(id_str) - 1);
    }

    params = cJSON_GetObjectItem(root, "params");
    if (params == NULL) {
        /* 兼容 desired/get/reply 格式: {"data":{"led":{"value":true}}} */
        params = cJSON_GetObjectItem(root, "data");
    }
    if (params == NULL) {
        rt_kprintf("No params/data field\n");
        rt_snprintf(res_buf, sizeof(res_buf), "{\"code\":-1,\"msg\":\"no params\"}");
        goto __response;
    }

    /* ----- LED 控制 ----- */
    item = cJSON_GetObjectItem(params, "led");
    if (item != NULL) {
        /* OneNET 物模型格式: {"led": {"value": true}} */
        cJSON *value = cJSON_GetObjectItem(item, "value");
        if (value == NULL) value = item; /* 兼容直接传值的情况 */

        if (cJSON_IsTrue(value)) {
            set_led_status(1);
            rt_kprintf("LED: ON\n");
        } else if (cJSON_IsFalse(value)) {
            set_led_status(0);
            rt_kprintf("LED: OFF\n");
        } else if (cJSON_IsNumber(value)) {
            set_led_status(value->valueint != 0 ? 1 : 0);
            rt_kprintf("LED: %s (number)\n", value->valueint != 0 ? "ON" : "OFF");
        } else {
            rt_kprintf("LED: unsupported value type, ignored\n");
        }
    }

    /* ----- 门锁控制 ----- */
    item = cJSON_GetObjectItem(params, "lock");
    if (item != NULL) {
        cJSON *value = cJSON_GetObjectItem(item, "value");
        if (value == NULL) value = item;

        if (cJSON_IsTrue(value) || (cJSON_IsNumber(value) && value->valueint != 0)) {
            door_lock_close();
            rt_kprintf("Door: CLOSE command (lock=true)\n");
        } else {
            door_lock_open();
            rt_kprintf("Door: OPEN command (lock=false)\n");
        }
    }

    /* ----- 报警控制 ----- */
    item = cJSON_GetObjectItem(params, "alarm");
    if (item != NULL) {
        cJSON *value = cJSON_GetObjectItem(item, "value");
        if (value == NULL) value = item;

        if (cJSON_IsTrue(value) || (cJSON_IsNumber(value) && value->valueint != 0)) {
            alarm_start();
            rt_kprintf("Alarm: ON\n");
        } else {
            alarm_stop();
            rt_kprintf("Alarm: OFF\n");
        }
    }

    rt_snprintf(res_buf, sizeof(res_buf),
        "{\"id\":\"%s\",\"code\":0,\"msg\":\"ok\"}", id_str);

    /* 不再手动 publish 响应，由框架自动通过 $crsp/... 回复 */
    /* 避免在 MQTT 回调中再次调用 MQTTPublish 导致死锁 */

__response:
    if (root != NULL) {
        cJSON_Delete(root);
    }

    *resp_data = (uint8_t *)ONENET_MALLOC(strlen(res_buf) + 1);
    if (*resp_data != NULL) {
        strcpy((char *)*resp_data, res_buf);
        *resp_size = strlen(res_buf);
        rt_kprintf("Response: %s\n", res_buf);
    }
}

/* ====================== 自动初始化 ====================== */
int on_recv(void)
{
    LOG_I("Initializing OneNET MQTT...");
    if (onenet_mqtt_init() < 0) {
        LOG_E("OneNET MQTT init failed!");
        return -1;
    }

    onenet_mqtt_subscribe(ONENET_TOPIC_PROPERTY_SET, QOS1);
    onenet_mqtt_subscribe(ONENET_TOPIC_DESIRED_GET_REPLY, QOS1);
    LOG_I("Subscribed to property/set and desired/get/reply");

    onenet_set_cmd_rsp_cb(onenet_cmd_handler);
    LOG_I("Command callback registered");

    rt_kprintf("=== OneNET Ready ===\n");
    return 0;
}
MSH_CMD_EXPORT(on_recv, start OneNET MQTT receiver);

static void onenet_retry_entry(void *parameter)
{
    int retry;

    /* 先等待 ESP8266 初始化完成（异步初始化需要时间） */
    rt_kprintf("[OneNET] Waiting for network ready...\n");
    rt_thread_mdelay(5000);

    for (retry = 0; retry < 30; retry++) {
        if (on_recv() == 0) return;
        rt_kprintf("[OneNET] Waiting for network... retry %d/30\n", retry + 1);
        rt_thread_mdelay(3000);
    }
    rt_kprintf("[OneNET] Auto-init failed after 30 retries\n");
    rt_kprintf("[OneNET] Please check:\n");
    rt_kprintf("  1. ESP8266 wiring (UART2 TX/RX)\n");
    rt_kprintf("  2. WiFi SSID and password\n");
    rt_kprintf("  3. Run 'ifconfig' to check IP\n");
    rt_kprintf("  4. Run 'on_recv' to retry manually\n");
}

static int onenet_auto_init(void)
{
    rt_thread_t tid;

    /* 创建 LED 消息队列（容量 4，回调死锁前消息已入队） */
    g_led_mq = rt_mq_create("led_mq", sizeof(int), 4, RT_IPC_FLAG_FIFO);
    if (g_led_mq == RT_NULL) {
        rt_kprintf("[LED] Failed to create mq\n");
        return -1;
    }

    /* 创建 LED 控制线程（独立于 MQTT 回调，不受死锁影响） */
    tid = rt_thread_create("led_sw",
                            led_thread_entry,
                            RT_NULL, 1024, 22, 10);
    if (tid) {
        rt_thread_startup(tid);
        rt_kprintf("[LED] Message-queue mode started\n");
    } else {
        rt_kprintf("[LED] Failed to create thread\n");
        return -1;
    }

    /* 创建 OneNET 重连线程 */
    tid = rt_thread_create("on_retry",
                            onenet_retry_entry,
                            RT_NULL, 1536, 20, 10);
    if (tid) {
        rt_thread_startup(tid);
        return 0;
    }
    return -1;
}
INIT_APP_EXPORT(onenet_auto_init);

/* ====================== 主动拉取物模型 ====================== */
int on_query(void)
{
    char *payload = "{\"id\":\"123\",\"version\":\"1.0\",\"params\":[\"led\",\"lock\",\"alarm\"]}";
    rt_kprintf("=== Querying desired properties ===\n");
    onenet_mqtt_publish(ONENET_TOPIC_DESIRED_GET,
                        (uint8_t *)payload, strlen(payload));
    return 0;
}
MSH_CMD_EXPORT(on_query, query desired properties from OneNET);

/* ====================== 状态查询 ====================== */
int on_led(int argc, char **argv)
{
    rt_kprintf("LED status: %s\n", g_led_state ? "ON" : "OFF");
    rt_kprintf("PA7 raw: %d\n", rt_pin_read(PIN_LED));
    return 0;
}
MSH_CMD_EXPORT(on_led, get LED status);

/* 直接控制 LED (绕过信号量机制，用于测试) */
int led_test(int argc, char **argv)
{
    if (argc < 2) {
        rt_kprintf("Usage: led_test <on|off|toggle>\n");
        rt_kprintf("Current PA7: %d\n", rt_pin_read(PIN_LED));
        return 0;
    }

    /* 直接操作 PA7 */
    if (strcmp(argv[1], "on") == 0) {
        rt_pin_write(PIN_LED, PIN_HIGH);
        rt_kprintf("PA7 -> HIGH (LED ON)\n");
    } else if (strcmp(argv[1], "off") == 0) {
        rt_pin_write(PIN_LED, PIN_LOW);
        rt_kprintf("PA7 -> LOW (LED OFF)\n");
    } else if (strcmp(argv[1], "toggle") == 0) {
        int val = rt_pin_read(PIN_LED);
        rt_pin_write(PIN_LED, val ? PIN_LOW : PIN_HIGH);
        rt_kprintf("PA7 toggled: %d -> %d\n", val, rt_pin_read(PIN_LED));
    }

    rt_kprintf("Verify: PA7 now = %d\n", rt_pin_read(PIN_LED));
    return 0;
}
MSH_CMD_EXPORT(led_test, direct LED control - led_test <on|off|toggle>);

#endif /* FINSH_USING_MSH */