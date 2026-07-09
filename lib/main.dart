// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/main.dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'pages/home_page.dart';
import 'pages/history_page.dart';
import 'pages/control_page.dart';
import 'pages/gallery_page.dart';
import 'pages/login_page.dart';
import 'providers/device_provider.dart';

final GlobalKey<_MainPageState> mainPageKey = GlobalKey<_MainPageState>();

void main() {
  runApp(
    ChangeNotifierProvider(
      create: (context) => DeviceProvider(),
      child: const MyApp(),
    ),
  );
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  bool _isAuthenticated = false;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: '智能门禁',
      theme: ThemeData(primarySwatch: Colors.blue),
      home: _isAuthenticated
          ? MainPage(key: mainPageKey)
          : LoginPage(onSuccess: () => setState(() => _isAuthenticated = true)),
    );
  }
}

class MainPage extends StatefulWidget {
  const MainPage({super.key});

  @override
  State<MainPage> createState() => _MainPageState();
}

class _MainPageState extends State<MainPage> {
  int _currentIndex = 0;

  final List<Widget> _pages = [
    const HomePage(),
    const HistoryPage(),
    const ControlPage(),
    const GalleryPage(),
  ];
  final List<String> _titles = ['智能门禁', '历史记录', '远程控制', '相册'];

  void setPageIndex(int index) => setState(() => _currentIndex = index);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(_titles[_currentIndex])),
      body: IndexedStack(
        index: _currentIndex,
        children: _pages,
      ),
      bottomNavigationBar: BottomNavigationBar(
        type: BottomNavigationBarType.fixed,
        selectedItemColor: Colors.blue,
        unselectedItemColor: Colors.grey,
        currentIndex: _currentIndex,
        onTap: (index) => setState(() => _currentIndex = index),
        items: const [
          BottomNavigationBarItem(icon: Icon(Icons.home), label: '首页'),
          BottomNavigationBarItem(icon: Icon(Icons.history), label: '历史'),
          BottomNavigationBarItem(
            icon: Icon(Icons.settings_remote),
            label: '控制',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.photo_library),
            label: '相册',
          ),
        ],
      ),
    );
  }
}