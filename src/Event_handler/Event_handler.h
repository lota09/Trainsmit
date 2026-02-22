#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <Arduino.h>

// 핀 정의 (ESP32-C6 기준 미사용 핀 추천)
#define MPU_SDA 4
#define MPU_SCL 5
#define MPU_INT 6

enum EventType {
  EVENT_NONE,
  EVENT_KNOCK,  // 화면 깨움 (진동 위주)
  EVENT_DOOR    // 문 열림 (회전 위주)
};

bool initMPU();
EventType checkMPUEvent();

#endif
