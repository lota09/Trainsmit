#include "Battery_LED.h"
#include <math.h>

// 모듈 내부 상태 변수
static float sharedPercentage = 0;
static float sharedFreq = 0.25;
static int sharedR = 0, sharedG = 0;

void initBatteryLED() {
  // 핀 모드 설정은 ADC의 경우 생략 가능하나 명시적으로 처리
  analogReadResolution(12);
  updateBatteryValue(); // 초기값 갱신
}

void updateBatteryValue() {
  int readMv = analogReadMilliVolts(BATT_ADC_PIN);
  float voltage = (readMv * 2.0) / 1000.0;

  sharedPercentage = (voltage - 3.0) / (4.2 - 3.0) * 100.0;
  sharedPercentage = constrain(sharedPercentage, 0.0f, 100.0f);

  // 1. 주파수 결정 (Staged)
  if (sharedPercentage <= 15.0f)
    sharedFreq = 4.0f;
  else if (sharedPercentage <= 30.0f)
    sharedFreq = 2.0f;
  else if (sharedPercentage <= 45.0f)
    sharedFreq = 1.0f;
  else
    sharedFreq = 0.25f;

  // 2. 색상 결정 (Continuous Gradient)
  if (sharedPercentage >= 50.0f) {
    // 100% -> 50% 구간: g 120 > 100, r 0 > 120
    float ratio = (sharedPercentage - 50.0f) / 50.0f;
    sharedR = (int)(120 * (1.0f - ratio));
    sharedG = 100 + (int)(20 * ratio);
  } else if (sharedPercentage >= 30.0f) {
    // 50% -> 30% 구간: g 100 > 0, r 120
    float ratio = (sharedPercentage - 30.0f) / 20.0f;
    sharedR = 120;
    sharedG = (int)(100 * ratio);
  } else {
    // 30% 이하: 완전 빨간색
    sharedR = 120;
    sharedG = 0;
  }
}

void handleLEDAnimation(bool isDetected) {
  if (isDetected) {
    // millis()를 활용한 비차단 애니메이션
    float time = millis() / 1000.0f;
    float brightness = (sin(2.0f * M_PI * sharedFreq * time) + 1.0f) / 2.0f;

    neopixelWrite(RGB_LED_PIN, (int)(sharedR * brightness),
                  (int)(sharedG * brightness), 0);
  } else {
    // 감지되지 않을 때는 즉시 소등
    neopixelWrite(RGB_LED_PIN, 0, 0, 0);
  }
}
