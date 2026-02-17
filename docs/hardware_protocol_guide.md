# 하드웨어 선택 및 통신 프로토콜 심화 가이드

## 1. 아두이노 우노 vs ESP32 메모리 비교

### 메모리 요구사항 분석

#### TFT 디스플레이 메모리 계산
```
320x240 해상도, 16bit 컬러 (RGB565):
- 프레임버퍼 크기 = 320 × 240 × 2 bytes = 153,600 bytes (150KB)
- 부분 스프라이트 (100x100) = 100 × 100 × 2 = 20KB
- 최소 필요 메모리 ≈ 20KB + 프로그램 메모리 + 변수 메모리
```

#### 하드웨어 비교표

| 구분 | Arduino Uno | ESP32 DevKit | ESP32-C6 Dev C1 |
|------|-------------|--------------|-----------------|
| **MCU** | ATmega328P | Xtensa LX6 | RISC-V 32bit |
| **SRAM** | 2KB | 320KB | 512KB |
| **Flash** | 32KB | 4MB | 4MB |
| **WiFi** | ❌ | ✅ | ✅ |
| **클럭속도** | 16MHz | 240MHz | 160MHz |
| **적합성** | ❌ 부족 | ✅ 적합 | ✅ 매우 적합 |

### 결론: 아두이노 우노의 한계
```cpp
// 아두이노 우노의 현실
int sram_total = 2048;        // 2KB 총 SRAM
int program_vars = 500;       // 프로그램 변수들
int available = 1548;         // 실제 사용 가능
int needed_minimum = 20480;   // 최소 필요량 20KB

// 결과: 10배 이상 부족! 절대 불가능
```

**ESP32-C6 Dev C1 선택이 탁월한 이유:**
- 512KB SRAM으로 여유로운 메모리
- RISC-V 아키텍처로 전력 효율성 우수
- WiFi 6 지원으로 빠른 API 통신
- 최신 ESP-IDF 지원

## 2. I2C 프로토콜 - 6핀에서 2핀으로 줄인 원리

### 기존 병렬 LCD (6핀) 동작 원리

```cpp
// 병렬 방식: 각 핀이 개별 신호선
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
//               RS, EN, D4,D5,D6,D7

// 데이터 전송 시뮬레이션
void sendChar_Parallel(char c) {
    // 1단계: 상위 4비트 전송
    digitalWrite(RS, HIGH);      // 데이터 모드
    digitalWrite(D7, (c >> 7) & 1);  // 비트 7
    digitalWrite(D6, (c >> 6) & 1);  // 비트 6  
    digitalWrite(D5, (c >> 5) & 1);  // 비트 5
    digitalWrite(D4, (c >> 4) & 1);  // 비트 4
    digitalWrite(EN, HIGH);      // Enable 펄스
    digitalWrite(EN, LOW);
    
    // 2단계: 하위 4비트 전송
    digitalWrite(D7, (c >> 3) & 1);  // 비트 3
    digitalWrite(D6, (c >> 2) & 1);  // 비트 2
    digitalWrite(D5, (c >> 1) & 1);  // 비트 1
    digitalWrite(D4, (c >> 0) & 1);  // 비트 0
    digitalWrite(EN, HIGH);      // Enable 펄스
    digitalWrite(EN, LOW);
}
```

### I2C LCD (2핀) 동작 원리

#### PCF8574 I2C 확장 칩의 역할
```
[Arduino]     [PCF8574]     [LCD]
SDA ---------> SDA
SCL ---------> SCL           RS, EN, D4, D5, D6, D7
               P0 ---------> RS
               P1 ---------> RW  
               P2 ---------> EN
               P3 ---------> Backlight
               P4 ---------> D4
               P5 ---------> D5
               P6 ---------> D6
               P7 ---------> D7
```

#### I2C 통신 프로토콜 상세
```cpp
// I2C 프레임 구조
// [START][ADDRESS+W][ACK][DATA][ACK][STOP]

void sendChar_I2C(char c) {
    uint8_t address = 0x27;  // PCF8574 주소
    
    // 상위 4비트 전송
    uint8_t data_high = (c & 0xF0) | 0x0C;  // EN=1, RS=1, BL=1
    Wire.beginTransmission(address);
    Wire.write(data_high);
    Wire.endTransmission();
    
    data_high &= 0xFB;  // EN=0
    Wire.beginTransmission(address);
    Wire.write(data_high);
    Wire.endTransmission();
    
    // 하위 4비트 전송
    uint8_t data_low = ((c << 4) & 0xF0) | 0x0C;
    Wire.beginTransmission(address);
    Wire.write(data_low);
    Wire.endTransmission();
    
    data_low &= 0xFB;  // EN=0
    Wire.beginTransmission(address);
    Wire.write(data_low);
    Wire.endTransmission();
}
```

### I2C의 핵심 원리

#### 1. 직렬화 (Serialization)
```
병렬 6비트 데이터: RS EN D4 D5 D6 D7 (동시 전송)
         ↓ 직렬화
I2C 8비트 패킷: [D7 D6 D5 D4 BL EN RW RS] (순차 전송)
```

#### 2. 주소 기반 멀티플렉싱
```cpp
// 하나의 I2C 버스에 여러 장치 연결 가능
#define LCD_ADDRESS    0x27
#define RTC_ADDRESS    0x68  
#define EEPROM_ADDRESS 0x50

// 주소로 장치 구분
Wire.beginTransmission(LCD_ADDRESS);    // LCD만 응답
Wire.beginTransmission(RTC_ADDRESS);    // RTC만 응답
```

#### 3. 시간 vs 핀 트레이드오프
```
병렬 방식: 빠름 (1 클럭) but 많은 핀 (6개)
I2C 방식:  느림 (9 클럭) but 적은 핀 (2개)

1바이트 전송 시간:
- 병렬: 1μs (16MHz 기준)  
- I2C:   20μs (400kHz 기준)
```

## 3. RFID-RC522와 SPI 프로토콜

### RFID-RC522 핀 배치 확인
```
RFID-RC522 모듈:
┌─────────────┐
│ SDA  SCK    │ ← SPI 신호
│ MOSI MISO   │ ← SPI 신호  
│ IRQ  GND    │
│ RST  3.3V   │
└─────────────┘
```

**네, 맞습니다! RFID-RC522는 SPI 프로토콜을 사용합니다.**

#### 핀 대응 관계
```cpp
// ESP32와 RFID-RC522 연결
#define RST_PIN    22    // Reset
#define SS_PIN     21    // SDA (Slave Select/Chip Select)
#define MOSI_PIN   23    // Master Out Slave In
#define MISO_PIN   19    // Master In Slave Out  
#define SCK_PIN    18    // Serial Clock

// SPI 설정
SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
```

### SPI 프로토콜 in RFID 동작

#### 1. RFID 레지스터 읽기
```cpp
uint8_t readRegister(uint8_t reg) {
    digitalWrite(SS_PIN, LOW);        // CS 활성화
    
    // 명령어 전송 (읽기 = 0x80 | 레지스터 주소)
    SPI.transfer(0x80 | reg);         // MOSI로 명령 전송
    uint8_t value = SPI.transfer(0);  // MISO로 데이터 수신
    
    digitalWrite(SS_PIN, HIGH);       // CS 비활성화
    return value;
}
```

#### 2. RFID 카드 감지 과정
```cpp
void detectCard() {
    // 1. 안테나 켜기
    writeRegister(TxControlReg, 0x83);
    
    // 2. REQA 명령 전송 (카드 감지 요청)
    uint8_t command[] = {0x26};
    transceiveData(command, 1);
    
    // 3. 카드 응답 확인
    if (getResponseLength() == 2) {
        Serial.println("카드 감지됨!");
    }
}

void transceiveData(uint8_t* data, uint8_t len) {
    digitalWrite(SS_PIN, LOW);
    
    // 명령어 레지스터에 데이터 쓰기
    SPI.transfer(FIFODataReg);
    for(int i = 0; i < len; i++) {
        SPI.transfer(data[i]);        // 카드로 전송할 데이터
    }
    
    digitalWrite(SS_PIN, HIGH);
    
    // 전송 명령 실행
    writeRegister(CommandReg, Transceive_CMD);
}
```

#### 3. SPI 타이밍 다이어그램
```
CS   ┐     ┌─────────────────┐     ┌
     └─────┘                 └─────┘

SCK  ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
     ┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘

MOSI ──┬───┬───┬───┬───┬───┬───┬───┬──
       │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0
       
MISO ──┬───┬───┬───┬───┬───┬───┬───┬──
       │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0
```

### SPI vs I2C 비교 in 실제 사용

#### 속도 비교
```cpp
// SPI (RFID): 10MHz까지 가능
SPI.setClockDivider(SPI_CLOCK_DIV2);  // 8MHz
uint8_t data = SPI.transfer(0x26);    // 1μs 내 완료

// I2C (LCD): 400kHz 표준
Wire.setClock(400000);
Wire.write(data);                     // 20μs 소요
```

#### 사용 사례별 선택 기준
```
I2C 적합: LCD, RTC, 센서 (느려도 됨, 많은 장치 연결)
SPI 적합: 디스플레이, RFID, SD카드 (빠른 속도 필요)

프로젝트에서:
- TFT 디스플레이: SPI (빠른 픽셀 데이터 전송)
- PIR 센서: GPIO (단순 디지털 신호)  
- WiFi: 내장 (ESP32)
```

## 결론 및 권장사항

### 하드웨어 선택
- **ESP32-C6 Dev C1 강력 추천**: 512KB SRAM으로 여유로운 개발
- Arduino Uno는 메모리 부족으로 불가능

### 통신 프로토콜 전략
```cpp
// 프로젝트 통신 구성 예시
SPI: TFT 디스플레이 (고속 필요)
I2C: 추가 센서들 (RTC, 온습도 센서 등)
GPIO: PIR 센서 (단순 디지털)
WiFi: API 통신 (내장)
```

이렇게 각 프로토콜의 특성을 이해하고 적재적소에 활용하면 효율적인 시스템을 구축할 수 있습니다!
