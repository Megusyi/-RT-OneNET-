// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';
import '../services/onenet_service.dart';
import '../main.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final OneNetService _service = OneNetService();
  Map<String, dynamic>? _properties;
  bool _loading = true;
  String? _error;
  DateTime? _lastUpdate;
  DateTime _now = DateTime.now();
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    _loadData();
    _timer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (mounted) setState(() => _now = DateTime.now());
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  Future<void> _loadData() async {
    if (!_loading) return;
    try {
      final data = await _service.queryDeviceProperties();
      if (!mounted) return;
      setState(() {
        _properties = data;
        _loading = false;
        _error = null;
        _lastUpdate = DateTime.now();
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = e.toString();
        _loading = false;
      });
    }
  }

  String _getValue(String key) =>
      _properties?[key]?['value']?.toString() ?? '--';

  double _getNum(String key) =>
      double.tryParse(_getValue(key)) ?? 0;

  String _getTimeAgo() {
    if (_lastUpdate == null) return '';
    final diff = DateTime.now().difference(_lastUpdate!);
    if (diff.inSeconds < 10) return '刚刚更新';
    if (diff.inMinutes < 1) return '${diff.inSeconds}秒前';
    if (diff.inHours < 1) return '${diff.inMinutes}分钟前';
    return '${diff.inHours}小时前';
  }

  @override
  Widget build(BuildContext context) {
    return RefreshIndicator(
      onRefresh: () async {
        setState(() {
          _loading = true;
          _error = null;
        });
        await _loadData();
      },
      child: SingleChildScrollView(
        physics: const AlwaysScrollableScrollPhysics(),
        child: Column(
          children: [
            _buildHeader(),
            if (_loading)
              const Padding(
                padding: EdgeInsets.all(80),
                child: CircularProgressIndicator(color: Colors.white),
              ),
            if (_error != null) _buildError(),
            if (!_loading && _error == null) ...[
              _buildSensorGrid(),
              _buildDeviceStatus(),
              _buildStatusBar(),
              _buildQuickActions(),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.fromLTRB(24, 48, 24, 32),
      decoration: const BoxDecoration(
        gradient: LinearGradient(
          colors: [Color(0xFF1A237E), Color(0xFF283593), Color(0xFF3949AB)],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.vertical(bottom: Radius.circular(32)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: Colors.white.withOpacity(0.15),
                  borderRadius: BorderRadius.circular(14),
                ),
                child: const Icon(Icons.kitchen, color: Colors.white, size: 28),
              ),
              const SizedBox(width: 14),
              const Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('智能厨房监控',
                      style: TextStyle(
                          color: Colors.white,
                          fontSize: 22,
                          fontWeight: FontWeight.bold)),
                  Text('环境数据实时监测',
                      style: TextStyle(color: Colors.white70, fontSize: 13)),
                ],
              ),
              const Spacer(),
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
                decoration: BoxDecoration(
                  color: Colors.green.withOpacity(0.2),
                  borderRadius: BorderRadius.circular(20),
                  border: Border.all(color: Colors.greenAccent.withOpacity(0.4)),
                ),
                child: const Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.check_circle,
                        color: Colors.greenAccent, size: 14),
                    SizedBox(width: 4),
                    Text('在线',
                        style:
                            TextStyle(color: Colors.greenAccent, fontSize: 12)),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 14),
          Row(
            children: [
              const Icon(Icons.access_time,
                  color: Colors.white60, size: 16),
              const SizedBox(width: 6),
              Text(
                '${_now.year}年${_now.month}月${_now.day}日  '
                '${_now.hour.toString().padLeft(2, '0')}:'
                '${_now.minute.toString().padLeft(2, '0')}:'
                '${_now.second.toString().padLeft(2, '0')}',
                style: const TextStyle(
                    color: Colors.white60,
                    fontSize: 14,
                    fontWeight: FontWeight.w500),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildError() {
    return Padding(
      padding: const EdgeInsets.all(24),
      child: Container(
        padding: const EdgeInsets.all(24),
        decoration: BoxDecoration(
          color: Colors.red[50],
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: Colors.red[100]!),
        ),
        child: Column(
          children: [
            const Icon(Icons.cloud_off, color: Colors.red, size: 48),
            const SizedBox(height: 12),
            const Text('加载失败',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            Text(_error!,
                style: const TextStyle(fontSize: 12, color: Colors.redAccent),
                textAlign: TextAlign.center),
            const SizedBox(height: 16),
            ElevatedButton.icon(
              onPressed: () {
                setState(() {
                  _loading = true;
                  _error = null;
                });
                _loadData();
              },
              icon: const Icon(Icons.refresh),
              label: const Text('重试'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSensorGrid() {
    final temp = _getNum('DHT');
    final hum = _getNum('humidity');
    final smoke = _getNum('MQ');

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          Row(
            children: [
              Expanded(
                child: _SensorCard(
                  icon: Icons.thermostat,
                  value: _getValue('DHT'),
                  unit: '°C',
                  label: '温度',
                  gradient: const [Color(0xFFFF6B6B), Color(0xFFEE5A24)],
                  status: _tempStatus(temp),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: _SensorCard(
                  icon: Icons.water_drop,
                  value: _getValue('humidity'),
                  unit: '%',
                  label: '湿度',
                  gradient: const [Color(0xFF74B9FF), Color(0xFF0984E3)],
                  status: _humStatus(hum),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          _SensorCard(
            icon: Icons.air,
            value: _getValue('MQ'),
            unit: '',
            label: '烟雾浓度',
            gradient: smoke > 0
                ? const [Color(0xFFFFA502), Color(0xFFE17055)]
                : const [Color(0xFF00B894), Color(0xFF00CEC9)],
            status: _smokeStatus(smoke),
            fullWidth: true,
          ),
        ],
      ),
    );
  }

  String _tempStatus(double v) => v > 35 ? '⚠️ 高温' : v > 10 ? '✅ 正常' : '❄️ 低温';
  String _humStatus(double v) => v > 70 ? '💧 偏湿' : v > 30 ? '✅ 正常' : '🏜 干燥';
  String _smokeStatus(double v) => v > 0 ? '🚨 检测到烟雾' : '✅ 空气安全';

  bool _ledOn() => _getValue('led').toLowerCase() == 'true';
  // 门锁设备值与用户理解相反：设备 true=解锁，用户"已锁定"=显示锁定
  bool _lockOn() => _getValue('lock').toLowerCase() != 'true';

  Future<void> _toggleLock() async {
    try {
      // 通过 Provider 调用（已处理设备值反转）
      final provider = context.read<DeviceProvider>();
      final lockNew = !provider.lockStatus;
      final success = await provider.setLockStatus(lockNew);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(success
              ? (lockNew ? '✅ 门已锁定' : '✅ 门已解锁')
              : '❌ 操作失败'),
        ),
      );
      setState(() { _loading = true; _error = null; });
      await _loadData();
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('❌ $e'), duration: const Duration(seconds: 5)),
      );
    }
  }

  Widget _buildDeviceStatus() {
    // 统一读 Provider：lockStatus=true=锁定，false=解锁，默认已锁定
    final provider = context.watch<DeviceProvider>();
    final isLocked = provider.lockStatus;
    final isLightOn = provider.lightStatus ?? false;
    // 按住开门或消防解锁时，强制显示解锁状态
    final isUnlocking = provider.isUnlocking || provider.isAutoUnlocking;
    final lockOn = isUnlocking ? false : isLocked;

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Row(
        children: [
          Expanded(
            child: _StatusCard(
              icon: lockOn ? Icons.lock : Icons.lock_open,
              label: '厨房门',
              value: lockOn ? '已锁定' : '已解锁',
              on: lockOn,
              activeColor: Colors.blue,       // 锁定→蓝色
              inactiveColor: Colors.green,    // 解锁→绿色
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: _StatusCard(
              icon: Icons.lightbulb,
              label: '光照灯',
              value: isLightOn ? '已开启' : '已关闭',
              on: isLightOn,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildStatusBar() {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
        decoration: BoxDecoration(
          color: Colors.grey[100],
          borderRadius: BorderRadius.circular(16),
        ),
        child: Row(
          children: [
            const Icon(Icons.access_time, size: 18, color: Colors.grey),
            const SizedBox(width: 8),
            Text('最后更新: ${_getTimeAgo()}',
                style: const TextStyle(color: Colors.grey, fontSize: 13)),
            const Spacer(),
            GestureDetector(
              onTap: () {
                setState(() {
                  _loading = true;
                  _error = null;
                });
                _loadData();
              },
              child: const Icon(Icons.refresh, size: 20, color: Colors.blue),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildQuickActions() {
    final provider = context.watch<DeviceProvider>();
    final isLocked = provider.lockStatus;
    final isUnlocking = provider.isUnlocking || provider.isAutoUnlocking;
    final lockOn = isUnlocking ? false : isLocked;
    return Padding(
      padding: const EdgeInsets.all(16),
      child: Row(
        children: [
          Expanded(
            child: _ActionButton(
              icon: Icons.meeting_room,
              label: lockOn ? '远程关门' : '远程开门',
              color: lockOn ? const Color(0xFFE17055) : const Color(0xFF0984E3),
              onTap: () => _toggleLock(),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: _ActionButton(
              icon: Icons.lightbulb,
              label: '光照控制',
              color: const Color(0xFFFDCB6E),
              onTap: () => mainPageKey.currentState?.setPageIndex(2),
            ),
          ),
        ],
      ),
    );
  }
}

class _StatusCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final bool? on;
  final Color activeColor;    // on=true 时的高亮色
  final Color? inactiveColor; // on=false 时的高亮色，null 则灰

  const _StatusCard({
    required this.icon,
    required this.label,
    required this.value,
    required this.on,
    this.activeColor = Colors.green,
    this.inactiveColor,
  });

  @override
  Widget build(BuildContext context) {
    final Color bgColor;
    final Color iconColor;
    if (on == null) {
      bgColor = Colors.grey[100]!;
      iconColor = Colors.grey;
    } else if (on!) {
      bgColor = activeColor.withOpacity(0.1);
      iconColor = activeColor;
    } else if (inactiveColor != null) {
      bgColor = inactiveColor!.withOpacity(0.1);
      iconColor = inactiveColor!;
    } else {
      bgColor = Colors.grey[200]!;
      iconColor = Colors.grey[600]!;
    }

    return Container(
      padding: const EdgeInsets.symmetric(vertical: 14, horizontal: 16),
      decoration: BoxDecoration(
        color: bgColor,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: iconColor.withOpacity(0.2)),
      ),
      child: Row(
        children: [
          Container(
            padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(
              color: iconColor.withOpacity(0.15),
              borderRadius: BorderRadius.circular(10),
            ),
            child: Icon(icon, color: iconColor, size: 22),
          ),
          const SizedBox(width: 12),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(label,
                  style: const TextStyle(
                      fontSize: 12, color: Colors.grey)),
              const SizedBox(height: 2),
              Text(value,
                  style: TextStyle(
                      fontSize: 15,
                      fontWeight: FontWeight.w600,
                      color: iconColor)),
            ],
          ),
        ],
      ),
    );
  }
}

class _SensorCard extends StatelessWidget {
  final IconData icon;
  final String value;
  final String unit;
  final String label;
  final List<Color> gradient;
  final String status;
  final bool fullWidth;

  const _SensorCard({
    required this.icon,
    required this.value,
    required this.unit,
    required this.label,
    required this.gradient,
    required this.status,
    this.fullWidth = false,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      width: fullWidth ? double.infinity : null,
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: gradient,
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(20),
        boxShadow: [
          BoxShadow(
            color: gradient.last.withOpacity(0.3),
            blurRadius: 12,
            offset: const Offset(0, 6),
          ),
        ],
      ),
      child: fullWidth
          ? Row(
              children: [
                Icon(icon, color: Colors.white, size: 36),
                const SizedBox(width: 16),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(value,
                        style: const TextStyle(
                            color: Colors.white,
                            fontSize: 30,
                            fontWeight: FontWeight.bold)),
                    const SizedBox(height: 2),
                    Text(label,
                        style: const TextStyle(
                            color: Colors.white70, fontSize: 13)),
                  ],
                ),
                const Spacer(),
                Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                  decoration: BoxDecoration(
                    color: Colors.white.withOpacity(0.2),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Text(status,
                      style: const TextStyle(
                          color: Colors.white, fontSize: 12)),
                ),
              ],
            )
          : Column(
              children: [
                Icon(icon, color: Colors.white, size: 32),
                const SizedBox(height: 10),
                RichText(
                  text: TextSpan(
                    children: [
                      TextSpan(
                        text: value,
                        style: const TextStyle(
                            color: Colors.white,
                            fontSize: 28,
                            fontWeight: FontWeight.bold),
                      ),
                      TextSpan(
                        text: unit,
                        style: const TextStyle(
                            color: Colors.white70, fontSize: 16),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 4),
                Text(label,
                    style:
                        const TextStyle(color: Colors.white70, fontSize: 13)),
                const SizedBox(height: 4),
                Text(status,
                    style: const TextStyle(color: Colors.white60, fontSize: 11)),
              ],
            ),
    );
  }
}

class _ActionButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color color;
  final VoidCallback onTap;

  const _ActionButton({
    required this.icon,
    required this.label,
    required this.color,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 18),
        decoration: BoxDecoration(
          color: color.withOpacity(0.1),
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: color.withOpacity(0.3)),
        ),
        child: Column(
          children: [
            Icon(icon, color: color, size: 28),
            const SizedBox(height: 6),
            Text(label,
                style: TextStyle(
                    color: color, fontSize: 14, fontWeight: FontWeight.w600)),
          ],
        ),
      ),
    );
  }
}