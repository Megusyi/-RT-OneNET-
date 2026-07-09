// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/pages/control_page.dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';

class ControlPage extends StatefulWidget {
  const ControlPage({super.key});

  @override
  State<ControlPage> createState() => _ControlPageState();
}

class _ControlPageState extends State<ControlPage> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final provider = context.read<DeviceProvider>();
      // 只在数据还未加载时再请求，避免重复
      if (provider.currentData == null) {
        provider.loadDeviceProperties();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          // 光照控制卡片
          Consumer<DeviceProvider>(
            builder: (context, provider, child) {
              return Card(
                elevation: 4,
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Column(
                        children: [
                          Icon(
                            Icons.lightbulb,
                            size: 48,
                            color: provider.lightStatus ?? false
                                ? Colors.yellow
                                : Colors.grey,
                          ),
                          const SizedBox(height: 8),
                          const Text('光照灯', style: TextStyle(fontSize: 16)),
                        ],
                      ),
                      Switch(
                        value: provider.lightStatus ?? false,
                        onChanged: (value) async {
                          try {
                            final success = await provider.setLightStatus(value);
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                duration: const Duration(seconds: 2),
                                content: Text(
                                  success
                                      ? (value ? '✅ 光照灯已开启' : '✅ 光照灯已关闭')
                                      : '❌ 操作失败',
                                ),
                              ),
                            );
                          } catch (e) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                duration: const Duration(seconds: 2),
                                content: Text('❌ $e'),
                              ),
                            );
                          }
                        },
                        activeColor: Colors.yellow,
                        activeTrackColor: Colors.yellow[200],
                      ),
                    ],
                  ),
                ),
              );
            },
          ),
          const SizedBox(height: 16),
          // 门锁控制卡片（模式：按住开门 / 消防解锁）
          Consumer<DeviceProvider>(
            builder: (context, provider, child) {
              final isPressing = provider.isUnlocking;
              final isAuto = provider.isAutoUnlocking;
              // 设备上报的实际状态：lockStatus=false 表示解锁，true/空 表示锁定
              final isDeviceOpen = provider.lockStatus == false;
              // 解锁状态：用户正在按住开门、消防解锁、或设备实际上报已解锁
              final isOpen = isPressing || isAuto || isDeviceOpen;
              return Card(
                elevation: 4,
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    children: [
                      // 顶部：状态图标
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Icon(
                                isOpen ? Icons.lock_open : Icons.lock,
                                size: 48,
                                color: isAuto
                                    ? Colors.orange
                                    : (isPressing ? Colors.green : Colors.blue),
                              ),
                              const SizedBox(height: 8),
                              const Text('门锁', style: TextStyle(fontSize: 16)),
                              Text(
                                isAuto
                                    ? '消防解锁中…'
                                    : (isPressing
                                        ? '正在开门…'
                                        : (isDeviceOpen ? '门已解锁' : '已锁定')),
                                style: TextStyle(
                                  color: isOpen
                                      ? (isAuto ? Colors.orange : Colors.green)
                                      : Colors.grey,
                                ),
                              ),
                            ],
                          ),
                          // 当前模式指示徽章
                          Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 6,
                            ),
                            decoration: BoxDecoration(
                              color: isAuto
                                  ? Colors.orange[50]
                                  : (isPressing
                                      ? Colors.green[50]
                                      : Colors.blue[50]),
                              borderRadius: BorderRadius.circular(16),
                              border: Border.all(
                                color: isAuto
                                    ? Colors.orange[200]!
                                    : (isPressing
                                        ? Colors.green[200]!
                                        : Colors.blue[200]!),
                              ),
                            ),
                            child: Text(
                              isAuto
                                  ? '消防模式'
                                  : '按住模式',
                              style: TextStyle(
                                color: isAuto
                                    ? Colors.orange[700]
                                    : (isPressing ? Colors.green[700]! : Colors.blue[700]!),
                                fontSize: 12,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 16),
                      // 底部：两个按钮行
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceAround,
                        children: [
                          // 按钮 1：消防解锁 - 点击切换
                          GestureDetector(
                            onTap: () => provider.toggleAutoUnlock(),
                            child: AnimatedContainer(
                              duration: const Duration(milliseconds: 150),
                              padding: const EdgeInsets.symmetric(
                                horizontal: 20,
                                vertical: 14,
                              ),
                              decoration: BoxDecoration(
                                color: isAuto ? Colors.orange : Colors.orange[300],
                                borderRadius: BorderRadius.circular(16),
                                boxShadow: [
                                  BoxShadow(
                                    color: Colors.orange.withOpacity(0.3),
                                    blurRadius: isAuto ? 12 : 6,
                                    spreadRadius: isAuto ? 2 : 1,
                                    offset: const Offset(0, 2),
                                  ),
                                ],
                              ),
                              child: Row(
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  Icon(
                                    isAuto ? Icons.local_fire_department : Icons.local_fire_department,
                                    color: Colors.white,
                                    size: 20,
                                  ),
                                  const SizedBox(width: 6),
                                  Text(
                                    isAuto ? '停止消防解锁' : '消防解锁',
                                    style: const TextStyle(
                                      color: Colors.white,
                                      fontSize: 14,
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ),
                          const SizedBox(width: 10),
                          // 按钮 2：按住开门 - Listener 按住模式
                          Listener(
                            onPointerDown: (_) => provider.startUnlockPulse(),
                            onPointerUp: (_) => provider.stopUnlockPulse(),
                            onPointerCancel: (_) => provider.stopUnlockPulse(),
                            child: AnimatedContainer(
                              duration: const Duration(milliseconds: 150),
                              padding: const EdgeInsets.symmetric(
                                horizontal: 20,
                                vertical: 14,
                              ),
                              decoration: BoxDecoration(
                                color: isPressing ? Colors.green : Colors.blue,
                                borderRadius: BorderRadius.circular(16),
                                boxShadow: [
                                  BoxShadow(
                                    color: (isPressing ? Colors.green : Colors.blue)
                                        .withOpacity(0.4),
                                    blurRadius: isPressing ? 12 : 6,
                                    spreadRadius: isPressing ? 2 : 1,
                                    offset: const Offset(0, 2),
                                  ),
                                ],
                              ),
                              child: Row(
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  Icon(
                                    isPressing ? Icons.lock_open : Icons.lock,
                                    color: Colors.white,
                                    size: 20,
                                  ),
                                  const SizedBox(width: 6),
                                  Text(
                                    isPressing ? '松开关门…' : '按住开门',
                                    style: const TextStyle(
                                      color: Colors.white,
                                      fontSize: 14,
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              );
            },
          ),
        ],
      ),
    );
  }
}