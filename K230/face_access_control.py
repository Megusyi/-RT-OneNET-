# Copyright (c) 2026, Megusyi, Lkkk8990
#
# SPDX-License-Identifier: Apache-2.0
#
from libs.PipeLine import PipeLine
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
from libs.Utils import *
import os, sys, gc, math, time
from media.media import *
import nncase_runtime as nn
import ulab.numpy as np
import image
import aidemo
import usocket, ussl
import network

# ============================================================================
#  SECTION 1: 全局配置
# ============================================================================

class Config:
    # ── 模型输入尺寸 ──
    DET_INPUT_SIZE = [320, 320]
    REG_INPUT_SIZE = [112, 112]
    LIVE_INPUT_SIZE = [112, 112]
    EMOTION_INPUT_SIZE = [224, 224]
    LANDMARK_INPUT_SIZE = [192, 192]

    # ── 图像分辨率 ──
    RGB888P_SIZE = [1280, 720]

    # ── 帧率优化参数 ──
    HEAVY_MODEL_INTERVAL = 3           # 重模型（识别/活体/表情/关键点）每N帧运行一次
    GC_INTERVAL = 60                   # 垃圾回收间隔（帧数）
    WIFI_CHECK_INTERVAL = 60           # WiFi状态检查间隔（帧数）

    # ── 模型文件路径 ──
    DET_KMODEL = "/sdcard/examples/kmodel/face_detection_320.kmodel"
    REG_KMODEL = "/sdcard/examples/kmodel/face_recognition.kmodel"
    LIVE_KMODEL = "/sdcard/examples/kmodel/face_liveness_rgb.kmodel"
    EMOTION_KMODEL = "/sdcard/examples/kmodel/face_emotion.kmodel"
    LANDMARK_KMODEL = "/sdcard/examples/kmodel/face_landmark.kmodel"
    ANCHORS_PATH = "/sdcard/examples/utils/prior_data_320.bin"
    DATABASE_DIR = "/sdcard/examples/utils/db/"

    # ── 检测阈值 ──
    CONFIDENCE_THRESHOLD = 0.35
    NMS_THRESHOLD = 0.45
    RECOGNITION_THRESHOLD = 0.78
    LIVENESS_THRESHOLD = 0.80
    EMOTION_CONFIDENCE_THRESHOLD = 0.5

    # ── 锚点参数 ──
    ANCHOR_LEN = 4200
    DET_DIM = 4
    FEATURE_NUM = 128
    MAX_REGISTER_FACE = 100

    # ── 仿射变换参数 ──
    UMEYAMA_ARGS = [38.2946, 51.6963, 73.5318, 51.5014, 56.0252, 71.7366, 41.5493, 92.3655, 70.7299, 92.2041]

    # ── 门禁行为参数 ──
    STABLE_DURATION = 3
    DOOR_OPEN_COOLDOWN = 5
    MAX_FACE_COUNT = 5
    QUALITY_THRESHOLD = 0.6
    BRIGHTNESS_MIN = 50
    BRIGHTNESS_MAX = 200

    # ── 显示模式 ──
    DISPLAY_MODE = "lcd"


# ============================================================================
#  SECTION 2: UART 通信协议
# ============================================================================

class UARTProtocol:
    STX = 0xAA
    ETX = 0x55
    MAX_PAYLOAD = 64
    MAX_FRAME = 7 + 64
    FRAME_TIMEOUT = 50

    CMD_FACE_UNLOCK_REQ = 0x01
    CMD_FACE_UNLOCK_ACK = 0x02
    CMD_DOOR_CTRL       = 0x03
    CMD_DOOR_STATUS_REQ = 0x04
    CMD_DOOR_STATUS_ACK = 0x05
    CMD_HEARTBEAT_REQ   = 0x06
    CMD_HEARTBEAT_ACK   = 0x07
    CMD_LED_CTRL        = 0x08
    CMD_ALARM_CTRL      = 0x09

    RESULT_OK    = 0x00
    RESULT_ERROR = 0x01
    RESULT_BUSY  = 0x02

    CMD_NAMES = {
        0x01: "FACE_UNLOCK_REQ", 0x02: "FACE_UNLOCK_ACK",
        0x03: "DOOR_CTRL", 0x04: "DOOR_STATUS_REQ",
        0x05: "DOOR_STATUS_ACK", 0x06: "HEARTBEAT_REQ",
        0x07: "HEARTBEAT_ACK", 0x08: "LED_CTRL",
        0x09: "ALARM_CTRL",
    }
    RES_NAMES = {0x00: "OK", 0x01: "ERROR", 0x02: "BUSY"}

    def __init__(self):
        self._seq = 0
        self._uart = None
        self._parser = self._K230Parser()

    def init(self, tx_pin=44, rx_pin=45, uart_id=None, baudrate=115200):
        from machine import UART, FPIOA
        if uart_id is None:
            uart_id = UART.UART2
        self._fpioa = FPIOA()
        self._fpioa.set_function(tx_pin, self._fpioa.UART2_TXD)
        self._fpioa.set_function(rx_pin, self._fpioa.UART2_RXD)
        self._uart = UART(uart_id, baudrate=baudrate, bits=UART.EIGHTBITS,
                          parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
        print("[UART] Initialized UART2 TX=%d RX=%d @%d bps" % (tx_pin, rx_pin, baudrate))

    def _crc16(self, data):
        crc = 0xFFFF
        for b in data:
            crc ^= b
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc = crc >> 1
        return crc

    def _hex(self, data):
        if not data:
            return ""
        return " ".join("%02X" % b for b in data)

    def send_frame(self, cmd, payload=b''):
        length = len(payload)
        if length > self.MAX_PAYLOAD:
            return -1
        seq = self._seq & 0xFF
        self._seq = (self._seq + 1) & 0xFF

        buf = bytearray(7 + length)
        buf[0] = self.STX
        buf[1] = length
        buf[2] = cmd
        buf[3] = seq
        if length > 0:
            buf[4:4 + length] = payload
        crc = self._crc16(buf[2:4 + length])
        buf[4 + length] = crc & 0xFF
        buf[5 + length] = (crc >> 8) & 0xFF
        buf[6 + length] = self.ETX

        self._uart.write(bytes(buf))
        name = self.CMD_NAMES.get(cmd, "CMD_0x%02X" % cmd)
        print("[TX] %s seq=%u len=%u payload[%s]" % (name, seq, length, self._hex(payload)))
        return seq

    def send_face_unlock_req(self, user_id, confidence):
        payload = bytes([(user_id >> 8) & 0xFF, user_id & 0xFF, confidence & 0xFF])
        return self.send_frame(self.CMD_FACE_UNLOCK_REQ, payload)

    def send_door_ctrl(self, action):
        payload = bytes([action & 0xFF])
        return self.send_frame(self.CMD_DOOR_CTRL, payload)

    def send_alarm(self, alarm_type):
        payload = bytes([alarm_type & 0xFF])
        return self.send_frame(self.CMD_ALARM_CTRL, payload)

    def wait_ack(self, expected_seq, timeout_ms=500):
        start = time.ticks_ms()
        while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
            if self._uart.any():
                data = self._uart.read(self._uart.any())
                if data:
                    for byte in data:
                        frame = self._parser.feed(byte)
                        if frame and frame['cmd'] == self.CMD_FACE_UNLOCK_ACK and frame['seq'] == expected_seq:
                            result = frame['payload'][0] if frame['payload'] else self.RESULT_ERROR
                            return result == self.RESULT_OK
            time.sleep_ms(10)
        print("[UART] ACK timeout for seq=%u" % expected_seq)
        self._parser.reset()
        return False

    def available(self):
        return self._uart is not None

    def poll(self):
        if not self._uart or not self._uart.any():
            return None
        data = self._uart.read(self._uart.any())
        if not data:
            return None
        frames = []
        for byte in data:
            frame = self._parser.feed(byte)
            if frame:
                frames.append(frame)
        return frames if frames else None

    class _K230Parser:
        STATE_WAIT_STX = 0
        STATE_WAIT_LEN = 1
        STATE_WAIT_DATA = 2
        STATE_WAIT_ETX = 3

        def __init__(self):
            self.state = self.STATE_WAIT_STX
            self.buf = bytearray(UARTProtocol.MAX_FRAME)
            self.pos = 0
            self.data_len = 0
            self.expected = 0
            self.last_byte_tick = time.ticks_ms()

        def reset(self):
            self.state = self.STATE_WAIT_STX
            self.pos = 0
            self.data_len = 0
            self.expected = 0

        def feed(self, byte):
            now = time.ticks_ms()
            if self.state != self.STATE_WAIT_STX:
                if time.ticks_diff(now, self.last_byte_tick) > UARTProtocol.FRAME_TIMEOUT:
                    self.reset()
            self.last_byte_tick = now

            if self.state == self.STATE_WAIT_STX:
                if byte == UARTProtocol.STX:
                    self.buf[0] = byte
                    self.pos = 1
                    self.state = self.STATE_WAIT_LEN

            elif self.state == self.STATE_WAIT_LEN:
                if byte > UARTProtocol.MAX_PAYLOAD:
                    self.reset()
                    return None
                self.buf[self.pos] = byte
                self.pos += 1
                self.data_len = byte
                self.expected = byte + 4
                self.state = self.STATE_WAIT_DATA

            elif self.state == self.STATE_WAIT_DATA:
                if self.pos < UARTProtocol.MAX_FRAME:
                    self.buf[self.pos] = byte
                    self.pos += 1
                else:
                    self.reset()
                    return None
                if self.pos >= 2 + self.expected:
                    self.state = self.STATE_WAIT_ETX

            elif self.state == self.STATE_WAIT_ETX:
                if byte == UARTProtocol.ETX:
                    self.buf[self.pos] = byte
                    self.pos += 1
                    p = self.buf
                    length = p[1]
                    crc_lo = p[4 + length]
                    crc_hi = p[5 + length]
                    crc_rx = (crc_hi << 8) | crc_lo
                    crc_calc = UARTProtocol._crc16_static(p[2:4 + length])
                    if crc_rx == crc_calc:
                        frame = {'cmd': p[2], 'seq': p[3], 'payload': bytes(p[4:4 + length])}
                        self.reset()
                        return frame
                self.reset()
            return None

    @staticmethod
    def _crc16_static(data):
        crc = 0xFFFF
        for b in data:
            crc ^= b
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc = crc >> 1
        return crc


# ============================================================================
#  SECTION 3: OneNET 云平台上传
# ============================================================================

class OneNETConfig:
    WIFI_SSID = "YOUR_WIFI_SSID"
    WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"
    PRODUCT_ID = "YOUR_PRODUCT_ID"
    DEVICE_NAME = "YOUR_DEVICE_NAME"
    AUTH = "YOUR_ONENET_AUTH_TOKEN"
    HOST = "iot-api.heclouds.com"
    UPLOAD_PATH = "/device/file-upload"
    UPLOAD_RETRY = 3
    CAPTURE_DIR = "/data/captures/"
    CAPTURE_SIZE = [640, 480]  # 截图缩放到此尺寸


class CloudUploader:
    def __init__(self):
        self._wlan = None
        self._connected = False
        self._status = u"WiFi: 未连接"

    def connect(self):
        return self._connect()

    def _connect(self):
        if self._connected:
            return True
        try:
            self._wlan = network.WLAN(network.STA_IF)
            if not self._wlan.active():
                self._wlan.active(True)
            self._status = u"WiFi: 连接中..."
            print("[Cloud] Connecting WiFi: %s ..." % OneNETConfig.WIFI_SSID)
            self._wlan.connect(OneNETConfig.WIFI_SSID, OneNETConfig.WIFI_PASSWORD)
            for _ in range(5):
                if self._wlan.isconnected():
                    break
                time.sleep(1)
                self._wlan.connect(OneNETConfig.WIFI_SSID, OneNETConfig.WIFI_PASSWORD)
            dhcp_timeout = 0
            while self._wlan.ifconfig()[0] == '0.0.0.0':
                time.sleep(0.5)
                dhcp_timeout += 1
                if dhcp_timeout > 30:
                    print("[Cloud] WiFi DHCP timeout (15s)!")
                    self._status = u"WiFi: DHCP超时"
                    return False
            if self._wlan.isconnected():
                self._connected = True
                ip = self._wlan.ifconfig()[0]
                self._status = u"WiFi: OK"
                print("[Cloud] WiFi OK! IP: %s" % ip)
                return True
            self._status = u"WiFi: 失败"
            print("[Cloud] WiFi failed!")
            return False
        except Exception as e:
            self._status = u"WiFi: 错误"
            print("[Cloud] WiFi error: %s" % e)
            sys.print_exception(e)
            return False

    def get_status(self):
        return self._status

    def upload(self, file_path):
        if not self._connect():
            return False

        filename = file_path.rsplit("/", 1)[-1]
        boundary = "----K230OneNET"

        try:
            with open(file_path, "rb") as f:
                file_data = f.read()
        except Exception as e:
            print("[Cloud] Read file failed: %s" % e)
            return False

        if filename.lower().endswith('.bmp'):
            content_type = 'image/bmp'
        elif filename.lower().endswith('.png'):
            content_type = 'image/png'
        else:
            content_type = 'image/jpeg'

        body = b""
        body += b"--" + boundary.encode() + b"\r\n"
        body += b'Content-Disposition: form-data; name="product_id"\r\n\r\n'
        body += OneNETConfig.PRODUCT_ID.encode() + b"\r\n"
        body += b"--" + boundary.encode() + b"\r\n"
        body += b'Content-Disposition: form-data; name="device_name"\r\n\r\n'
        body += OneNETConfig.DEVICE_NAME.encode() + b"\r\n"
        body += b"--" + boundary.encode() + b"\r\n"
        body += ('Content-Disposition: form-data; name="file"; filename="%s"\r\n' % filename).encode()
        body += ("Content-Type: %s\r\n\r\n" % content_type).encode()
        body += file_data + b"\r\n"
        body += b"--" + boundary.encode() + b"--\r\n"

        header = "POST %s HTTP/1.1\r\n" % OneNETConfig.UPLOAD_PATH
        header += "Host: %s\r\n" % OneNETConfig.HOST
        header += "authorization: %s\r\n" % OneNETConfig.AUTH
        header += "Content-Type: multipart/form-data; boundary=%s\r\n" % boundary
        header += "Content-Length: %d\r\n" % len(body)
        header += "Connection: close\r\n\r\n"

        request = header.encode() + body

        for attempt in range(OneNETConfig.UPLOAD_RETRY):
            try:
                print("[Cloud] Uploading (%d/%d)..." % (attempt + 1, OneNETConfig.UPLOAD_RETRY))
                addr = usocket.getaddrinfo(OneNETConfig.HOST, 443)[0][-1]
                sock = usocket.socket()
                sock.settimeout(10)
                sock.connect(addr)
                sock = ussl.wrap_socket(sock, server_hostname=OneNETConfig.HOST)
                sock.write(request)

                response = b""
                while True:
                    try:
                        data = sock.read(1024)
                        if not data:
                            break
                        response += data
                    except:
                        break
                sock.close()

                resp_str = response.decode('utf-8', 'ignore')
                parts = resp_str.split('\r\n\r\n', 1)
                body_text = parts[1] if len(parts) > 1 else resp_str

                if '"code":0' in body_text or '"code": 0' in body_text:
                    print("[Cloud] Upload success: %s" % filename)
                    return True
                print("[Cloud] Upload response: %s" % body_text[:100])
                time.sleep(0.5)
            except Exception as e:
                print("[Cloud] Upload error: %s" % e)
                try:
                    sock.close()
                except:
                    pass
                time.sleep(0.5)

        print("[Cloud] Upload failed after %d retries" % OneNETConfig.UPLOAD_RETRY)
        return False


# ============================================================================
#  SECTION 4: 数学工具函数
# ============================================================================

def svd22(a):
    s = [0.0, 0.0]
    u = [0.0, 0.0, 0.0, 0.0]
    v = [0.0, 0.0, 0.0, 0.0]
    s[0] = (math.sqrt((a[0] - a[3]) ** 2 + (a[1] + a[2]) ** 2) +
            math.sqrt((a[0] + a[3]) ** 2 + (a[1] - a[2]) ** 2)) / 2
    s[1] = abs(s[0] - math.sqrt((a[0] - a[3]) ** 2 + (a[1] + a[2]) ** 2))
    v[2] = math.sin((math.atan2(2 * (a[0] * a[1] + a[2] * a[3]),
                                a[0] ** 2 - a[1] ** 2 + a[2] ** 2 - a[3] ** 2)) / 2) if s[0] > s[1] else 0
    v[0] = math.sqrt(1 - v[2] ** 2)
    v[1] = -v[2]
    v[3] = v[0]
    u[0] = -(a[0] * v[0] + a[1] * v[2]) / s[0] if s[0] != 0 else 1
    u[2] = -(a[2] * v[0] + a[3] * v[2]) / s[0] if s[0] != 0 else 0
    u[1] = (a[0] * v[1] + a[1] * v[3]) / s[1] if s[1] != 0 else -u[2]
    u[3] = (a[2] * v[1] + a[3] * v[3]) / s[1] if s[1] != 0 else u[0]
    v[0], v[2] = -v[0], -v[2]
    return u, s, v


def umeyama_affine(src):
    src_mean = [sum(src[i] for i in range(0, 10, 2)) / 5,
                sum(src[i] for i in range(1, 10, 2)) / 5]
    dst_mean = [sum(Config.UMEYAMA_ARGS[i] for i in range(0, 10, 2)) / 5,
                sum(Config.UMEYAMA_ARGS[i] for i in range(1, 10, 2)) / 5]

    src_demean = [[src[2 * i] - src_mean[0], src[2 * i + 1] - src_mean[1]] for i in range(5)]
    dst_demean = [[Config.UMEYAMA_ARGS[2 * i] - dst_mean[0],
                   Config.UMEYAMA_ARGS[2 * i + 1] - dst_mean[1]] for i in range(5)]

    A = [[0.0, 0.0], [0.0, 0.0]]
    for i in range(2):
        for k in range(2):
            A[i][k] = sum(dst_demean[j][i] * src_demean[j][k] for j in range(5)) / 5

    T = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    U, S, V = svd22([A[0][0], A[0][1], A[1][0], A[1][1]])

    T[0][0], T[0][1] = U[0] * V[0] + U[1] * V[2], U[0] * V[1] + U[1] * V[3]
    T[1][0], T[1][1] = U[2] * V[0] + U[3] * V[2], U[2] * V[1] + U[3] * V[3]

    src_var = sum((src_demean[i][0] ** 2 + src_demean[i][1] ** 2) for i in range(5)) / 5
    scale = (S[0] + S[1]) / src_var if src_var != 0 else 1.0

    T[0][2] = dst_mean[0] - scale * (T[0][0] * src_mean[0] + T[0][1] * src_mean[1])
    T[1][2] = dst_mean[1] - scale * (T[1][0] * src_mean[0] + T[1][1] * src_mean[1])
    T[0][0], T[0][1], T[1][0], T[1][1] = [x * scale for x in [T[0][0], T[0][1], T[1][0], T[1][1]]]

    return [T[0][0], T[0][1], T[0][2], T[1][0], T[1][1], T[1][2]]


def estimate_face_quality(landms):
    if len(landms) < 10:
        return 0.0

    le_x, le_y = landms[0], landms[1]
    re_x, re_y = landms[2], landms[3]
    nose_x, nose_y = landms[4], landms[5]
    lm_x, lm_y = landms[6], landms[7]
    rm_x, rm_y = landms[8], landms[9]

    eye_dist = math.sqrt((re_x - le_x) ** 2 + (re_y - le_y) ** 2)
    mouth_dist = math.sqrt((rm_x - lm_x) ** 2 + (rm_y - lm_y) ** 2)

    eye_mouth_dist = math.sqrt(((le_x + re_x) / 2 - (lm_x + rm_x) / 2) ** 2 +
                               ((le_y + re_y) / 2 - (lm_y + rm_y) / 2) ** 2)

    symmetry_score = abs((le_x - nose_x) - (re_x - nose_x)) / max(eye_dist, 1)
    symmetry_score += abs((lm_x - nose_x) - (rm_x - nose_x)) / max(mouth_dist, 1)
    symmetry_score = max(0, 1 - symmetry_score / 2)

    ratio_score = min(eye_mouth_dist / max(eye_dist, 1), 2) / 2

    return (symmetry_score + ratio_score) / 2


def check_frame_quality(frame_np):
    if frame_np is None:
        return False, "No frame"
    if len(frame_np.shape) < 3:
        return False, "Invalid frame shape"

    brightness = np.mean(frame_np)
    if brightness < Config.BRIGHTNESS_MIN:
        return False, "Too dark"
    if brightness > Config.BRIGHTNESS_MAX:
        return False, "Too bright"

    gray = np.mean(frame_np, axis=2)
    contrast = np.std(gray)
    if contrast < 10:
        return False, "Low contrast"

    return True, "OK"


# ============================================================================
#  SECTION 5: AI推理模块
# ============================================================================

class FaceDetector(AIBase):
    def __init__(self, anchors, rgb888p_size, display_size, debug_mode=0):
        super().__init__(Config.DET_KMODEL, Config.DET_INPUT_SIZE, rgb888p_size, debug_mode)
        self._anchors = anchors
        self._confidence_thr = Config.CONFIDENCE_THRESHOLD
        self._nms_thr = Config.NMS_THRESHOLD
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self._debug = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("det preprocess", self._debug > 0):
            size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right, _ = letterbox_pad_param(self.rgb888p_size, Config.DET_INPUT_SIZE)
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, size[1], size[0]],
                             [1, 3, Config.DET_INPUT_SIZE[1], Config.DET_INPUT_SIZE[0]])

    def postprocess(self, results):
        with ScopedTiming("det postprocess", self._debug > 0):
            res = aidemo.face_det_post_process(
                self._confidence_thr, self._nms_thr, Config.DET_INPUT_SIZE[0],
                self._anchors, self.rgb888p_size, results)
            return (res, res) if len(res) == 0 else (res[0], res[1])


class FaceRecognizer(AIBase):
    def __init__(self, rgb888p_size, display_size, debug_mode=0):
        super().__init__(Config.REG_KMODEL, Config.REG_INPUT_SIZE, rgb888p_size, debug_mode)
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self._debug = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

    def config_preprocess(self, landm, input_image_size=None):
        with ScopedTiming("recog preprocess", self._debug > 0):
            size = input_image_size if input_image_size else self.rgb888p_size
            affine_matrix = umeyama_affine(landm)
            self.ai2d.affine(nn.interp_method.cv2_bilinear, 0, 0, 127, 1, affine_matrix)
            self.ai2d.build([1, 3, size[1], size[0]],
                             [1, 3, Config.REG_INPUT_SIZE[1], Config.REG_INPUT_SIZE[0]])

    def postprocess(self, results):
        return results[0][0]


class FaceLandMarkApp(AIBase):
    """
    106点人脸关键点模型推理
    从 face_landmark.py 移植，用于几何疲劳检测
    """
    def __init__(self, kmodel_path, model_input_size, rgb888p_size=[1920, 1080], display_size=[1920, 1080], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.matrix_dst = None
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

    def config_preprocess(self, det, input_image_size=None):
        with ScopedTiming("landmark preprocess", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            self.matrix_dst = self.get_affine_matrix(det)
            affine_matrix = [self.matrix_dst[0][0], self.matrix_dst[0][1], self.matrix_dst[0][2],
                             self.matrix_dst[1][0], self.matrix_dst[1][1], self.matrix_dst[1][2]]
            self.ai2d.affine(nn.interp_method.cv2_bilinear, 0, 0, 127, 1, affine_matrix)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                             [1, 3, self.model_input_size[1], self.model_input_size[0]])

    def postprocess(self, results):
        with ScopedTiming("landmark postprocess", self.debug_mode > 0):
            pred = results[0]
            half_input_len = self.model_input_size[0] // 2
            pred = pred.flatten()
            for i in range(len(pred)):
                pred[i] += (pred[i] + 1) * half_input_len
            matrix_dst_inv = aidemo.invert_affine_transform(self.matrix_dst)
            matrix_dst_inv = matrix_dst_inv.flatten()
            half_out_len = len(pred) // 2
            for kp_id in range(half_out_len):
                old_x = pred[kp_id * 2]
                old_y = pred[kp_id * 2 + 1]
                new_x = old_x * matrix_dst_inv[0] + old_y * matrix_dst_inv[1] + matrix_dst_inv[2]
                new_y = old_x * matrix_dst_inv[3] + old_y * matrix_dst_inv[4] + matrix_dst_inv[5]
                pred[kp_id * 2] = new_x
                pred[kp_id * 2 + 1] = new_y
            return pred

    def get_affine_matrix(self, bbox):
        with ScopedTiming("get_affine_matrix", self.debug_mode > 1):
            x1, y1, w, h = map(lambda x: int(round(x, 0)), bbox[:4])
            scale_ratio = (self.model_input_size[0]) / (max(w, h) * 1.5)
            cx = (x1 + w / 2) * scale_ratio
            cy = (y1 + h / 2) * scale_ratio
            half_input_len = self.model_input_size[0] / 2
            matrix_dst = np.zeros((2, 3), dtype=np.float)
            matrix_dst[0, 0] = scale_ratio
            matrix_dst[0, 1] = 0
            matrix_dst[0, 2] = half_input_len - cx
            matrix_dst[1, 0] = 0
            matrix_dst[1, 1] = scale_ratio
            matrix_dst[1, 2] = half_input_len - cy
            return matrix_dst


class LivenessDetector(AIBase):
    def __init__(self, rgb888p_size, display_size, debug_mode=0):
        super().__init__(Config.LIVE_KMODEL, Config.LIVE_INPUT_SIZE, rgb888p_size, debug_mode)
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self._debug = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

    def config_preprocess(self, box, input_image_size=None):
        with ScopedTiming("liveness preprocess", self._debug > 0):
            size = input_image_size if input_image_size else self.rgb888p_size
            expand = 0.3
            x = max(0, int(float(box[0] - box[2] * expand)))
            y = max(0, int(float(box[1] - box[3] * expand)))
            w = min(size[0] - x, int(float(box[2] * (1 + 2 * expand))))
            h = min(size[1] - y, int(float(box[3] * (1 + 2 * expand))))
            self.ai2d.crop(x, y, w, h)
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            top, bottom, left, right, _ = letterbox_pad_param([w, h], Config.LIVE_INPUT_SIZE)
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [128, 128, 128])
            self.ai2d.build([1, 3, size[1], size[0]],
                             [1, 3, Config.LIVE_INPUT_SIZE[1], Config.LIVE_INPUT_SIZE[0]])

    def postprocess(self, results):
        return results[0]


class EmotionAnalyzer(AIBase):
    EMOTION_LABELS = ["neutral", "happy", "sad", "angry", "surprise", "fear", "disgust"]

    VAD = {
        "neutral":  ( 0.0,  0.0,  0.0),
        "happy":    (+1.0, +0.7, +0.5),
        "sad":      (-1.0, -0.6, -0.5),
        "angry":    (-0.8, +0.7, +0.5),
        "surprise": (+0.2, +1.0,  0.0),
        "fear":     (-0.9, +0.8, -0.8),
        "disgust":  (-0.8, +0.3, -0.3),
    }

    FATIGUE_V_THRESH = -0.4
    FATIGUE_A_THRESH = +0.4
    FATIGUE_THRESHOLD = 0.15
    SEVERE_FATIGUE_THRESHOLD = 0.30
    CONFIDENCE_MIN = 0.3
    HISTORY_SIZE = 6
    CONSECUTIVE_REQUIRED = 2

    def __init__(self, rgb888p_size, display_size, debug_mode=0):
        super().__init__(Config.EMOTION_KMODEL, Config.EMOTION_INPUT_SIZE, rgb888p_size, debug_mode)
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self._debug = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
        self._fatigue_history = []
        self._consecutive_fatigue = 0

    def config_preprocess(self, face_box, input_image_size=None):
        with ScopedTiming("emotion preprocess", self._debug > 0):
            size = input_image_size if input_image_size else self.rgb888p_size
            x, y, w, h = face_box
            expand_ratio = 0.1
            x = max(0, int(float(x - w * expand_ratio)))
            y = max(0, int(float(y - h * expand_ratio)))
            w = min(size[0] - x, int(float(w * (1 + 2 * expand_ratio))))
            h = min(size[1] - y, int(float(h * (1 + 2 * expand_ratio))))

            self.ai2d.crop(x, y, w, h)
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            top, bottom, left, right, _ = letterbox_pad_param([w, h], Config.EMOTION_INPUT_SIZE)
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [128, 128, 128])
            self.ai2d.build([1, 3, size[1], size[0]],
                             [1, 3, Config.EMOTION_INPUT_SIZE[1], Config.EMOTION_INPUT_SIZE[0]])

    def postprocess(self, results):
        with ScopedTiming("emotion postprocess", self._debug > 0):
            try:
                output = results[0]
                exp_output = np.exp(output - np.max(output))
                probabilities = exp_output / np.sum(exp_output)
                probs = [float(probabilities[0, i]) for i in range(len(self.EMOTION_LABELS))]

                max_idx = np.argmax(probabilities)
                top_emotion = self.EMOTION_LABELS[max_idx]
                top_conf = probs[max_idx]

                if top_conf < self.CONFIDENCE_MIN:
                    self._consecutive_fatigue = max(0, self._consecutive_fatigue - 1)
                    return "normal", "uncertain", 0.0

                valence = 0.0
                arousal = 0.0
                dominance = 0.0
                for i, label in enumerate(self.EMOTION_LABELS):
                    v, a, d = self.VAD[label]
                    valence   += probs[i] * v
                    arousal   += probs[i] * a
                    dominance += probs[i] * d

                fatigue_score = 0.0
                if valence <= self.FATIGUE_V_THRESH and arousal <= self.FATIGUE_A_THRESH:
                    fatigue_score = 0.6 + 0.2 * max(0.0, -dominance)
                elif valence <= self.FATIGUE_V_THRESH or arousal <= self.FATIGUE_A_THRESH:
                    fatigue_score = 0.3 + 0.1 * max(0.0, -dominance)
                else:
                    fatigue_score = 0.0

                self._fatigue_history.append(fatigue_score)
                if len(self._fatigue_history) > self.HISTORY_SIZE:
                    self._fatigue_history.pop(0)

                smoothed = sum(self._fatigue_history) / len(self._fatigue_history)

                if smoothed >= self.FATIGUE_THRESHOLD:
                    self._consecutive_fatigue += 1
                else:
                    self._consecutive_fatigue = max(0, self._consecutive_fatigue - 1)

                if smoothed >= self.SEVERE_FATIGUE_THRESHOLD and self._consecutive_fatigue >= self.CONSECUTIVE_REQUIRED:
                    return "illness", top_emotion, smoothed
                elif smoothed >= self.FATIGUE_THRESHOLD and self._consecutive_fatigue >= self.CONSECUTIVE_REQUIRED:
                    return "fatigue", top_emotion, smoothed
                else:
                    return "normal", top_emotion, smoothed
            except Exception as e:
                print("[Emotion] Postprocess error: %s" % e)
                return "unknown", "unknown", 0.0


# ============================================================================
#  SECTION 5.5: 脸色分析（基于 RGB 传统 CV，无需额外模型）
# ============================================================================

class ComplexionAnalyzer:
    REDNESS_THRESHOLD = 1.15
    PALENESS_VAL_THRESHOLD = 200
    PALENESS_SAT_THRESHOLD = 40
    YELLOWNESS_THRESHOLD = 1.2

    @staticmethod
    def analyze(face_roi):
        if face_roi is None or face_roi.size == 0:
            return "normal", {"redness": 0, "brightness": 0, "yellowness": 0, "val": 0, "sat": 0}

        eps = 1e-6
        r = float(np.mean(face_roi[:, :, 0]))
        g = float(np.mean(face_roi[:, :, 1]))
        b = float(np.mean(face_roi[:, :, 2]))

        redness = r / (g + b + eps)
        brightness = (r + g + b) / 3
        yellowness = (r + g) / (2 * b + eps)

        max_c = max(r, g, b)
        min_c = min(r, g, b)
        delta = max_c - min_c
        val = max_c
        sat = delta / (max_c + eps) * 255

        features = {
            "redness": round(redness, 3),
            "brightness": round(brightness, 1),
            "yellowness": round(yellowness, 3),
            "val": round(val, 1),
            "sat": round(sat, 1),
        }

        if redness > ComplexionAnalyzer.REDNESS_THRESHOLD:
            return "flushed", features
        elif val > ComplexionAnalyzer.PALENESS_VAL_THRESHOLD and sat < ComplexionAnalyzer.PALENESS_SAT_THRESHOLD:
            return "pale", features
        elif yellowness > ComplexionAnalyzer.YELLOWNESS_THRESHOLD:
            return "yellow", features
        else:
            return "normal", features


# ============================================================================
#  SECTION 5.6: 几何疲劳分析（基于106点关键点，无需额外模型推理）
# ============================================================================

class FatigueAnalyzer:
    """
    专家级几何疲劳检测
    基于 106 点人脸关键点，独立几何计算，完全解耦情绪模型
    稳定抗干扰，精准区分疲劳/悲伤/愤怒
    """

    DICT_KP_SEQ = [
        [43, 44, 45, 47, 46, 50, 51, 49, 48],              # left_eyebrow
        [97, 98, 99, 100, 101, 105, 104, 103, 102],        # right_eyebrow
        [35, 36, 33, 37, 39, 42, 40, 41],                  # left_eye
        [89, 90, 87, 91, 93, 96, 94, 95],                  # right_eye
        [34, 88],                                          # pupil
        [72, 73, 74, 86],                                  # bridge_nose
        [77, 78, 79, 80, 85, 84, 83],                      # wing_nose
        [52, 55, 56, 53, 59, 58, 61, 68, 67, 71, 63, 64],  # out_lip
        [65, 54, 60, 57, 69, 70, 62, 66],                  # in_lip
    ]

    WEIGHT_EYE_DROOP   = 0.45
    WEIGHT_BROW_DROOP  = 0.25
    WEIGHT_MOUTH_DROOP = 0.15
    WEIGHT_FACE_RELAX  = 0.15

    PENALTY_HAPPY      = 0.40
    PENALTY_SURPRISE   = 0.35
    PENALTY_ANGRY      = 0.30
    PENALTY_SAD_TIGHT  = 0.20

    FATIGUE_THRESHOLD  = 0.25
    SEVERE_THRESHOLD   = 0.50

    MAX_YAW_ANGLE      = 25.0
    MIN_EAR_OPEN       = 0.08

    HISTORY_SIZE       = 6
    CONSECUTIVE_REQ    = 3

    def __init__(self):
        self._fatigue_history = []
        self._kp_history = []
        self._consecutive_count = 0

    def extract_points(self, landmarks, indices):
        points = []
        for idx in indices:
            if idx * 2 + 1 < len(landmarks):
                x = landmarks[idx * 2]
                y = landmarks[idx * 2 + 1]
                points.append((x, y))
        return points

    def calculate_ear(self, eye_points):
        if len(eye_points) < 6:
            return 0.3

        points = [(x, y) for x, y in eye_points]
        v1 = math.sqrt((points[1][0] - points[5][0])**2 + (points[1][1] - points[5][1])**2)
        v2 = math.sqrt((points[2][0] - points[4][0])**2 + (points[2][1] - points[4][1])**2)
        h  = math.sqrt((points[0][0] - points[3][0])**2 + (points[0][1] - points[3][1])**2)

        if h == 0:
            return 0.3

        ear = (v1 + v2) / (2.0 * h)
        return ear

    def get_eyelid_droop_coeff(self, landmarks):
        left_eye_idx  = self.DICT_KP_SEQ[2]
        right_eye_idx = self.DICT_KP_SEQ[3]

        left_points  = self.extract_points(landmarks, left_eye_idx)
        right_points = self.extract_points(landmarks, right_eye_idx)

        left_ear  = self.calculate_ear(left_points)
        right_ear = self.calculate_ear(right_points)

        ear_avg = (left_ear + right_ear) / 2.0

        if ear_avg < 0.12:
            return 1.0
        if ear_avg > 0.40:
            return 0.0

        coeff = (0.40 - ear_avg) / (0.40 - 0.12)
        return max(0.0, min(1.0, coeff))

    def get_brow_droop_coeff(self, landmarks):
        left_brow_idx  = self.DICT_KP_SEQ[0]
        right_brow_idx = self.DICT_KP_SEQ[1]

        left_brow  = self.extract_points(landmarks, left_brow_idx)
        right_brow = self.extract_points(landmarks, right_brow_idx)

        if len(left_brow) < 5 or len(right_brow) < 5:
            return 0.0

        left_head_y  = left_brow[0][1]
        left_tail_y  = left_brow[-1][1]
        right_head_y = right_brow[0][1]
        right_tail_y = right_brow[-1][1]

        left_drop  = max(0, left_head_y - left_tail_y)
        right_drop = max(0, right_head_y - right_tail_y)

        all_ys = [landmarks[i*2+1] for i in range(len(landmarks)//2)]
        face_height = max(all_ys) - min(all_ys)
        if face_height == 0:
            return 0.0

        norm_drop = (left_drop + right_drop) / (2.0 * face_height)

        coeff = norm_drop / 0.15
        return max(0.0, min(1.0, coeff))

    def get_mouth_droop_coeff(self, landmarks):
        out_lip_idx = self.DICT_KP_SEQ[7]
        out_lip = self.extract_points(landmarks, out_lip_idx)

        if len(out_lip) < 12:
            return 0.0

        wing_nose_idx = self.DICT_KP_SEQ[6]
        wing_points = self.extract_points(landmarks, wing_nose_idx)
        if len(wing_points) >= 5:
            nose_y = sum(y for _, y in wing_points) / len(wing_points)
        else:
            all_y = [y for _, y in out_lip]
            nose_y = min(all_y) - 10

        left_corner_y  = out_lip[0][1]
        right_corner_y = out_lip[6][1]

        all_ys = [landmarks[i*2+1] for i in range(len(landmarks)//2)]
        face_height = max(all_ys) - min(all_ys)
        if face_height == 0:
            return 0.0

        avg_drop = ((left_corner_y - nose_y) + (right_corner_y - nose_y)) / 2.0
        norm_drop = avg_drop / (face_height * 0.1)

        coeff = norm_drop / 1.0
        return max(0.0, min(1.0, coeff))

    def get_face_relax_coeff(self, landmarks):
        self._kp_history.append(list(landmarks))
        if len(self._kp_history) > self.HISTORY_SIZE:
            self._kp_history.pop(0)

        if len(self._kp_history) < 3:
            return 0.0

        n_points = len(landmarks) // 2
        variance_sum = 0.0

        for i in range(n_points):
            xs = [frame[i*2] for frame in self._kp_history]
            ys = [frame[i*2+1] for frame in self._kp_history]
            mean_x = sum(xs) / len(xs)
            mean_y = sum(ys) / len(ys)
            var_x = sum((x - mean_x)**2 for x in xs) / len(xs)
            var_y = sum((y - mean_y)**2 for y in ys) / len(ys)
            variance_sum += (var_x + var_y)

        xs_all = [landmarks[i*2] for i in range(n_points)]
        ys_all = [landmarks[i*2+1] for i in range(n_points)]
        face_area = (max(xs_all) - min(xs_all)) * (max(ys_all) - min(ys_all))
        if face_area == 0:
            return 0.0

        norm_var = variance_sum / (n_points * face_area)
        coeff = (0.05 - norm_var) / 0.05
        return max(0.0, min(1.0, coeff))

    def calculate_penalty(self, landmarks):
        penalty = 0.0

        penalty += self.PENALTY_HAPPY * self._detect_happy(landmarks)
        penalty += self.PENALTY_SURPRISE * self._detect_surprise(landmarks)
        penalty += self.PENALTY_ANGRY * self._detect_angry(landmarks)
        penalty += self.PENALTY_SAD_TIGHT * self._detect_sad_tight(landmarks)

        return min(1.0, penalty)

    def _detect_happy(self, landmarks):
        out_lip_idx = self.DICT_KP_SEQ[7]
        out_lip = self.extract_points(landmarks, out_lip_idx)
        if len(out_lip) < 12:
            return 0.0

        left_corner_y  = out_lip[0][1]
        right_corner_y = out_lip[6][1]
        mid_upper_y    = out_lip[3][1]

        corner_above_mid = (left_corner_y < mid_upper_y) + (right_corner_y < mid_upper_y)
        return corner_above_mid / 2.0

    def _detect_surprise(self, landmarks):
        left_eye_idx  = self.DICT_KP_SEQ[2]
        right_eye_idx = self.DICT_KP_SEQ[3]

        left_points  = self.extract_points(landmarks, left_eye_idx)
        right_points = self.extract_points(landmarks, right_eye_idx)

        left_ear  = self.calculate_ear(left_points)
        right_ear = self.calculate_ear(right_points)
        ear_avg = (left_ear + right_ear) / 2.0

        mouth_open = self._is_mouth_open(landmarks)

        if ear_avg > 0.40 and mouth_open:
            return 1.0
        if ear_avg > 0.35 or mouth_open:
            return 0.5
        return 0.0

    def _detect_angry(self, landmarks):
        left_brow_idx  = self.DICT_KP_SEQ[0]
        right_brow_idx = self.DICT_KP_SEQ[1]

        left_brow  = self.extract_points(landmarks, left_brow_idx)
        right_brow = self.extract_points(landmarks, right_brow_idx)

        if len(left_brow) < 5 or len(right_brow) < 5:
            return 0.0

        left_brow_xs = [x for x, y in left_brow]
        right_brow_xs = [x for x, y in right_brow]
        left_width = max(left_brow_xs) - min(left_brow_xs)
        right_width = max(right_brow_xs) - min(right_brow_xs)

        xs_all = [landmarks[i*2] for i in range(len(landmarks)//2)]
        face_width = max(xs_all) - min(xs_all)
        if face_width == 0:
            return 0.0

        avg_width = (left_width + right_width) / 2.0
        norm_width = avg_width / (face_width * 0.12)

        if norm_width < 0.6:
            return 1.0
        if norm_width < 0.8:
            return 0.5
        return 0.0

    def _detect_sad_tight(self, landmarks):
        wing_nose_idx = self.DICT_KP_SEQ[6]
        wing_nose = self.extract_points(landmarks, wing_nose_idx)

        if len(wing_nose) < 5:
            return 0.0

        nose_width = max(x for x, y in wing_nose) - min(x for x, y in wing_nose)
        xs_all = [landmarks[i*2] for i in range(len(landmarks)//2)]
        face_width = max(xs_all) - min(xs_all)
        if face_width == 0:
            return 0.0

        norm_nose = nose_width / face_width

        if norm_nose > 0.15:
            return 1.0
        if norm_nose > 0.12:
            return 0.5
        return 0.0

    def _is_mouth_open(self, landmarks):
        if len(self.DICT_KP_SEQ) > 8:
            in_lip_idx = self.DICT_KP_SEQ[8]
            in_points = self.extract_points(landmarks, in_lip_idx)
            if len(in_points) >= 4:
                ys = [y for x, y in in_points]
                h = max(ys) - min(ys)
                all_ys = [landmarks[i*2+1] for i in range(len(landmarks)//2)]
                face_h = max(all_ys) - min(all_ys)
                if face_h > 0:
                    return h / face_h > 0.15

        out_lip_idx = self.DICT_KP_SEQ[7]
        out_points = self.extract_points(landmarks, out_lip_idx)
        if len(out_points) >= 4:
            ys = [y for x, y in out_points]
            h = max(ys) - min(ys)
            all_ys = [landmarks[i*2+1] for i in range(len(landmarks)//2)]
            face_h = max(all_ys) - min(all_ys)
            if face_h > 0:
                return h / face_h > 0.12

        return False

    def _check_min_eye_open(self, landmarks):
        left_eye_idx  = self.DICT_KP_SEQ[2]
        right_eye_idx = self.DICT_KP_SEQ[3]

        left_points  = self.extract_points(landmarks, left_eye_idx)
        right_points = self.extract_points(landmarks, right_eye_idx)

        left_ear  = self.calculate_ear(left_points)
        right_ear = self.calculate_ear(right_points)
        ear_avg = (left_ear + right_ear) / 2.0

        return ear_avg >= self.MIN_EAR_OPEN

    def analyze(self, landmarks):
        if not landmarks or len(landmarks) < 106 * 2:
            return "normal", 0.0, {}

        if not self._check_min_eye_open(landmarks):
            return "uncertain", 0.0, {"reason": "eyes_closed"}

        eye_droop   = self.get_eyelid_droop_coeff(landmarks)
        brow_droop  = self.get_brow_droop_coeff(landmarks)
        mouth_droop = self.get_mouth_droop_coeff(landmarks)
        face_relax  = self.get_face_relax_coeff(landmarks)

        base = (self.WEIGHT_EYE_DROOP   * eye_droop +
                self.WEIGHT_BROW_DROOP  * brow_droop +
                self.WEIGHT_MOUTH_DROOP * mouth_droop +
                self.WEIGHT_FACE_RELAX  * face_relax)

        penalty = self.calculate_penalty(landmarks)
        score = max(0.0, base - penalty)

        self._fatigue_history.append(score)
        if len(self._fatigue_history) > self.HISTORY_SIZE:
            self._fatigue_history.pop(0)
        smoothed = sum(self._fatigue_history) / len(self._fatigue_history)

        if smoothed >= self.FATIGUE_THRESHOLD:
            self._consecutive_count += 1
        else:
            self._consecutive_count = max(0, self._consecutive_count - 1)

        details = {
            "eye_droop": eye_droop,
            "brow_droop": brow_droop,
            "mouth_droop": mouth_droop,
            "face_relax": face_relax,
            "base": base,
            "penalty": penalty,
            "raw_score": score,
            "smoothed": smoothed,
            "consecutive": self._consecutive_count
        }

        if self._consecutive_count >= self.CONSECUTIVE_REQ:
            if smoothed >= self.SEVERE_THRESHOLD:
                return "illness", smoothed, details
            elif smoothed >= self.FATIGUE_THRESHOLD:
                return "fatigue", smoothed, details

        return "normal", smoothed, details


# ============================================================================
#  SECTION 6: 人脸数据库管理
# ============================================================================

class FaceDatabase:
    def __init__(self):
        self._names = []
        self._features = []
        self._count = 0

    def load(self):
        try:
            self._names = []
            self._features = []
            self._count = 0
            for f in os.listdir(Config.DATABASE_DIR):
                if not f.endswith('.bin') or self._count >= Config.MAX_REGISTER_FACE:
                    continue
                with open(Config.DATABASE_DIR + f, 'rb') as fp:
                    self._features.append(np.frombuffer(fp.read(), dtype=np.float))
                    name = f.split('.')[0]
                    if '_' in name:
                        suffix = name.rsplit('_', 1)[-1]
                        if suffix.isdigit():
                            name = name.rsplit('_', 1)[0]
                    self._names.append(name)
                    self._count += 1
            print("[DB] Loaded %d faces from %s" % (self._count, Config.DATABASE_DIR))
        except Exception as e:
            print("[DB] Load failed: %s" % e)

    def search(self, feature):
        if self._count == 0:
            return "unknown", 0.0, -1

        feature_norm = feature / np.linalg.norm(feature)
        max_score, max_idx = 0.0, -1

        for i in range(self._count):
            db_feature = self._features[i] / np.linalg.norm(self._features[i])
            score = np.dot(feature_norm, db_feature) / 2 + 0.5
            if score > max_score:
                max_score, max_idx = score, i

        if max_idx < 0 or max_score < Config.RECOGNITION_THRESHOLD:
            return "unknown", 0.0, -1

        return self._names[max_idx], max_score, max_idx

    @property
    def count(self):
        return self._count

    @property
    def names(self):
        return self._names


# ============================================================================
#  SECTION 7: 门禁控制器主类
# ============================================================================

class AccessController:
    STATUS_IDLE = "idle"
    STATUS_DETECTING = "detecting"
    STATUS_STABILIZING = "stabilizing"
    STATUS_UNLOCKING = "unlocking"

    def __init__(self, anchors, rgb888p_size, display_size,
                 enable_uart=True, enable_cloud=True, debug_mode=0):
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self._debug = debug_mode
        self._anchors = anchors

        self._status = self.STATUS_IDLE
        self._last_unlock_time = 0
        self._need_leave = False
        self._last_recognized_user = None
        self._last_frame = None
        self._capture_needed = False
        self._pending_uploads = []
        self._last_capture_time = 0
        self._idle_frame_count = 0
        self._consecutive_success = 0
        self._consecutive_fail = 0
        self._required_consecutive = 10
        self._max_tolerance_fail = 3
        self._feature_history = []
        self._feature_history_max = 5

        self._frame_count = 0
        self._last_heavy_frame = -1
        self._cached_det_boxes = []
        self._cached_landms = []
        self._cached_liveness = []
        self._cached_recognition = []
        self._cached_emotion = []
        self._cached_wifi_status = u"WiFi: 关闭"
        self._wifi_check_counter = 0

        self._detector = FaceDetector(anchors, rgb888p_size, display_size, debug_mode)
        self._detector.config_preprocess()

        self._recognizer = None
        self._liveness = None
        self._emotion = None
        self._landmark = None
        self._fatigue_analyzer = FatigueAnalyzer()
        self._models_loaded = False

        self._db = FaceDatabase()
        self._db.load()

        self._uart = UARTProtocol() if enable_uart else None
        self._cloud = CloudUploader() if enable_cloud else None
        if self._cloud:
            print("[Controller] Connecting WiFi at startup...")
            self._cloud.connect()
            self._wifi_status = self._cloud.get_status()
            print("[Controller] WiFi status: %s" % self._wifi_status)
        else:
            self._wifi_status = u"WiFi: 关闭"
        self._cached_wifi_status = self._wifi_status

        if self._uart:
            self._uart.init()

        print("[Controller] Initialized. Status: %s" % self._status)

    def _lazy_load_models(self):
        if self._models_loaded:
            return True
        try:
            gc.collect()
            print("[Controller] Loading recognition model...")
            self._recognizer = FaceRecognizer(self.rgb888p_size, self.display_size, self._debug)
            gc.collect()
            print("[Controller] Loading liveness model...")
            self._liveness = LivenessDetector(self.rgb888p_size, self.display_size, self._debug)
            gc.collect()
            print("[Controller] Loading emotion model...")
            self._emotion = EmotionAnalyzer(self.rgb888p_size, self.display_size, self._debug)
            gc.collect()
            print("[Controller] Loading landmark model...")
            self._landmark = FaceLandMarkApp(Config.LANDMARK_KMODEL, Config.LANDMARK_INPUT_SIZE,
                                              self.rgb888p_size, self.display_size, self._debug)
            gc.collect()
            self._models_loaded = True
            print("[Controller] All models loaded (5 models)")
            return True
        except Exception as e:
            print("[Controller] Model load failed: %s" % e)
            sys.print_exception(e)
            return False

    def process(self, input_np):
        self._frame_count += 1

        try:
            det_boxes, landms = self._detector.run(input_np)
        except Exception as e:
            print("[Detector] Error: %s" % e)
            return [], [], [], [], self._status, "Detector error"

        if not det_boxes or len(det_boxes) == 0:
            self._status = self.STATUS_IDLE
            self._need_leave = False
            self._feature_history = []
            self._cached_det_boxes = []
            self._cached_landms = []
            self._cached_liveness = []
            self._cached_recognition = []
            self._cached_emotion = []
            if self._pending_uploads:
                self._idle_frame_count += 1
                if self._idle_frame_count >= 30:
                    elapsed = time.time() - self._last_capture_time if self._last_capture_time > 0 else 999
                    if elapsed >= 3:
                        self._process_uploads()
                        self._idle_frame_count = 0
            return [], [], [], [], self.STATUS_IDLE, "No face"

        if len(det_boxes) > Config.MAX_FACE_COUNT:
            return det_boxes, ["人多"] * len(det_boxes), ["跳过"] * len(det_boxes), \
                [("unknown", "unknown", 0.0)] * len(det_boxes), self._status, "Too many faces"

        self._idle_frame_count = 0

        if not self._lazy_load_models():
            return det_boxes, ["加载中"] * len(det_boxes), ["等待"] * len(det_boxes), \
                [("unknown", "unknown", 0.0)] * len(det_boxes), self._status, "Loading models"

        run_heavy = (self._frame_count - self._last_heavy_frame) >= Config.HEAVY_MODEL_INTERVAL

        if run_heavy:
            liveness_results = []
            recognition_results = []
            emotion_results = []
            found_authorized = False
            best_user = None

            for i, box in enumerate(det_boxes):
                try:
                    self._liveness.config_preprocess(box)
                    live_res = self._liveness.run(input_np)
                    live_score = float(live_res[0][1])
                except Exception as e:
                    print("[Liveness] Error: %s" % e)
                    liveness_results.append("错误")
                    recognition_results.append("跳过")
                    emotion_results.append(("unknown", "unknown", 0.0))
                    continue

                if live_score > Config.LIVENESS_THRESHOLD:
                    liveness_results.append("真人")

                    if i < len(landms):
                        try:
                            self._recognizer.config_preprocess(landms[i])
                            feature = self._recognizer.run(input_np)
                            self._feature_history.append(feature)
                            if len(self._feature_history) > self._feature_history_max:
                                self._feature_history.pop(0)
                            n = len(self._feature_history)
                            avg_feature = self._feature_history[0]
                            for j in range(1, n):
                                avg_feature = avg_feature + self._feature_history[j]
                            avg_feature = avg_feature / n
                            name, score, idx = self._db.search(avg_feature)
                        except Exception as e:
                            print("[Recognizer] Error: %s" % e)
                            recognition_results.append("识别错误")
                            emotion_results.append(("unknown", "unknown", 0.0))
                            continue

                        if name != "unknown":
                            recognition_results.append("%s(%.2f)" % (name, score))
                            found_authorized = True
                            best_user = {"name": name, "id": idx, "score": score}
                        else:
                            recognition_results.append("陌生人")

                        try:
                            self._emotion.config_preprocess(box)
                            emotion_result = self._emotion.run(input_np)
                            state, emotion, conf = emotion_result

                            if i < len(landms):
                                self._landmark.config_preprocess(box)
                                landmarks_106 = self._landmark.run(input_np)
                                geo_state, geo_score, geo_details = self._fatigue_analyzer.analyze(landmarks_106)
                                if geo_state == "uncertain":
                                    geo_state = "normal"
                            else:
                                geo_state, geo_score, geo_details = "normal", 0.0, {}

                            any_fatigue = (state == "fatigue" or geo_state in ("fatigue", "illness"))
                            if any_fatigue:
                                x, y, w, h = int(box[0]), int(box[1]), int(box[2]), int(box[3])
                                cx, cy = x + w // 2, y + h // 2
                                iw, ih = int(w * 0.5), int(h * 0.4)
                                sx = max(0, cx - iw // 2)
                                sy = max(0, cy - ih // 2)
                                ex = min(input_np.shape[1], sx + iw)
                                ey = min(input_np.shape[0], sy + ih)
                                if ex > sx and ey > sy:
                                    face_roi = input_np[sy:sy + (ey - sy), sx:sx + (ex - sx)]
                                    complexion, _ = ComplexionAnalyzer.analyze(face_roi)
                                    if complexion in ("flushed", "pale"):
                                        state = "illness"
                                        print("[Health] fatigue + %s -> illness" % complexion)
                                    elif geo_state in ("fatigue", "illness"):
                                        state = "fatigue"
                                    else:
                                        state = "fatigue"
                                elif geo_state in ("fatigue", "illness"):
                                    state = "fatigue"

                            emotion_result = (state, emotion, conf)
                            emotion_results.append(emotion_result)
                        except Exception as e:
                            print("[Emotion] Error: %s" % e)
                            emotion_results.append(("unknown", "unknown", 0.0))
                    else:
                        recognition_results.append("无特征")
                        emotion_results.append(("unknown", "unknown", 0.0))
                else:
                    liveness_results.append("非真人")
                    recognition_results.append("驳回")
                    emotion_results.append(("unknown", "unknown", 0.0))

            self._last_heavy_frame = self._frame_count
            self._cached_det_boxes = det_boxes
            self._cached_landms = landms
            self._cached_liveness = liveness_results
            self._cached_recognition = recognition_results
            self._cached_emotion = emotion_results

            self._update_status(found_authorized, best_user)
        else:
            det_boxes = self._cached_det_boxes
            liveness_results = self._cached_liveness
            recognition_results = self._cached_recognition
            emotion_results = self._cached_emotion

        return det_boxes, liveness_results, recognition_results, emotion_results, self._status, "OK"

    def _update_status(self, found_authorized, best_user):
        now = time.time()

        if self._need_leave:
            if now - self._last_unlock_time > Config.DOOR_OPEN_COOLDOWN:
                self._need_leave = False
            return

        if found_authorized and best_user:
            self._consecutive_fail = 0
            self._consecutive_success += 1
            self._last_recognized_user = best_user
            self._status = self.STATUS_STABILIZING

            if self._consecutive_success >= self._required_consecutive:
                self._do_unlock(best_user)
                self._consecutive_success = 0
                self._consecutive_fail = 0
        else:
            self._consecutive_fail += 1
            if self._consecutive_fail >= self._max_tolerance_fail:
                if self._consecutive_success > 0:
                    print("[Stability] Lost after %d successes, %d fails" % (
                        self._consecutive_success, self._consecutive_fail))
                self._consecutive_success = 0
                self._consecutive_fail = 0
                self._need_leave = False
                self._status = self.STATUS_IDLE

    def _do_unlock(self, user):
        print("[Unlock] Authorized: %s (score=%.2f)" % (user["name"], user["score"]))
        self._status = self.STATUS_UNLOCKING

        self._capture_needed = True

        if self._uart:
            seq = self._uart.send_face_unlock_req(user["id"], int(user["score"] * 100))
            if self._uart.wait_ack(seq, timeout_ms=500):
                print("[Unlock] Master ACK received, unlock success")
            else:
                print("[Unlock] Master ACK timeout, door may not open")

        self._need_leave = True
        self._last_unlock_time = time.time()
        self._status = self.STATUS_IDLE
        self._consecutive_success = 0
        self._consecutive_fail = 0
        self._feature_history = []
        print("[Unlock] Open door done")

    def _capture_screen(self, pl):
        if not self._capture_needed:
            return
        self._capture_needed = False

        if self._last_frame is None:
            print("[Capture] No frame")
            return

        try:
            try:
                os.mkdir(OneNETConfig.CAPTURE_DIR)
            except:
                pass

            t = time.localtime()
            timestamp = "%04d%02d%02d_%02d%02d%02d" % (
                t[0], t[1], t[2], t[3], t[4], t[5])
            filename = "%scapture_%s.bmp" % (OneNETConfig.CAPTURE_DIR, timestamp)

            cam_w, cam_h = self.rgb888p_size[0], self.rgb888p_size[1]
            cap_w, cap_h = OneNETConfig.CAPTURE_SIZE[0], OneNETConfig.CAPTURE_SIZE[1]
            pixel_count = cap_w * cap_h

            try:
                cam_data = np.ascontiguousarray(self._last_frame).tobytes()
            except:
                cam_data = self._last_frame.tobytes()

            plane_size = cam_w * cam_h
            bgra = bytearray(pixel_count * 4)

            for dy in range(cap_h):
                src_y = dy * cam_h // cap_h
                row_base = dy * cap_w * 4
                cam_row_base = src_y * cam_w
                for dx in range(cap_w):
                    src_x = dx * cam_w // cap_w
                    src_idx = cam_row_base + src_x
                    di = row_base + dx * 4
                    bgra[di]     = cam_data[2 * plane_size + src_idx]
                    bgra[di + 1] = cam_data[1 * plane_size + src_idx]
                    bgra[di + 2] = cam_data[0 * plane_size + src_idx]
                    bgra[di + 3] = 255

            try:
                osd_data = pl.osd_img.bytearray()
                osd_w = pl.osd_img.width()
                osd_h = pl.osd_img.height()
                for dy in range(cap_h):
                    osd_src_y = dy * osd_h // cap_h
                    if osd_src_y >= osd_h:
                        break
                    src_row = osd_src_y * osd_w * 4
                    dst_row = dy * cap_w * 4
                    for dx in range(cap_w):
                        osd_src_x = dx * osd_w // cap_w
                        if osd_src_x >= osd_w:
                            break
                        sp = src_row + osd_src_x * 4
                        a = osd_data[sp]
                        if a > 0:
                            dp = dst_row + dx * 4
                            if a >= 250:
                                bgra[dp]     = osd_data[sp + 3]
                                bgra[dp + 1] = osd_data[sp + 2]
                                bgra[dp + 2] = osd_data[sp + 1]
                            else:
                                inv_a = 255 - a
                                bgra[dp]     = (osd_data[sp + 3] * a + bgra[dp]     * inv_a) >> 8
                                bgra[dp + 1] = (osd_data[sp + 2] * a + bgra[dp + 1] * inv_a) >> 8
                                bgra[dp + 2] = (osd_data[sp + 1] * a + bgra[dp + 2] * inv_a) >> 8
                            bgra[dp + 3] = 255
            except Exception as osd_err:
                print("[Capture] OSD blend skipped:", osd_err)

            pixel_data_size = pixel_count * 4
            file_size = 54 + pixel_data_size
            bmp_header = bytearray([
                0x42, 0x4D,
                (file_size & 0xFF), ((file_size >> 8) & 0xFF), ((file_size >> 16) & 0xFF), ((file_size >> 24) & 0xFF),
                0, 0, 0, 0,
                54, 0, 0, 0,
                40, 0, 0, 0,
                (cap_w & 0xFF), ((cap_w >> 8) & 0xFF), ((cap_w >> 16) & 0xFF), ((cap_w >> 24) & 0xFF),
                (cap_h & 0xFF), ((cap_h >> 8) & 0xFF), ((cap_h >> 16) & 0xFF), ((cap_h >> 24) & 0xFF),
                1, 0,
                32, 0,
                0, 0, 0, 0,
                (pixel_data_size & 0xFF), ((pixel_data_size >> 8) & 0xFF), ((pixel_data_size >> 16) & 0xFF), ((pixel_data_size >> 24) & 0xFF),
                0, 0, 0, 0,
                0, 0, 0, 0,
                0, 0, 0, 0,
                0, 0, 0, 0
            ])

            with open(filename, 'wb') as f:
                f.write(bmp_header)
                for y in range(cap_h - 1, -1, -1):
                    start = y * cap_w * 4
                    f.write(bgra[start:start + cap_w * 4])
                f.flush()

            print("[Capture] Saved: %s (%dx%d)" % (filename, cap_w, cap_h))

            if self._cloud:
                self._pending_uploads.append(filename)
                self._last_capture_time = time.time()
                print("[Capture] Queued upload: %s" % filename)
        except Exception as e:
            print("[Capture] Failed: %s" % e)

    def _process_uploads(self):
        if not self._pending_uploads:
            return
        file_path = self._pending_uploads[0]
        try:
            if self._cloud.upload(file_path):
                self._pending_uploads.pop(0)
                try:
                    os.remove(file_path)
                    print("[Upload] Removed local file: %s" % file_path)
                except Exception as e:
                    print("[Upload] Failed to remove local file: %s" % e)
            else:
                print("[Upload] Failed, will retry later: %s" % file_path)
        except Exception as e:
            print("[Upload] Error: %s" % e)

    def draw_result(self, pl, det_boxes, liveness_results, recognition_results, emotion_results, status):
        pl.osd_img.clear()

        self._wifi_check_counter += 1
        if self._cloud and self._wifi_check_counter >= Config.WIFI_CHECK_INTERVAL:
            self._wifi_status = self._cloud.get_status()
            self._cached_wifi_status = self._wifi_status
            self._wifi_check_counter = 0
        wifi_status = self._cached_wifi_status
        y = 10
        if "OK" in wifi_status:
            wifi_color = (0, 255, 0)
        elif "连接" in wifi_status:
            wifi_color = (255, 165, 0)
        elif "失败" in wifi_status or "错误" in wifi_status:
            wifi_color = (255, 0, 0)
        else:
            wifi_color = (128, 128, 128)
        pl.osd_img.draw_string_advanced(10, y, 24, wifi_status, color=wifi_color)

        if not det_boxes:
            return

        for i, det in enumerate(det_boxes):
            if not isinstance(det, (list, tuple, np.ndarray)) or len(det) < 4:
                continue

            x, y, w, h = map(lambda v: int(float(v)), det[:4])
            x = x * self.display_size[0] // self.rgb888p_size[0]
            y = y * self.display_size[1] // self.rgb888p_size[1]
            w = w * self.display_size[0] // self.rgb888p_size[0]
            h = h * self.display_size[1] // self.rgb888p_size[1]

            live_label = liveness_results[i] if i < len(liveness_results) else "?"
            recg_label = recognition_results[i] if i < len(recognition_results) else "?"
            emo_state, emo_label, emo_conf = ("unknown", "unknown", 0.0)
            if i < len(emotion_results):
                emo_state, emo_label, emo_conf = emotion_results[i]

            if "陌生人" not in recg_label and recg_label not in ("?", "识别错误"):
                box_color = (0, 255, 0)
            else:
                box_color = (255, 255, 0)

            pl.osd_img.draw_rectangle(x, y, w, h, color=box_color, thickness=3)

            offset = y - 60
            pl.osd_img.draw_string_advanced(x, offset, 28,
                                            "ID:%s" % recg_label, color=(255, 255, 255))
            offset += 28
            if emo_state == "illness":
                emo_color = (255, 0, 0)
            elif emo_state == "fatigue":
                emo_color = (255, 165, 0)
            elif emo_state == "normal":
                emo_color = (0, 255, 0)
            else:
                emo_color = (128, 128, 128)
            emo_cn = {"illness": u"病态", "fatigue": u"疲劳", "normal": u"正常", "unknown": u"未知"}
            pl.osd_img.draw_string_advanced(x, offset, 28,
                                            "Status:%s" % emo_cn.get(emo_state, emo_state),
                                            color=emo_color)
            offset += 28
            if live_label == "真人":
                live_color = (0, 255, 0)
            elif live_label == "非真人":
                live_color = (255, 0, 0)
            else:
                live_color = (255, 255, 0)
            pl.osd_img.draw_string_advanced(x, offset, 28,
                                            "Live:%s" % live_label, color=live_color)

        status_labels = {
            AccessController.STATUS_IDLE: ("待机", (128, 128, 128)),
            AccessController.STATUS_DETECTING: ("检测中", (255, 255, 0)),
            AccessController.STATUS_STABILIZING: ("稳定中...", (255, 165, 0)),
            AccessController.STATUS_UNLOCKING: ("已开门", (0, 255, 0)),
        }
        label, color = status_labels.get(status, (status, (255, 255, 255)))
        pl.osd_img.draw_string_advanced(10, 40, 32, "Status: %s" % label, color=color)

        if self._consecutive_success > 0 and status == self.STATUS_STABILIZING:
            remaining = self._required_consecutive - self._consecutive_success
            pl.osd_img.draw_string_advanced(10, 80, 28,
                                            "Hold: %d/%d" % (self._consecutive_success, self._required_consecutive),
                                            color=(255, 165, 0))

    def deinit(self):
        self._detector.deinit()
        if self._recognizer:
            self._recognizer.deinit()
        if self._emotion:
            self._emotion.deinit()
        if self._landmark:
            self._landmark.deinit()
        print("[Controller] Deinitialized")


# ============================================================================
#  SECTION 8: 主入口
# ============================================================================

def validate_model_paths():
    paths = [
        ("检测模型", Config.DET_KMODEL),
        ("识别模型", Config.REG_KMODEL),
        ("表情模型", Config.EMOTION_KMODEL),
        ("关键点模型", Config.LANDMARK_KMODEL),
        ("锚点文件", Config.ANCHORS_PATH),
        ("人脸数据库", Config.DATABASE_DIR),
    ]
    all_ok = True
    for name, path in paths:
        try:
            os.stat(path)
            print("[Check] %s: OK (%s)" % (name, path))
        except OSError:
            print("[Check] %s: MISSING! (%s)" % (name, path))
            all_ok = False
    return all_ok


def main():
    print("=" * 55)
    print("  K230 Face Access Control System")
    print("  Features: Detection + Recognition + Emotion + Geometric Fatigue")
    print("=" * 55)

    if not validate_model_paths():
        print("[System] FATAL: Model files missing, check paths in Config!")

    anchors = np.fromfile(Config.ANCHORS_PATH, dtype=np.float)
    anchors = anchors.reshape((Config.ANCHOR_LEN, Config.DET_DIM))

    pl = PipeLine(rgb888p_size=Config.RGB888P_SIZE, display_mode=Config.DISPLAY_MODE)
    pl.create()
    display_size = pl.get_display_size()

    print("[System] Resolution: %dx%d" % (Config.RGB888P_SIZE[0], Config.RGB888P_SIZE[1]))
    print("[System] Display: %dx%d" % (display_size[0], display_size[1]))

    controller = AccessController(
        anchors=anchors,
        rgb888p_size=Config.RGB888P_SIZE,
        display_size=display_size,
        enable_uart=True,
        enable_cloud=True,
        debug_mode=0
    )

    print("[System] Running... Press Ctrl+C to stop")
    frame_count = 0
    try:
        while True:
            img = pl.get_frame()
            if img is None:
                time.sleep_ms(10)
                continue

            det_boxes, live_res, recg_res, emo_res, status, msg = controller.process(img)
            controller.draw_result(pl, det_boxes, live_res, recg_res, emo_res, status)
            pl.show_image()

            if controller._uart:
                frames = controller._uart.poll()
                if frames:
                    for frame in frames:
                        name = UARTProtocol.CMD_NAMES.get(frame['cmd'], "CMD_0x%02X" % frame['cmd'])
                        payload_hex = " ".join("%02X" % b for b in frame['payload'])
                        print("[RX] %s seq=%u payload=[%s]" % (name, frame['seq'], payload_hex))

            if controller._capture_needed:
                controller._last_frame = img.copy()
                controller._capture_screen(pl)

            frame_count += 1
            if frame_count % 30 == 0:
                print("[Frame] %d frames processed, status=%s" % (frame_count, status))

            if frame_count % Config.GC_INTERVAL == 0:
                gc.collect()
    except KeyboardInterrupt:
        print("\n[System] Stopped by user")
    except Exception as e:
        print("[System] Error: %s" % e)
        sys.print_exception(e)
    finally:
        controller.deinit()
        pl.destroy()
        print("[System] Cleanup complete")


if __name__ == "__main__":
    main()