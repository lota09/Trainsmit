#include <Wire.h>

#include "esp_sleep.h"


// --- 하드웨어 설정 ---
#define MPU_ADDR      0x68
#define MPU_SDA       4
#define MPU_SCL       5
#define MPU_INT_PIN   6  // MPU6050 INT 핀과 연결된 ESP32-C6 핀
#define INDICATOR_LED 8  // C6 DevKit 내장 RGB LED (NeoPixel)

// --- MPU6050 레지스터 ---
#define REG_INT_ENABLE   0x38
#define REG_INT_STATUS   0x3A
#define REG_MOT_THR      0x1F  // Motion threshold
#define REG_MOT_DUR      0x20  // Motion duration
#define REG_ACCEL_CONFIG 0x1C
#define REG_PWR_MGMT_1   0x6B

void configureMPU();
void analyzeVibration();

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(MPU_SDA, MPU_SCL);

  // INT 핀을 풀다운으로 설정하여 플로팅 방지
  pinMode(MPU_INT_PIN, INPUT_PULLDOWN);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    Serial.println(">>> Woken up by MPU6050 Interrupt!");

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_PWR_MGMT_1);
    Wire.write(0x00);
    Wire.endTransmission();

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_INT_STATUS);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 1);
    uint8_t intStatus = Wire.read();

    if (intStatus & 0x40) {
      analyzeVibration();
    }
  } else {
    Serial.println(">>> Normal Boot. Initializing System...");
    configureMPU();

    neopixelWrite(INDICATOR_LED, 0, 20, 0);
    delay(500);
    neopixelWrite(INDICATOR_LED, 0, 0, 0);
  }

  Serial.println(">>> Preparing for Deep Sleep...");

  // 잠들기 직전 인터럽트 상태 최종 클리어 (매우 중요)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_INT_STATUS);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1);
  Wire.read();

  esp_sleep_enable_ext1_wakeup(1ULL << MPU_INT_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);

  Serial.println(">>> Zzz...");
  Serial.flush();
  neopixelWrite(INDICATOR_LED, 0, 0, 0);

  esp_deep_sleep_start();
}

void loop() {
  // Deep Sleep 모드에서는 setup()만 실행되고 loop()는 도달하지 않습니다.
}

void configureMPU() {
  // 1. MPU6050 깨우기 및 클락 설정
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_PWR_MGMT_1);
  Wire.write(0x01);
  Wire.endTransmission();

  // 2. 인터럽트 핀 설정 (Latching 유지)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x37);  // INT_PIN_CFG
  Wire.write(0x20);  // LATCH_INT_EN = 1 (읽을 때까지 HIGH 유지)
  Wire.endTransmission();

  // 3. 가속도 범위 및 고역 통과 필터(HPF) 설정
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_CONFIG);
  Wire.write(0x01);  // +/- 2g 범위 & 5Hz HPF 활성화
  Wire.endTransmission();

  // 4. Motion Interrupt 임계값 (더 민감하게 10으로 조정)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_MOT_THR);
  Wire.write(10);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_MOT_DUR);
  Wire.write(1);
  Wire.endTransmission();

  // 5. 모션 인터럽트 활성화
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_INT_ENABLE);
  Wire.write(0x40);
  Wire.endTransmission();
}

void analyzeVibration() {
  int16_t ax, ay, az;
  int16_t lastAx = 0, lastAy = 0;
  int32_t totalDiff = 0;
  int samples = 0;

  uint32_t startTime = millis();
  while (millis() - startTime < 50) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true);

    if (Wire.available() >= 6) {
      ax = Wire.read() << 8 | Wire.read();
      ay = Wire.read() << 8 | Wire.read();
      az = Wire.read() << 8 | Wire.read();

      if (samples > 0) {
        totalDiff += abs(ax - lastAx) + abs(ay - lastAy);
      }
      lastAx = ax;
      lastAy = ay;
      samples++;
    }
    delay(5);
  }

  float averageDiff = (float)totalDiff / samples;
  Serial.print("   > Variance: ");
  Serial.println(averageDiff);

  if (averageDiff > 20000.0) {
    Serial.println(">>> [DOOR MOVEMENT] Result: RED");
    // 빨간색 LED 표시
    neopixelWrite(INDICATOR_LED, 255, 0, 0);
    delay(1000);
  } else {
    Serial.println(">>> [KNOCK] Result: BLUE");
    // 파란색 LED 표시
    neopixelWrite(INDICATOR_LED, 0, 0, 255);
    delay(1000);
  }
}
