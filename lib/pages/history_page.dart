// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/pages/history_page.dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';

class HistoryPage extends StatefulWidget {
  const HistoryPage({super.key});

  @override
  State<HistoryPage> createState() => _HistoryPageState();
}

class _HistoryPageState extends State<HistoryPage> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      Provider.of<DeviceProvider>(context, listen: false).loadAccessLogs();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<DeviceProvider>(
      builder: (context, provider, child) {
        return RefreshIndicator(
          onRefresh: () => provider.loadAccessLogs(),
          child: _buildBody(provider),
        );
      },
    );
  }

  Widget _buildBody(DeviceProvider provider) {
    if (provider.isLoading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (provider.accessLogs.isEmpty) {
      return ListView(
        physics: const AlwaysScrollableScrollPhysics(),
        children: const [
          SizedBox(height: 200),
          Center(
            child: Column(
              children: [
                Icon(Icons.history, size: 64, color: Colors.grey),
                SizedBox(height: 16),
                Text('暂无操作记录',
                    style: TextStyle(fontSize: 16, color: Colors.grey)),
              ],
            ),
          ),
        ],
      );
    }

    return ListView.builder(
      physics: const AlwaysScrollableScrollPhysics(),
      itemCount: provider.accessLogs.length,
      itemBuilder: (context, index) {
        final log = provider.accessLogs[index];

        // 图标：按 type 区分，光照灯用灯泡，门锁用锁；解锁状态绿/橙色，关闭状态黑/灰色
        final text = log.operator;
        final isLight = log.type == 'light';
        final isUnlockLine = text.contains('解锁') && !text.contains('停止');
        late final IconData icon;
        late final Color iconColor;
        if (isLight) {
          // 光照灯：已解锁→橙色实心灯泡；已关闭→灰色空心灯泡
          if (text.contains('解锁')) {
            icon = Icons.lightbulb;
            iconColor = Colors.orange;
          } else {
            icon = Icons.lightbulb_outline;
            iconColor = Colors.grey;
          }
        } else {
          // 门锁：解锁状态→绿色 lock_open；关闭→黑色 lock
          if (isUnlockLine) {
            icon = Icons.lock_open;
            iconColor = Colors.green;
          } else {
            icon = Icons.lock;
            iconColor = Colors.black;
          }
        }
        final bgColor = iconColor.withOpacity(0.1);
        // 右侧"成功"标签统一绿色
        const tagColor = Color(0xFFE8F5E9);
        const tagBorder = Color(0xFFA5D6A7);
        const tagTextColor = Color(0xFF2E7D32);
        // 将文字按"解锁"拆分：解锁状态行"解锁"绿色，其他（如停止消防解锁）全黑
        final parts = text.split('解锁');
        final spans = <TextSpan>[];
        for (int i = 0; i < parts.length; i++) {
          spans.add(TextSpan(
            text: parts[i],
            style: const TextStyle(
              color: Colors.black,
              fontWeight: FontWeight.w600,
            ),
          ));
          if (i < parts.length - 1) {
            spans.add(TextSpan(
              text: '解锁',
              style: TextStyle(
                color: isUnlockLine ? Colors.green : Colors.black,
                fontWeight: FontWeight.w600,
              ),
            ));
          }
        }

        return Card(
          margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
          elevation: 1,
          child: ListTile(
            leading: Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: bgColor,
                borderRadius: BorderRadius.circular(10),
              ),
              child: Icon(icon, color: iconColor, size: 24),
            ),
            title: RichText(
              text: TextSpan(
                style: const TextStyle(
                  fontSize: 14,
                  color: Colors.black,
                ),
                children: spans,
              ),
            ),
            subtitle: Text(
              log.time,
              style: const TextStyle(fontSize: 12, color: Colors.grey),
            ),
            trailing: Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
              decoration: BoxDecoration(
                color: tagColor,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: tagBorder),
              ),
              child: Text(
                log.status,
                style: TextStyle(
                  fontSize: 12,
                  fontWeight: FontWeight.w500,
                  color: tagTextColor,
                ),
              ),
            ),
          ),
        );
      },
    );
  }
}