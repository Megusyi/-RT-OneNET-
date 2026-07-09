// lib/models/device_data.dart
class DeviceData {
  final double temperature;
  final double humidity;
  final DateTime timestamp;

  DeviceData({
    required this.temperature,
    required this.humidity,
    required this.timestamp,
  });
}
