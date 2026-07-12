/*
 * File      : app_def.h
 * 项目全局定义 - 集中管理所有资源配置
 */
#ifndef __APP_DEF_H__
#define __APP_DEF_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 一、线程优先级分配 (RT-Thread: 数值越小优先级越高)
 * ================================================================
 * 系统保留:     0-1   (idle, tshell 等)
 * 网络/MQTT:    2-3   (lwIP, MQTT 内部)
 * 按键扫描:      4     (安全关键, 最高响应)
 * 门锁控制:      5     (安全关键)
 * 声光报警:      6     (安全关键)
 * K230 串口:    8-9   (AI 视觉指令)
 * OneNET 上传:  10    (数据上报)
 * MQ2 传感器:   12    (周期性采集)
 * 系统监控:     14    (心跳/看门狗)
 * 背景任务:    16+    (低优先级)
 * ================================================================ */
#define PRIO_ALARM          6       /* 声光报警 (安全关键) */
#define PRIO_DOOR_LOCK      5       /* 门锁控制 (安全关键) */
#define PRIO_KEY_SCAN       4       /* 按键扫描 (安全关键, 最高响应) */
#define PRIO_K230_UART      8       /* AI 视觉通信 */
#define PRIO_ONENET_UPLOAD  10      /* 云端数据上传 */
#define PRIO_MQ2_SENSOR     12      /* 气体传感器采集 */
#define PRIO_SYS_MONITOR    14      /* 系统监控 (最低) */

/* ================================================================
 * 二、线程栈大小分配 (单位: 字节)
 * ================================================================ */
#define STACK_ONENET_UPLOAD  2048   /* JSON 格式化 + 浮点运算 */
#define STACK_K230_UART      2048   /* 命令解析 + 串口缓冲 */
#define STACK_DOOR_LOCK      1536   /* 状态机 + 舵机控制链 */
#define STACK_ALARM          1536   /* PWM 初始化 + HAL 库调用 */
#define STACK_MQ2_SENSOR     1536   /* 浮点运算 */
#define STACK_SYS_MONITOR    1024   /* 简单监控 */

/* ================================================================
 * 三、线程时间片分配 (单位: RT-Thread ticks)
 * ================================================================ */
#define TICK_ONENET_UPLOAD   5
#define TICK_K230_UART       10
#define TICK_DOOR_LOCK       10
#define TICK_ALARM           10
#define TICK_MQ2_SENSOR      20
#define TICK_SYS_MONITOR     10

/* ================================================================
 * 四、硬件引脚映射 (STM32F407)
 * ================================================================ */
#define PIN_LED             GET_PIN(C, 8)   /* LED 指示灯 (PC8) */
#define PIN_ALARM_LED       GET_PIN(G, 6)   /* 报警灯 (PG6) */
#define PIN_BUZZER          GET_PIN(E, 2)   /* 蜂鸣器PWM (TIM3_CH2) PE2 */
#define BUZZER_PWM_FREQ      3000            /* 无源蜂鸣器 3kHz */
#define PIN_DOOR_RELAY      GET_PIN(E, 0)   /* 门锁继电器 (低电平吸合) */
#define PIN_MQ2_DO          GET_PIN(D, 0)   /* MQ2 DO 数字输出 (硬件超标触发) */
#define PIN_KEY_DOOR        GET_PIN(C, 2)   /* 按键开门 PC2 */
#define PIN_SERVO1_PWM      GET_PIN(E, 9)   /* 舵机1 (TIM1_CH1 PWM) PE9 */
#define PIN_SERVO2_PWM      GET_PIN(E, 11)  /* 舵机2 (TIM1_CH2 PWM) PE11 */

/* ================================================================
 * 五、传感器参数
 * ================================================================ */
#define ADC_DEV_NAME        "adc2"
#define ADC_DEV_CHANNEL     9
#define REFER_VOLTAGE       330             /* 参考电压 3.3V (x100) */
#define CONVERT_BITS        (1 << 12)       /* 12位 ADC */
#define RL_VALUE            5.0f            /* 负载电阻 (kΩ) */
#define RO_CLEAN_AIR        9.83f           /* 洁净空气 Ro (kΩ) */
#define VC_VOLTAGE          5.0f            /* 供电电压 (V) */
#define GAS_THRESHOLD       1000            /* 气体浓度报警阈值 (PPM) */

/* ================================================================
 * 六、外设名称
 * ================================================================ */
#define SERVO_PWM_DEV_NAME   "pwm1"          /* 舵机 PWM 设备 TIM1 */
#define SERVO_PWM_CHANNEL1   1               /* TIM1_CH1 (PE9) */
#define SERVO_PWM_CHANNEL2   2               /* TIM1_CH2 (PE11) */
#define SERVO_PWM_DEV        1               /* 舵机使用的 PWM 设备号 */
#define K230_UART_DEV       "uart3"
#define K230_BAUD_RATE      115200
#define K230_BUF_SIZE       512

/* ================================================================
 * 七、K230 串口协议定义
 * ================================================================ */

/* K230 帧格式常量 */
#define K230_STX             0xAA        /* 帧起始标记 */
#define K230_ETX             0x55        /* 帧结束标记 */
#define K230_MAX_PAYLOAD     64          /* 最大负载长度 */
#define K230_MAX_FRAME       (7 + K230_MAX_PAYLOAD)  /* STX+LEN+CMD+SEQ+PAYLOAD+CRC16+ETX */
#define K230_FRAME_TIMEOUT   50          /* 帧间超时 (ms) */

/* K230 解析器状态 */
#define K230_STATE_WAIT_STX  0
#define K230_STATE_WAIT_LEN  1
#define K230_STATE_WAIT_DATA 2
#define K230_STATE_WAIT_ETX  3

/* K230 命令定义 */
#define K230_CMD_FACE_UNLOCK_REQ    0x01
#define K230_CMD_FACE_UNLOCK_ACK    0x02
#define K230_CMD_DOOR_CTRL          0x03
#define K230_CMD_DOOR_STATUS_REQ    0x04
#define K230_CMD_DOOR_STATUS_ACK    0x05
#define K230_CMD_HEARTBEAT_REQ      0x06
#define K230_CMD_HEARTBEAT_ACK      0x07
#define K230_CMD_LED_CTRL           0x08
#define K230_CMD_ALARM_CTRL         0x09

/* K230 结果码 */
#define K230_RESULT_OK              0x00
#define K230_RESULT_ERROR           0x01
#define K230_RESULT_BUSY            0x02

/* K230 门状态 */
#define K230_DOOR_LOCKED            0x00
#define K230_DOOR_OPENING           0x01
#define K230_DOOR_OPEN              0x02
#define K230_DOOR_CLOSING           0x03
#define K230_DOOR_ERROR             0xFF

/* K230 解析器结构体 */
typedef struct {
    uint8_t state;              /* 当前状态 */
    uint8_t buf[K230_MAX_FRAME]; /* 帧缓冲区 */
    uint8_t pos;               /* 当前写入位置 */
    uint8_t data_len;          /* 负载长度 */
    uint8_t expected;          /* 期望接收的字节数 */
    rt_tick_t last_byte_tick;  /* 最后收到字节的时间 */
    uint8_t last_seq;          /* 上一个序列号 */
} k230_parser_t;

/* K230 帧结构体 */
typedef struct {
    uint8_t stx;
    uint8_t len;
    uint8_t cmd;
    uint8_t seq;
    uint8_t payload[K230_MAX_PAYLOAD];
    uint8_t crc_lo;
    uint8_t crc_hi;
    uint8_t etx;
} k230_frame_t;

/* K230 全局解析器 */
extern k230_parser_t g_parser;

/* ================================================================
 * 八、OneNET 平台配置
 * ================================================================ */
#define ONENET_PRODUCT_ID   "guQ0KGSGuD"
#define ONENET_DEVICE_NAME  "ceshi"

/* 主题宏 */
#define ONENET_TOPIC_PROPERTY_POST     "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/post"
#define ONENET_TOPIC_PROPERTY_SET      "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/set"
#define ONENET_TOPIC_PROPERTY_SET_REPLY "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/set_reply"
#define ONENET_TOPIC_DESIRED_GET       "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/desired/get"
#define ONENET_TOPIC_DESIRED_GET_REPLY "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/desired/get/reply"

/* ================================================================
 * 九、定时参数
 * ================================================================ */
#define DOOR_OPEN_TIMEOUT_MS    3000    /* 门锁自动关门延时 */
#define SERVO_CLOSE_DELAY_MS    800     /* 舵机关门到位后等待再上锁 */
#define SERVO_SWEEP_TIME_MS     2000    /* 舵机关门缓慢转动时间 */
#define MQ2_SAMPLE_INTERVAL_MS  2000    /* MQ2 采样间隔 */
#define ONENET_UPLOAD_INTERVAL_MS 2000  /* OneNET 上传间隔 */
#define ALARM_BLINK_INTERVAL_MS 100     /* 报警闪烁间隔 */
#define SYS_MONITOR_INTERVAL_MS 5000    /* 系统监控间隔 */
#define MQ2_DO_DEBOUNCE_MS      200     /* DO 触发防抖 (ms) */

#define IWDG_TIMEOUT_MS         8000    /* 独立看门狗超时 (ms) */

/* ================================================================
 * 十、全局互斥锁 & 状态变量
 *     ⚠️ 访问以下变量前必须持有 g_state_mutex
 * ================================================================ */
extern rt_mutex_t   g_state_mutex;      /* 全局互斥锁 (main.c 创建) */
extern float        g_current_ppm;      /* 当前气体浓度 (Mq2.c) */
extern rt_bool_t    g_led_state;        /* LED 状态 (onenet_recv.c) */

/* ================================================================
 * 十一、公共 API 声明
 * ================================================================ */
/* LED 控制 */
void set_led_status(int status);

/* 声光报警 (docker.c) */
int alarm_start(void);
int alarm_stop(void);
rt_bool_t gas_check_alarm(void);

/* 门锁控制 */
rt_err_t    door_lock_open(void);
rt_err_t    door_lock_open_no_auto_close(void);
rt_err_t    door_lock_close(void);

/* 气体检测 */
rt_bool_t   gas_check_alarm(void);

/* 看门狗刷新 (main.c) */
void iwdg_refresh(void);

/* MQ2 传感器继电器过滤 (Mq2.c) */
void mq2_ignore_after_relay_op(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DEF_H__ */