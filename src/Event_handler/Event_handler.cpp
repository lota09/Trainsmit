#include "Event_handler.h"

#include <Wire.h>


#define MPU_ADDR 0x68

// 임계값 설정 (실제 환경에 맞춰 튜닝 필요)
const int16_t ACCEL_KNOCK_THRESHOLD = 15000;  // Z축 충격
const int16_t GYRO_DOOR_THRESHOLD = 5000;     // 회전 속도

bool initMPU() {
  Wire.begin(MPU_SDA, MPU_SCL);

  // MPU6050 깨우기 (Sleep 모드 해제)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0x00);  // 0으로 설정하여 깨움
  if (Wire.endTransmission() != 0) return false;

  // 가속도 범위 설정 (+/- 8g)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  return true;
}

EventType checkMPUEvent() {
  int16_t ax, ay, az, gx, gy, gz;

  // 데이터 읽기 시작 (0x3B 레지스터부터 14바이트 - 가속도, 온도, 자이로)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  if (Wire.available() >= 14) {
    ax = Wire.read() << 8 | Wire.read();
    ay = Wire.read() << 8 | Wire.read();
    az = Wire.read() << 8 | Wire.read();
    Wire.read();
    Wire.read();  // 온도 데이터 건너뜀
    gx = Wire.read() << 8 | Wire.read();
    gy = Wire.read() << 8 | Wire.read();
    gz = Wire.read() << 8 | Wire.read();
  } else {
    return EVENT_NONE;
  }

  // --- 판별 로직 ---

  // 1. 문 열림 판별: 자이로(회전) 값이 크게 발생하면 문이 움직이는 중임
  // 문이 위아래(Z축) 보다는 옆으로(X or Y) 회전한다고 가정
  if (abs(gx) > GYRO_DOOR_THRESHOLD || abs(gy) > GYRO_DOOR_THRESHOLD) {
    return EVENT_DOOR;
  }

  // 2. 노크 판별: 기기를 직접 쳤을 때 발생하는 날카로운 가속도 변화
  // 기기 평면(보통 Z축)에 수직인 충격을 감지
  if (abs(az) > ACCEL_KNOCK_THRESHOLD) {
    return EVENT_KNOCK;
  }

  return EVENT_NONE;
}
