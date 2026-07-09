// lib/models/door_status.dart
class DoorStatus {
  final bool isOpen;
  final String lastOpenTime;
  final String lastCloseTime;
  final int openCountToday;

  DoorStatus({
    required this.isOpen,
    required this.lastOpenTime,
    required this.lastCloseTime,
    required this.openCountToday,
  });
}
