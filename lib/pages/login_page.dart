// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../config/app_config.dart';

class LoginPage extends StatefulWidget {
  final VoidCallback onSuccess;

  const LoginPage({super.key, required this.onSuccess});

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final _controller = TextEditingController();
  final _focusNode = FocusNode();
  String _error = '';

  @override
  void dispose() {
    _controller.dispose();
    _focusNode.dispose();
    super.dispose();
  }

  void _submit() {
    if (_controller.text == AppConfig.appPassword) {
      widget.onSuccess();
    } else {
      setState(() {
        _error = '密码错误，请重试';
        _controller.clear();
      });
      _focusNode.requestFocus();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: [Color(0xFF1565C0), Color(0xFF42A5F5), Color(0xFF90CAF9)],
          ),
        ),
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.symmetric(horizontal: 32),
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 40),
              decoration: BoxDecoration(
                color: Colors.white.withOpacity(0.95),
                borderRadius: BorderRadius.circular(20),
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withOpacity(0.15),
                    blurRadius: 20,
                    offset: const Offset(0, 10),
                  ),
                ],
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Container(
                    padding: const EdgeInsets.all(16),
                    decoration: BoxDecoration(
                      color: Colors.blue[50],
                      shape: BoxShape.circle,
                    ),
                    child: const Icon(Icons.lock, size: 48, color: Colors.blue),
                  ),
                  const SizedBox(height: 20),
                  const Text(
                    '智能厨房监控系统',
                    style: TextStyle(
                        fontSize: 24,
                        fontWeight: FontWeight.bold,
                        color: Color(0xFF1565C0)),
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    '请输入密码以继续',
                    style: TextStyle(fontSize: 14, color: Colors.grey),
                  ),
                  const SizedBox(height: 32),
                  TextField(
                    controller: _controller,
                    focusNode: _focusNode,
                    autofocus: true,
                    obscureText: true,
                    obscuringCharacter: '•',
                    keyboardType: TextInputType.number,
                    inputFormatters: [FilteringTextInputFormatter.digitsOnly],
                    textAlign: TextAlign.center,
                    style: const TextStyle(
                        fontSize: 24,
                        letterSpacing: 12,
                        fontWeight: FontWeight.w500),
                    decoration: InputDecoration(
                      hintText: '输入密码',
                      hintStyle: TextStyle(
                          fontSize: 16,
                          letterSpacing: 2,
                          color: Colors.grey[400]),
                      filled: true,
                      fillColor: Colors.grey[50],
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(12),
                        borderSide: BorderSide.none,
                      ),
                      enabledBorder: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(12),
                        borderSide: BorderSide(color: Colors.grey[200]!),
                      ),
                      focusedBorder: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(12),
                        borderSide:
                            const BorderSide(color: Colors.blue, width: 2),
                      ),
                      contentPadding: const EdgeInsets.symmetric(
                          horizontal: 20, vertical: 18),
                    ),
                    onSubmitted: (_) => _submit(),
                  ),
                  const SizedBox(height: 12),
                  Text(
                    _error,
                    style: const TextStyle(color: Colors.red, fontSize: 13),
                  ),
                  const SizedBox(height: 24),
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton(
                      onPressed: _submit,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF1565C0),
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(vertical: 16),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(12)),
                        elevation: 4,
                      ),
                      child: const Text('进入',
                          style: TextStyle(
                              fontSize: 16, fontWeight: FontWeight.bold)),
                    ),
                  ),
                  const SizedBox(height: 8),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}