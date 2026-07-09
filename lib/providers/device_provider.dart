// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/providers/device_provider.dart
import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/device_data.dart';
import '../models/access_log.dart';
import '../services/onenet_service.dart';

class DeviceProvider extends ChangeNotifier {
  final OneNetService _onenetService = OneNetService();

  DeviceData? _currentData;
  List<AccessLog> _accessLogs = [];
  bool? _lightStatus;
  bool _lockStatus = true;  // 门锁默认上电锁定，无需等待设备上报
  bool _isLoading = false;
  // 脉冲解锁相关
  bool _isUnlocking = false;  // 按住模式
  Timer? _unlockTimer;      // 按住模式用的 Timer
  DateTime? _unlockStartTime;  // 记录开始时间
  bool _isAutoUnlocking = false;  // 持续解锁模式（消防）
  Timer? _autoUnlockTimer;         // 持续解锁模式用的 Timer
  DateTime? _autoUnlockStartTime;  // 持续解锁开始时间

  // Getters
  DeviceData? get currentData => _currentData;
  List<AccessLog> get accessLogs => _accessLogs;
  bool? get lightStatus => _lightStatus;
  bool get lockStatus => _lockStatus;  // 默认为 true（锁定）
  bool get isLoading => _isLoading;
  bool get isUnlocking => _isUnlocking;
  bool get isAutoUnlocking => _isAutoUnlocking;

  // 加载设备物模型属性
  Future<void> loadDeviceProperties() async {
    print('🔄 开始加载设备属性...');
    _isLoading = true;
    notifyListeners();

    try {
      final properties = await _onenetService.queryDeviceProperties();
      if (properties != null) {
        print('✅ 属性解析: 共 ${properties.length} 个');

        // 解析温度 (DHT)
        if (properties.containsKey('DHT')) {
          final dht = properties['DHT'];
          final temp = double.tryParse(dht['value']?.toString() ?? '') ?? 0.0;
          print('   🌡 温度(DHT): $temp°C');

          // 解析湿度 (humidity)
          final humidity = properties.containsKey('humidity')
              ? double.tryParse(
                      properties['humidity']['value']?.toString() ?? '') ??
                  0.0
              : 0.0;
          print('   💧 湿度(humidity): $humidity%');

          _currentData = DeviceData(
            temperature: temp,
            humidity: humidity,
            timestamp: DateTime.now(),
          );
        } else {
          print('⚠️ 未找到 DHT 属性');
        }

        // 解析光照灯状态 (led)
        if (properties.containsKey('led')) {
          final ledValue = properties['led']['value']?.toString() ?? '';
          _lightStatus = ledValue == 'true' || ledValue == '1';
          print('   💡 光照灯(led): $_lightStatus');
        }

        // 解析门锁状态 (lock) —— 云平台 lock=true→单片机收到 false→物理锁定；lock=false→单片机收到 true→物理解锁
        if (properties.containsKey('lock')) {
          final lockValue = properties['lock']['value']?.toString() ?? '';
          final platformLocked = lockValue == 'true' || lockValue == '1';
          _lockStatus = platformLocked;  // 直接对应：平台 true=已锁定，平台 false=已解锁
          print('   🔒 门锁(lock): platform=$lockValue → platformLocked=$platformLocked → lockStatus=$_lockStatus');
        }
      } else {
        print('⚠️ queryDeviceProperties 返回 null');
      }
    } catch (e) {
      print('❌ 加载设备属性失败: $e');
    }

    _isLoading = false;
    notifyListeners();
    print('🔄 设备属性加载完成, isLoading=$_isLoading, data=$_currentData');
  }

  // 添加本地操作记录
  void _addAccessLog({required String type, required String detail}) {
    final now = DateTime.now();
    final timeStr =
        '${now.year}-${_pad(now.month)}-${_pad(now.day)} '
        '${_pad(now.hour)}:${_pad(now.minute)}:${_pad(now.second)}';

    _accessLogs.insert(0, AccessLog(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      time: timeStr,
      type: type,
      operator: detail,
      status: '成功',
    ));
  }

  String _pad(int n) => n.toString().padLeft(2, '0');

  // 秒数格式化：<60秒→"持续X秒"，<3600秒→"持续X分钟"，≥3600秒→"持续X小时Y分"
  String _formatSeconds(int seconds) {
    if (seconds < 60) {
      return '持续${seconds}秒';
    } else if (seconds < 3600) {
      return '持续${seconds ~/ 60}分钟';
    } else {
      final hours = seconds ~/ 3600;
      final mins = (seconds % 3600) ~/ 60;
      return mins > 0 ? '持续${hours}小时${mins}分' : '持续${hours}小时';
    }
  }

  // 设置光照状态（HTTP API 下发，平台自动 MQTT 推送给设备）
  Future<bool> setLightStatus(bool status) async {
    final success = await _onenetService.setDeviceProperty('led', status);

    if (success) {
      _lightStatus = status;
      _addAccessLog(
        type: 'light',
        detail: status ? '光照灯已解锁' : '光照灯已关闭',
      );
      notifyListeners();
    }

    return success;
  }

  // 设置门锁状态（HTTP API 下发）
  // 注意：设备 lock 属性值与用户理解相反，发给设备时需要取反
  Future<bool> setLockStatus(bool status) async {
    // status=true 锁定 → 发送 lock=true → 单片机收到 false → 物理锁定
    // status=false 解锁 → 发送 lock=false → 单片机收到 true → 物理解锁
    final deviceValue = status;  // 直接对应，不再取反
    final success = await _onenetService.setDeviceProperty('lock', deviceValue);

    if (success) {
      _lockStatus = status;
      _addAccessLog(
        type: 'lock',
        detail: status ? '门已关闭' : '门已解锁',
      );
      notifyListeners();
    }

    return success;
  }

  // 脉冲解锁：按住时周期性发送解锁信号给设备
  // 设备收到 lock=true 即解锁，松手后停止发送，设备3s后自动关门
  Future<void> startUnlockPulse() async {
    if (_isUnlocking) return;
    _isUnlocking = true;
    _unlockStartTime = DateTime.now();
    _lockStatus = false;  // 标记为正在解锁
    notifyListeners();
    print('🔓 开始脉冲解锁');

    // 立刻发送一次，如果失败则立即停止
    final firstOk = await _sendUnlockOnce();
    if (!firstOk) {
      _unlockTimer?.cancel();
      _unlockTimer = null;
      _isUnlocking = false;
      _lockStatus = true;
      _unlockStartTime = null;
      notifyListeners();
      print('❌ 第一次解锁信号发送失败，停止脉冲解锁');
      return;
    }
    _unlockTimer = Timer.periodic(
      const Duration(milliseconds: 1500),
      (_) async {
        final ok = await _sendUnlockOnce();
        if (!ok) {
          _unlockTimer?.cancel();
          _unlockTimer = null;
          _isUnlocking = false;
          _lockStatus = true;
          _unlockStartTime = null;
          notifyListeners();
          print('❌ 持续发送失败，停止脉冲解锁');
        }
      },
    );
  }

  // 停止脉冲解锁（松手时调用）
  Future<void> stopUnlockPulse() async {
    if (!_isUnlocking) return;
    _unlockTimer?.cancel();
    _unlockTimer = null;

    // 计算开门持续时间（秒）
    final seconds = _unlockStartTime != null
        ? DateTime.now().difference(_unlockStartTime!).inSeconds
        : 0;
    _unlockStartTime = null;

    _isUnlocking = false;
    _lockStatus = true;  // 松手后标记为锁定状态（设备3s后自动关门）
    notifyListeners();

    // 发送 lock=true → 云平台 true → 单片机收到 false → 物理锁定（门关闭）
    try {
      await _onenetService.setDeviceProperty('lock', true);
      print('🔒 停止脉冲解锁，已同步平台状态 lock=true（锁定）');
    } catch (e) {
      print('❌ 同步锁定状态到平台失败: $e');
    }

    // 写一条清晰的操作日志
    _addAccessLog(
      type: 'lock',
      detail: '门已解锁（${_formatSeconds(seconds)}）',
    );

    notifyListeners();
    print('🔒 停止脉冲解锁，持续 ${seconds}秒');
  }

  // 发送单次解锁信号（内部调用，不记录单次日志）
  // 返回 true 表示发送成功，false 表示失败（如设备不在线）
  Future<bool> _sendUnlockOnce() async {
    try {
      // 发送 lock=false → 云平台 false → 单片机收到 true → 物理解锁（门打开）
      final ok = await _onenetService.setDeviceProperty('lock', false);
      return ok;
    } catch (e) {
      print('❌ 脉冲解锁发送失败: $e');
      return false;
    }
  }

  // 持续解锁模式（消防解锁）：点击一次后自动持续发送解锁信号
  // 再点击一次停止
  Future<void> toggleAutoUnlock() async {
    if (_isAutoUnlocking) {
      // 停止持续解锁
      _autoUnlockTimer?.cancel();
      _autoUnlockTimer = null;
      _isAutoUnlocking = false;
      _lockStatus = true;  // 门恢复锁定
      notifyListeners();
      // 发送 lock=true → 云平台 true → 单片机收到 false → 物理锁定（门关闭）
      try {
        await _onenetService.setDeviceProperty('lock', true);
        print('🔒 停止消防解锁，已同步平台状态 lock=true（锁定）');
      } catch (e) {
        print('❌ 同步锁定状态到平台失败: $e');
      }
      // 计算持续时间并写日志
      final seconds = _autoUnlockStartTime != null
          ? DateTime.now().difference(_autoUnlockStartTime!).inSeconds
          : 0;
      _autoUnlockStartTime = null;
      _addAccessLog(
        type: 'lock',
        detail: '停止消防解锁（${_formatSeconds(seconds)}）',
      );
    } else {
      // 开始持续解锁
      // 如果正在按住模式，先停止按住模式
      if (_isUnlocking) {
        _unlockTimer?.cancel();
        _unlockTimer = null;
        _isUnlocking = false;
      }
      _isAutoUnlocking = true;
      _autoUnlockStartTime = DateTime.now();
      _lockStatus = false;  // 标记为解锁状态
      notifyListeners();
      print('🚨 开始持续解锁（消防模式）');
      // 立刻发送一次，如果失败则立即停止
      final firstOk = await _sendUnlockOnce();
      if (!firstOk) {
        _autoUnlockTimer?.cancel();
        _autoUnlockTimer = null;
        _isAutoUnlocking = false;
        _lockStatus = true;
        _autoUnlockStartTime = null;
        notifyListeners();
        print('❌ 第一次消防解锁信号发送失败，停止持续解锁');
        return;
      }
      _autoUnlockTimer = Timer.periodic(
        const Duration(milliseconds: 1500),
        (_) async {
          final ok = await _sendUnlockOnce();
          if (!ok) {
            _autoUnlockTimer?.cancel();
            _autoUnlockTimer = null;
            _isAutoUnlocking = false;
            _lockStatus = true;
            _autoUnlockStartTime = null;
            notifyListeners();
            print('❌ 持续发送失败，停止消防解锁');
          }
        },
      );
      _addAccessLog(
        type: 'lock',
        detail: '消防解锁',
      );
    }
  }

  // 加载门禁日志（直接展示本地积累的记录，无需 API 调用）
  Future<void> loadAccessLogs() async {
    _isLoading = true;
    notifyListeners();
    await Future.delayed(const Duration(milliseconds: 200));
    _isLoading = false;
    notifyListeners();
  }
}