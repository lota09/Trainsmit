#ifndef BATTERY_LED_H
#define BATTERY_LED_H

#include <Arduino.h>

// --- 기본 핀 설정 (상황에 따라 수정 가능) ---
#ifndef BATT_ADC_PIN
#define BATT_ADC_PIN 3
#endif

#ifndef RGB_LED_PIN
#define RGB_LED_PIN 8
#endif

/**
 * @brief 배터리 및 LED 초기 설정
 */
void initBatteryLED();

/**
 * @brief 배터리 전압을 읽고 색상/주파수를 갱신 (이벤트 시 1회 호출 권장)
 */
void updateBatteryValue();

/**
 * @brief LED 애니메이션 처리 (loop에서 매번 호출, 비차단 방식)
 * @param isDetected 감지 상태 (true일 때만 LED 작동)
 */
void handleLEDAnimation(bool isDetected);

#endif
