/*
 * Trainsmit Project - Event-driven Battery Monitor
 * Target: ESP32-C6 DevKit
 *
 * 기능:
 * 1. PIR 감지(GPIO 4) 시에만 LED 작동
 * 2. 감지 시 1회만 배터리 측정 (전력 및 리소스 최적화)
 * 3. millis() 기반 비차단 애니메이션 적용
 */

#define BATT_ADC_PIN 3
#define RGB_LED_PIN 8
#define PIR_PIN 4

// 공유 변수 (이벤트 발생 시에만 갱신)
float sharedPercentage = 0;
float sharedFreq = 0.25;
int sharedR = 0, sharedG = 0;

void updateBattery() {
  int readMv = analogReadMilliVolts(BATT_ADC_PIN);
  float voltage = (readMv * 2.0) / 1000.0;

  sharedPercentage = (voltage - 3.0) / (4.2 - 3.0) * 100.0;
  sharedPercentage = constrain(sharedPercentage, 0, 100);

  // 주파수 및 색상 미리 계산 (연산량 분산)
  if (sharedPercentage <= 15)
    sharedFreq = 4.0;
  else if (sharedPercentage <= 30)
    sharedFreq = 2.0;
  else if (sharedPercentage <= 45)
    sharedFreq = 1.0;
  else
    sharedFreq = 0.25;

  if (sharedPercentage >= 50) {
    float ratio = (sharedPercentage - 50) / 50.0;
    sharedR = (int)(120 * (1.0 - ratio));
    sharedG = 100 + (int)(20 * ratio);
  } else if (sharedPercentage >= 30) {
    float ratio = (sharedPercentage - 30) / 20.0;
    sharedR = 120;
    sharedG = (int)(100 * ratio);
  } else {
    sharedR = 120;
    sharedG = 0;
  }
  Serial.printf("Battery Refreshed: %.2fV\n", voltage);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  analogReadResolution(12);
  updateBattery(); // 시작 시 1회 측정
}

void loop() {
  bool isDetected = digitalRead(PIR_PIN);
  static bool lastDetected = false;

  // 1. 이벤트 트리거: 감지된 직후 1회만 배터리 전압 갱신
  if (isDetected && !lastDetected) {
    updateBattery();
  }
  lastDetected = isDetected;

  // 2. 비차단 LED 애니메이션
  if (isDetected) {
    // millis()를 사용하여 절대 시간에 따른 밝기 계산
    float time = millis() / 1000.0;
    float brightness = (sin(2 * PI * sharedFreq * time) + 1.0) / 2.0;

    neopixelWrite(RGB_LED_PIN, (int)(sharedR * brightness),
                  (int)(sharedG * brightness), 0);
  } else {
    neopixelWrite(RGB_LED_PIN, 0, 0, 0); // 미감지 시 소등
  }

  // 10ms 주기 보장 (CPU 사용률 조절)
  delay(10);
}
