import 'package:flutter/material.dart';

class AppStyles {
  // 主色调配置
  static const Color primaryColor = Color(0xFF2196F3);
  static const Color primaryDark = Color(0xFF1976D2);
  static const Color successColor = Color(0xFF4CAF50);
  static const Color warningColor = Color(0xFFFF9800);
  static const Color errorColor = Color(0xFFF44336);

  // 文字样式
  static const TextStyle titleStyle = TextStyle(
    fontSize: 24,
    fontWeight: FontWeight.bold,
    color: Colors.black87,
  );

  static const TextStyle subtitleStyle = TextStyle(
    fontSize: 16,
    color: Colors.grey,
  );

  static const TextStyle cardTitleStyle = TextStyle(
    fontSize: 18,
    fontWeight: FontWeight.bold,
    color: Colors.black87,
  );

  static const TextStyle cardSubtitleStyle = TextStyle(
    fontSize: 14,
    color: Colors.grey,
  );

  // 卡片样式
  static BoxDecoration cardDecoration = BoxDecoration(
    color: Colors.white,
    borderRadius: BorderRadius.circular(16),
    boxShadow: [
      BoxShadow(
        color: Colors.grey.withOpacity(0.15),
        spreadRadius: 2,
        blurRadius: 8,
        offset: const Offset(0, 2),
      ),
    ],
  );

  // 按钮样式
  static ButtonStyle primaryButtonStyle = ElevatedButton.styleFrom(
    backgroundColor: primaryColor,
    foregroundColor: Colors.white,
    padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 14),
    textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
    elevation: 4,
  );

  static ButtonStyle successButtonStyle = ElevatedButton.styleFrom(
    backgroundColor: successColor,
    foregroundColor: Colors.white,
    padding: const EdgeInsets.symmetric(horizontal: 40, vertical: 16),
    textStyle: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
    elevation: 4,
  );

  static ButtonStyle outlineButtonStyle = OutlinedButton.styleFrom(
    foregroundColor: primaryColor,
    side: const BorderSide(color: primaryColor),
    padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 14),
    textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
  );
}
