# 基于RT操作系统的视觉多特征检测与OneNET云端管控的厨房智能门禁系统

基于 **Flutter + STM32F407 + K230** 的物联网智能家居项目，通过 **中国移动 OneNET 云平台** 实现手机远程监控、AI 人脸识别门禁、环境数据采集与声光报警。

## 功能特性

### 手机APP（Flutter）

| 页面 | 功能 |
|------|------|
| **首页** | 实时温度、湿度、烟雾浓度；门禁与灯光状态；实时时钟 |
| **历史记录** | 门禁操作日志，含时间戳与状态 |
| **远程控制** | MQTT 连接；LED 开关；按住开门 / 消防紧急解锁 |
| **相册** | 查看 K230 上传至云端的抓拍图片 |

### AI人脸识别门禁（K230）

- 5个AI模型：人脸检测、人脸识别（128维特征）、RGB活体检测、7种表情识别、106点关键点检测
- 活体检测防照片/视频攻击
- 基于关键点几何 + VAD情绪模型的疲劳/病态分析
- 传统CV面色分析（红润/苍白/蜡黄）
- 连续15帧稳定识别后才开门，防误触发
- 开门自动抓拍并上传云端

### 嵌入式控制模块（STM32F407 + RT-Thread）

- MQ-2烟雾传感器：ADC采样、PPM浓度计算，>1000PPM自动报警
- 继电器控制电磁锁，支持按键 / AI / 远程三种开门方式，3秒自动关门
- PWM蜂鸣器（3kHz）+ LED声光报警，支持气体超标或远程触发
- ESP8266 WiFi每2秒上传传感器数据到OneNET
- UART自定义协议（CRC16，115200波特率）与K230通信

## 硬件清单

| 组件 | 型号 |
|------|------|
| 主控芯片 | STM32F407VGT6 |
| AI 视觉芯片 | 嘉楠K230 |
| 烟雾传感器 | MQ-2 |
| 温湿度传感器 | DHT11 |
| WiFi 模块 | ESP8266 |
| 蜂鸣器 | 无源蜂鸣器（3kHz PWM） |
| 摄像头 | K230配套摄像头模组 |

## 快速开始

### 环境要求

- Flutter SDK >= 3.0
- RT-Thread开发环境（MDK / GCC）
- K230 Python开发环境 + nncase SDK

### 1. 配置 OneNET 平台

在 [OneNET 物联网平台](https://open.iot.10086.cn) 创建产品和设备，然后将凭证信息更新到 `lib/config/app_config.dart`。

### 2. 运行Flutter APP

```bash
cd lib/
flutter pub get
flutter run
```

### 3. 部署STM32固件

用RT-Thread Studio打开`documents/stm32/`，编译并烧录至STM32F407。

### 4. 部署K230 AI模块

将`documents/face_access_control.py`和模型文件拷贝至K230，运行：

```bash
python face_access_control.py
```

## 项目结构

```
APPalter_2/
├── lib/                          # Flutter APP源码
│   ├── main.dart                 # 入口，4 个Tab导航
│   ├── config/app_config.dart    # OneNET配置
│   ├── models/                   # 数据模型
│   ├── pages/                    # 页面
│   ├── providers/                # 状态管理（Provider）
│   ├── services/                 # MQTT与HTTP API服务
│   └── styles/                   # 全局样式
├── documents/
│   ├── face_access_control.py    # K230人脸识别系统
│   └── stm32/applications/       # STM32固件源码
│       ├── main.c                # 系统入口
│       ├── app_def.h             # 引脚映射与协议定义
│       ├── Mq2.c                 # MQ-2烟雾传感器驱动
│       ├── door_lock.c/h         # 门锁控制
│       ├── k230_uart.c           # K230 UART协议
│       ├── onenet_sample.c       # 数据上传
│       ├── onenet_recv.c         # 命令接收
│       └── docker.c              # 声光报警
└── docs/
```

## 技术栈

| 类别 | 技术 |
|------|------|
| 移动端 | Flutter, Dart, Provider |
| 云平台 | 中国移动OneNET（MQTT + HTTP） |
| 嵌入式 | STM32F407, RT-Thread, ESP8266 |
| AI | 嘉楠 K230, nncase, Python |
| 通信协议 | MQTT v3.1.1, REST, UART（CRC16） |

## 许可证

Apache License 2.0