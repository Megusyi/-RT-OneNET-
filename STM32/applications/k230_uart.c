/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-16     32764       K230 UART command receiver & door control
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "door_lock.h"
#include "app_def.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME    "k230.uart"
#if ONENET_DEBUG
#define DBG_LEVEL           DBG_LOG
#else
#define DBG_LEVEL           DBG_INFO
#endif
#include <rtdbg.h>

#ifdef FINSH_USING_MSH
#include <finsh.h>

static rt_device_t k230_serial = RT_NULL;
static rt_thread_t  k230_tid = RT_NULL;

/* 全局解析器实例 */
k230_parser_t g_parser;

/* ========================= CRC16 (Modbus) ========================= */
uint16_t k230_crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    uint8_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/* ========================= 帧解析器初始化 ========================= */
void k230_parser_init(k230_parser_t *parser)
{
    parser->state = K230_STATE_WAIT_STX;
    parser->pos = 0;
    parser->data_len = 0;
    parser->expected = 0;
    parser->last_byte_tick = 0;
    parser->last_seq = 0xFF;  /* 让第一个 SEQ 总是有效 */
}

/* ========================= 帧解析器喂字节 =========================
 * 返回: 1 = 完整帧已解析 (out_frame 有效)
 *       0 = 继续等待
 *      -1 = 帧错误 (CRC/超时/格式), 已自动复位
 */
int k230_parser_feed(k230_parser_t *parser, uint8_t byte,
                     k230_frame_t *out_frame)
{
    rt_tick_t now = rt_tick_get();

    /* 帧间超时检测: 超过 50ms 未收到新字节 → 复位状态机 */
    if (parser->state != K230_STATE_WAIT_STX &&
        (now - parser->last_byte_tick) > rt_tick_from_millisecond(K230_FRAME_TIMEOUT)) {
        LOG_W("Frame timeout, reset parser");
        parser->state = K230_STATE_WAIT_STX;
        parser->pos = 0;
    }
    parser->last_byte_tick = now;

    switch (parser->state) {

    case K230_STATE_WAIT_STX:
        if (byte == K230_STX) {
            parser->buf[0] = byte;
            parser->pos = 1;
            parser->state = K230_STATE_WAIT_LEN;
        }
        /* 非 STX 字节直接丢弃 */
        break;

    case K230_STATE_WAIT_LEN:
        if (byte > K230_MAX_PAYLOAD) {
            LOG_W("Invalid LEN=%d, reset", byte);
            parser->state = K230_STATE_WAIT_STX;
            parser->pos = 0;
            break;
        }
        parser->buf[parser->pos++] = byte;
        parser->data_len = byte;
        /* 需要读取: CMD(1) + SEQ(1) + PAYLOAD(N) + CRC16(2) = N+4 */
        parser->expected = byte + 4;
        parser->state = K230_STATE_WAIT_DATA;
        break;

    case K230_STATE_WAIT_DATA:
        if (parser->pos < K230_MAX_FRAME) {
            parser->buf[parser->pos++] = byte;
        } else {
            LOG_W("Buffer overflow, reset parser");
            parser->state = K230_STATE_WAIT_STX;
            parser->pos = 0;
            break;
        }
        if (parser->pos >= 2 + parser->expected) {
            parser->state = K230_STATE_WAIT_ETX;
        }
        break;

    case K230_STATE_WAIT_ETX:
        if (byte == K230_ETX) {
            uint8_t *p = parser->buf;
            uint8_t  len = p[1];
            uint8_t  cmd = p[2];
            uint8_t  seq = p[3];
            uint8_t  crc_lo = p[4 + len];
            uint8_t  crc_hi = p[5 + len];
            uint16_t crc_rx = ((uint16_t)crc_hi << 8) | crc_lo;
            uint16_t crc_calc = k230_crc16(&p[2], 2 + len);  /* CRC over CMD+SEQ+PAYLOAD */

            if (crc_rx == crc_calc) {
                /* 填充输出帧 */
                out_frame->stx = K230_STX;
                out_frame->len = len;
                out_frame->cmd = cmd;
                out_frame->seq = seq;
                memcpy(out_frame->payload, &p[4], len);
                out_frame->crc_lo = crc_lo;
                out_frame->crc_hi = crc_hi;
                out_frame->etx = K230_ETX;

                parser->state = K230_STATE_WAIT_STX;
                parser->pos = 0;
                return 1;  /* 完整帧 */
            } else {
                LOG_W("CRC error: rx=0x%04X calc=0x%04X", crc_rx, crc_calc);
            }
        } else {
            LOG_W("ETX mismatch: expected 0x55, got 0x%02X", byte);
        }
        /* CRC 错或 ETX 错 → 复位 */
        parser->state = K230_STATE_WAIT_STX;
        parser->pos = 0;
        break;
    }

    return 0;
}

/* ========================= 发送帧 ========================= */
int k230_send_frame(uint8_t cmd, uint8_t seq,
                    const uint8_t *payload, uint8_t len)
{
    uint8_t  buf[K230_MAX_FRAME];
    uint16_t crc;

    if (len > K230_MAX_PAYLOAD) return -1;
    if (k230_serial == RT_NULL) return -1;

    buf[0] = K230_STX;
    buf[1] = len;
    buf[2] = cmd;
    buf[3] = seq;
    if (len > 0) {
        memcpy(&buf[4], payload, len);
    }

    crc = k230_crc16(&buf[2], 2 + len);
    buf[4 + len]     = (uint8_t)(crc & 0xFF);
    buf[5 + len]     = (uint8_t)(crc >> 8);
    buf[6 + len]     = K230_ETX;

    rt_device_write(k230_serial, 0, buf, 7 + len);

    LOG_D("TX frame: cmd=0x%02X seq=%u len=%u", cmd, seq, len);
    return 0;
}

/* ========================= 发送应答 ========================= */
int k230_send_ack(uint8_t cmd, uint8_t seq, uint8_t result)
{
    uint8_t pl = result;
    return k230_send_frame(cmd, seq, &pl, 1);
}

/* ========================= 命令分发 ========================= */
static void k230_cmd_dispatch(k230_frame_t *frame)
{
    uint8_t cmd = frame->cmd;
    uint8_t seq = frame->seq;

    LOG_I("CMD=0x%02X SEQ=%u LEN=%u", cmd, seq, frame->len);

    switch (cmd) {

    /* ──── 人脸解锁请求 (最高优先级) ──── */
    case K230_CMD_FACE_UNLOCK_REQ: {
        if (frame->len < 3) {
            k230_send_ack(K230_CMD_FACE_UNLOCK_ACK, seq, K230_RESULT_ERROR);
            break;
        }
        uint16_t user_id = ((uint16_t)frame->payload[0] << 8) | frame->payload[1];
        uint8_t  confidence = frame->payload[2];

        rt_kprintf("[K230] FACE UNLOCK: user=%u confidence=%u%%\n",
                   user_id, confidence);

        if (door_lock_open() == RT_EOK) {
            k230_send_ack(K230_CMD_FACE_UNLOCK_ACK, seq, K230_RESULT_OK);
            rt_kprintf("[K230] Door unlock command sent OK\n");
        } else {
            k230_send_ack(K230_CMD_FACE_UNLOCK_ACK, seq, K230_RESULT_BUSY);
            rt_kprintf("[K230] Door unlock FAILED: busy\n");
        }
        break;
    }

    /* ──── 门锁控制 ──── */
    case K230_CMD_DOOR_CTRL: {
        if (frame->len < 1) {
            k230_send_ack(K230_CMD_DOOR_CTRL, seq, K230_RESULT_ERROR);
            break;
        }
        if (frame->payload[0]) {
            door_lock_open();
            rt_kprintf("[K230] Door OPEN\n");
        } else {
            door_lock_close();
            rt_kprintf("[K230] Door CLOSE\n");
        }
        k230_send_ack(K230_CMD_DOOR_CTRL, seq, K230_RESULT_OK);
        break;
    }

    /* ──── 门状态查询 ──── */
    case K230_CMD_DOOR_STATUS_REQ: {
        door_state_t ds = door_lock_get_state();
        uint8_t st;
        switch (ds) {
        case DOOR_LOCKED:  st = K230_DOOR_LOCKED;  break;
        case DOOR_OPENING: st = K230_DOOR_OPENING; break;
        case DOOR_OPEN:    st = K230_DOOR_OPEN;    break;
        case DOOR_CLOSING: st = K230_DOOR_CLOSING; break;
        default:           st = K230_DOOR_ERROR;   break;
        }
        k230_send_frame(K230_CMD_DOOR_STATUS_ACK, seq, &st, 1);
        break;
    }

    /* ──── 心跳 ──── */
    case K230_CMD_HEARTBEAT_REQ: {
        uint32_t uptime = (uint32_t)(rt_tick_get() / RT_TICK_PER_SECOND);
        uint8_t pl[4];
        pl[0] = (uint8_t)(uptime >> 24);
        pl[1] = (uint8_t)(uptime >> 16);
        pl[2] = (uint8_t)(uptime >> 8);
        pl[3] = (uint8_t)(uptime);
        k230_send_frame(K230_CMD_HEARTBEAT_ACK, seq, pl, 4);
        break;
    }

    /* ──── LED 控制 ──── */
    case K230_CMD_LED_CTRL: {
        if (frame->len < 1) break;
        set_led_status(frame->payload[0] ? 1 : 0);
        rt_kprintf("[K230] LED %s\n", frame->payload[0] ? "ON" : "OFF");
        break;
    }

    /* ──── 报警控制 ──── */
    case K230_CMD_ALARM_CTRL: {
        if (frame->len < 1) break;
        if (frame->payload[0]) {
            alarm_start();
            rt_kprintf("[K230] Alarm ON\n");
        } else {
            alarm_stop();
            rt_kprintf("[K230] Alarm OFF\n");
        }
        break;
    }

    default:
        LOG_W("Unknown command: 0x%02X", cmd);
        rt_kprintf("[K230] Unknown CMD: 0x%02X\n", cmd);
        break;
    }
}

/* ========================= 串口接收线程 ========================= */
static void k230_uart_entry(void *parameter)
{
    uint8_t rx_buf[K230_BUF_SIZE];
    rt_size_t rx_len;
    int i, ret;
    k230_frame_t frame;

    k230_parser_init(&g_parser);
    LOG_I("K230 UART thread started (binary protocol)");

    while (1) {
        rx_len = rt_device_read(k230_serial, 0, rx_buf, K230_BUF_SIZE);

        if (rx_len > 0) {
            for (i = 0; i < (int)rx_len; i++) {
                ret = k230_parser_feed(&g_parser, rx_buf[i], &frame);
                if (ret == 1) {
                    /* 完整帧接收成功 */
                    k230_cmd_dispatch(&frame);
                }
                /* ret == 0: 继续等待 */
                /* ret == -1: CRC/格式错误, 已自动复位 */
            }
        } else if (rx_len == 0) {
            rt_thread_mdelay(10);
        } else {
            LOG_E("Serial read error: %d", rx_len);
            rt_thread_mdelay(100);
        }
    }
}

/* ========================= 启动串口线程 ========================= */
int k230_start(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    if (k230_serial != RT_NULL) {
        LOG_W("K230 UART already started");
        return 0;
    }

    k230_serial = rt_device_find(K230_UART_DEV);
    if (k230_serial == RT_NULL) {
        LOG_E("Cannot find %s", K230_UART_DEV);
        rt_kprintf("Error: %s not found\n", K230_UART_DEV);
        return -1;
    }

    config.baud_rate = K230_BAUD_RATE;
    config.bufsz      = K230_BUF_SIZE;
    if (rt_device_control(k230_serial, RT_DEVICE_CTRL_CONFIG, &config) != RT_EOK) {
        LOG_E("Failed to configure %s", K230_UART_DEV);
        return -1;
    }

    if (rt_device_open(k230_serial, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX) != RT_EOK) {
        LOG_E("Failed to open %s", K230_UART_DEV);
        k230_serial = RT_NULL;
        return -1;
    }

    k230_tid = rt_thread_create("k230_uart",
                                k230_uart_entry,
                                RT_NULL,
                                STACK_K230_UART,
                                PRIO_K230_UART,
                                TICK_K230_UART);

    if (k230_tid == RT_NULL) {
        LOG_E("Failed to create K230 UART thread");
        rt_device_close(k230_serial);
        k230_serial = RT_NULL;
        return -1;
    }

    rt_thread_startup(k230_tid);

    LOG_I("K230 UART ready (%s @ %d bps, PRIO=%d) [BINARY PROTOCOL]",
          K230_UART_DEV, K230_BAUD_RATE, PRIO_K230_UART);
    rt_kprintf("=== K230 UART: %s, %d bps [BINARY] ===\n", K230_UART_DEV, K230_BAUD_RATE);

    return 0;
}
MSH_CMD_EXPORT(k230_start, start K230 UART receiver thread);

static int k230_auto_init(void)
{
    return k230_start();
}
INIT_APP_EXPORT(k230_auto_init);

/* ========================= 停止串口线程 ========================= */
int k230_stop(void)
{
    if (k230_tid != RT_NULL) {
        rt_thread_delete(k230_tid);
        k230_tid = RT_NULL;
    }

    if (k230_serial != RT_NULL) {
        rt_device_close(k230_serial);
        k230_serial = RT_NULL;
    }

    LOG_I("K230 UART stopped");
    rt_kprintf("K230 UART stopped\n");

    return 0;
}
MSH_CMD_EXPORT(k230_stop, stop K230 UART receiver thread);

#endif /* FINSH_USING_MSH */