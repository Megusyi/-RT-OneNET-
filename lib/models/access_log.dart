// lib/models/access_log.dart
class AccessLog {
  final String id;
  final String time;
  final String type;
  final String operator;
  final String status;

  AccessLog({
    required this.id,
    required this.time,
    required this.type,
    required this.operator,
    required this.status,
  });
}
