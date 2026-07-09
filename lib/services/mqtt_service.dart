// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/services/mqtt_service.dart
import 'dart:convert';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import '../config/app_config.dart';

class MqttService {
  MqttServerClient? _client;
  bool _connected = false;

  bool get connected => _connected;

  // 连接MQTT
  Future<bool> connect() async {
    if (_connected) {
      print('⚠️ MQTT 已连接，跳过');
      return true;
    }

    // 断开旧连接
    if (_client != null) {
      try {
        _client!.disconnect();
      } catch (_) {}
      _client = null;
      _connected = false;
    }

    try {
      // 客户端ID = deviceName = "ceshi"
      _client = MqttServerClient(
        'mqtts.heclouds.com',
        AppConfig.deviceName,
      );

      _client!.port = 1883; // ✅ 端口
      _client!.keepAlivePeriod = 60; // ✅ 心跳间隔
      _client!.setProtocolV311(); // ✅ 协议版本
      _client!.autoReconnect = true; // 自动重连

      // ✅ 认证信息（与Python完全一致）
      // username = productId, password = accessToken
      final connMessage = MqttConnectMessage()
          .startClean()
          .authenticateAs(
            AppConfig.productId, // 用户名
            AppConfig.accessToken, // 密码
          );

      _client!.connectionMessage = connMessage;

      print('🔌 正在连接MQTT...');
      print('   服务器: ${_client!.server}:${_client!.port}');
      print('   客户端ID: ${AppConfig.deviceName}');
      print('   用户名: ${AppConfig.productId}');

      await _client!.connect();

      if (_client!.connectionStatus?.state == MqttConnectionState.connected) {
        print('✅ MQTT连接成功!');
        _connected = true;
        _subscribeTopics(); // 订阅主题
        return true;
      } else {
        print('❌ MQTT连接失败');
        _connected = false;
        return false;
      }
    } catch (e) {
      print('❌ 连接异常: $e');
      _connected = false;
      return false;
    }
  }

  // 订阅主题
  void _subscribeTopics() {
    if (_client == null || !_connected) return;

    // ✅ 订阅回复主题
    final replyTopic =
        '\$sys/${AppConfig.productId}/${AppConfig.deviceName}/thing/property/post/reply';
    _client!.subscribe(replyTopic, MqttQos.atLeastOnce);
    print('📡 已订阅: $replyTopic');

    // ✅ 订阅设置主题
    final setTopic =
        '\$sys/${AppConfig.productId}/${AppConfig.deviceName}/thing/property/set';
    _client!.subscribe(setTopic, MqttQos.atLeastOnce);
    print('📡 已订阅: $setTopic');

    // 消息监听
    _client!.updates!.listen((List<MqttReceivedMessage<MqttMessage>> c) {
      final message = c[0];
      final topic = message.topic;
      final payload = message.payload as MqttPublishMessage;
      final content = MqttPublishPayload.bytesToStringAsString(
        payload.payload.message,
      );

      print('📨 收到消息 [$topic]: $content');

      // 解析消息
      try {
        final data = json.decode(content);

        if (topic.contains('property/set')) {
          // 处理平台下发指令
          print('🔽 收到平台下发指令');
          final params = data['params'] as Map?;
          if (params != null) {
            params.forEach((key, value) {
              if (value is Map && value.containsKey('value')) {
                print('   $key: ${value['value']}');
              } else {
                print('   $key: $value');
              }
            });
          }
        } else {
          // 处理平台响应
          if (data['code'] == 200) {
            print('✅ 数据上传成功!');
          }
        }
      } catch (e) {
        print('❌ 解析消息失败: $e');
      }
    });
  }

  // 发布属性（同时发 post 更新云端 + set 触发设备硬件）
  Future<bool> publishProperty(String key, dynamic value) async {
    if (_client == null || !_connected) {
      print('❌ 未连接MQTT');
      return false;
    }

    try {
      final payload = json.encode({
        "id": "${DateTime.now().millisecondsSinceEpoch}",
        "version": "1.0",
        "params": {
          key: {"value": value},
        },
      });

      final builder = MqttClientPayloadBuilder();
      builder.addString(payload);

      // ① post 主题 → 更新云端值
      final postTopic =
          '\$sys/${AppConfig.productId}/${AppConfig.deviceName}/thing/property/post';
      _client!.publishMessage(postTopic, MqttQos.atLeastOnce, builder.payload!);
      print('📤 [post] $key = $value');

      // ② set 主题 → 触发设备硬件（K230 onenet_cmd_handler）
      final setTopic =
          '\$sys/${AppConfig.productId}/${AppConfig.deviceName}/thing/property/set';
      _client!.publishMessage(setTopic, MqttQos.atLeastOnce, builder.payload!);
      print('📤 [set] $key = $value');

      return true;
    } catch (e) {
      print('❌ 发送失败: $e');
      return false;
    }
  }

  // 断开连接
  void disconnect() {
    if (_client != null) {
      _client!.disconnect();
      _connected = false;
      print('🔌 已断开MQTT');
    }
  }
}