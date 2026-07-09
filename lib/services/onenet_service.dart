// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/services/onenet_service.dart
import 'dart:convert';
import 'package:http/http.dart' as http;
import '../config/app_config.dart';

class OneNetService {
  final String _baseUrl = AppConfig.baseUrl;

  // 查询设备物模型属性（一次性获取所有属性）
  Future<Map<String, dynamic>> queryDeviceProperties() async {
    final uri = Uri.parse('$_baseUrl/thingmodel/query-device-property')
        .replace(
      queryParameters: {
        'product_id': AppConfig.productId,
        'device_name': AppConfig.deviceName,
      },
    );

    final response = await http.get(
      uri,
      headers: {
        'Accept': 'application/json, text/plain, */*',
        'authorization': AppConfig.userAuthorization,
      },
    );

    if (response.statusCode != 200) {
      throw Exception('HTTP ${response.statusCode}: ${response.body}');
    }

    final data = json.decode(response.body);
    if (data['code'] != 0) {
      throw Exception('code=${data['code']}: ${data['msg']}');
    }

    final properties = data['data'] as List;
    final Map<String, dynamic> result = {};
    for (final prop in properties) {
      result[prop['identifier'] as String] = prop;
    }
    return result;
  }

  // 获取账户文件列表（用户级鉴权），page/size 分页控制
  Future<List<Map<String, dynamic>>> getImageList({int page = 1, int size = 100}) async {
    final uri = Uri.parse('$_baseUrl/device/file-list').replace(
      queryParameters: {
        'page': page.toString(),
        'size': size.toString(),
      },
    );

    final response = await http.get(
      uri,
      headers: {
        'Accept': 'application/json, text/plain, */*',
        'authorization': AppConfig.userAuthorization,
      },
    );

    if (response.statusCode != 200) {
      throw Exception('HTTP ${response.statusCode}\n${response.body}');
    }

    final data = json.decode(response.body);
    if (data['code'] != 0) {
      throw Exception('API错误 code=${data['code']}: ${data['msg']}');
    }

    final fileData = data['data'];
    if (fileData == null || fileData['list'] == null) return [];
    return List<Map<String, dynamic>>.from(fileData['list']);
  }

  // 设置设备属性（HTTP API 下发，平台会自动 MQTT 推送给设备）
  Future<bool> setDeviceProperty(String identifier, dynamic value) async {
    final uri = Uri.parse('$_baseUrl/thingmodel/set-device-property');

    final response = await http.post(
      uri,
      headers: {
        'Content-Type': 'application/json',
        'authorization': AppConfig.userAuthorization,
      },
      body: json.encode({
        'product_id': AppConfig.productId,
        'device_name': AppConfig.deviceName,
        'params': {
          identifier: value,
        },
      }),
    );

    if (response.statusCode != 200) {
      throw Exception('HTTP ${response.statusCode}\n${response.body}');
    }

    final data = json.decode(response.body);
    if (data['code'] != 0) {
      throw Exception('API错误 code=${data['code']}: ${data['msg']}');
    }

    return true;
  }

  // 获取文件下载URL（直接拼接，不调API）
  String getImageDownloadUrl(String fid) {
    return '$_baseUrl/device/file-download?fid=$fid';
  }
}