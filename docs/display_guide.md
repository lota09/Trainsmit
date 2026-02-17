# 디스플레이 개발 가이드 - GPIO와 픽셀 렌더링 중심

## 1. LiquidCrystal.h와 SPI/I2C 프로토콜 연관성

### 기존 LCD (LiquidCrystal.h) 방식
```cpp
#include <LiquidCrystal.h>
// LiquidCrystal lcd(rs, enable, d4, d5, d6, d7);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);  // 6개 핀 사용
```

**특징:**
- **병렬 통신**: 데이터 핀 4개(D4~D7) + 제어 핀 2개(RS, Enable) = 총 6개 GPIO 핀 사용
- **문자 기반**: 미리 정의된 문자만 표시 가능 (16x2, 20x4 등)
- **제한적 제어**: 커서 위치와 간단한 문자열만 제어

### SPI/I2C 프로토콜로 전환 시 장점

#### I2C 방식 (2핀만 사용)
```cpp
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);  // 주소, 열, 행

// GPIO 연결
// SDA (데이터) - GPIO 21 (ESP32) 또는 A4 (Arduino)
// SCL (클럭)  - GPIO 22 (ESP32) 또는 A5 (Arduino)
```

#### SPI 방식 (4핀 사용)
```cpp
// SPI 핀 연결 (ESP32 기준)
// MOSI (데이터) - GPIO 23
// MISO (응답)   - GPIO 19  
// SCK (클럭)    - GPIO 18
// CS (선택)     - GPIO 5
```

**프로토콜 비교:**
- **병렬 (기존)**: 빠르지만 많은 핀 사용, 배선 복잡
- **I2C**: 2핀만 사용, 여러 장치 연결 가능, 속도 느림 (100kHz~400kHz)
- **SPI**: 4핀 사용, 빠른 속도 (몇 MHz), 단순한 프로토콜

## 2. TFT LCD/OLED vs LiquidCrystal 개발 차이점

### 근본적 차이점

| 구분 | LiquidCrystal | TFT LCD/OLED |
|------|---------------|--------------|
| **표시 방식** | 문자 기반 (Character) | 픽셀 기반 (Pixel) |
| **해상도** | 16x2, 20x4 등 고정 | 320x240, 480x320 등 자유 |
| **색상** | 단색 또는 백라이트 | 16bit~24bit 풀컬러 |
| **메모리** | 거의 불필요 | 프레임버퍼 필요 |
| **제어 복잡도** | 간단 (문자열만) | 복잡 (픽셀 단위 제어) |

### 개발 방식 변화

#### 기존 LCD 방식
```cpp
lcd.setCursor(0, 0);
lcd.print("Hello World");     // 간단한 텍스트 출력
lcd.setCursor(0, 1);
lcd.print("Temperature: 25C");
```

#### TFT/OLED 방식
```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

// 훨씬 복잡한 초기화
tft.init();
tft.setRotation(1);           // 화면 회전
tft.fillScreen(TFT_BLACK);    // 전체 화면 칠하기

// 픽셀 단위 제어
tft.drawPixel(x, y, color);   // 개별 픽셀 제어
tft.drawRect(x, y, w, h, color);
tft.fillRect(x, y, w, h, color);
tft.drawString("Hello", x, y, font);
```

### 메모리 관리의 중요성
```cpp
// TFT 디스플레이의 메모리 사용량 계산
// 320x240 해상도, 16bit 컬러 = 320 × 240 × 2 = 153,600 bytes (약 150KB)
// ESP32 RAM: 약 320KB이므로 절반 이상 사용!

// 메모리 절약 기법 필요
TFT_eSprite sprite = TFT_eSprite(&tft);  // 스프라이트 (부분 버퍼)
sprite.createSprite(100, 50);            // 작은 영역만 버퍼링
```

## 3. 사진 출력 및 텍스트 오버레이 구현

### GPIO 연결 (ESP32 + TFT 디스플레이 예시)
```cpp
// TFT_eSPI 라이브러리 설정 (User_Setup.h)
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15  // Chip select
#define TFT_DC    2  // Data Command
#define TFT_RST   4  // Reset (또는 -1로 미사용)

// SPI 통신 설정
#define SPI_FREQUENCY  27000000  // 27MHz (빠른 전송을 위해)
```

### 사진(이미지) 출력 방법

#### 1. 비트맵 배열로 저장된 이미지
```cpp
// 이미지를 C 배열로 변환 (온라인 컨버터 사용)
const uint16_t subway_icon[32*32] = {
  0x0000, 0x0000, 0x001F, 0x001F, // 16bit RGB565 포맷
  // ... 1024개의 픽셀 데이터
};

// 이미지 출력
void drawImage(int16_t x, int16_t y) {
  tft.pushImage(x, y, 32, 32, subway_icon);
}
```

#### 2. SD카드에서 이미지 로드
```cpp
#include <FS.h>
#include <SD.h>
#include <JPEGDecoder.h>

void drawJPEG(const char* filename, int x, int y) {
  File file = SD.open(filename);
  if (JpegDec.decodeFile(file)) {
    // JPEG 디코딩 후 TFT에 출력
    renderJPEG(x, y);
  }
  file.close();
}
```

### 텍스트 오버레이 구현

#### 기본 텍스트 출력
```cpp
void drawTextOverlay() {
  // 배경 이미지 먼저 출력
  tft.pushImage(0, 0, 320, 240, background_image);
  
  // 반투명 배경 박스 (텍스트 가독성 향상)
  tft.fillRect(10, 10, 300, 80, TFT_BLACK);
  tft.drawRect(10, 10, 300, 80, TFT_WHITE);
  
  // 텍스트 출력 (이미지 위에)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("2호선 강남역", 20, 20);
  tft.drawString("1분 후 도착", 20, 45);
  tft.drawString("다음: 3분 후", 20, 70);
}
```

#### 지하철 정보 표시 예시
```cpp
void displaySubwayInfo() {
  // 1. 배경 그리기
  tft.fillScreen(TFT_BLACK);
  
  // 2. 지하철 노선 색상 표시
  tft.fillRect(0, 0, 320, 50, TFT_GREEN);  // 2호선 = 녹색
  
  // 3. 역 이름 (큰 글씨)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.drawString("강남역", 10, 10);
  
  // 4. 도착 정보 (작은 글씨)
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("잠실 방면", 10, 60);
  tft.setTextColor(TFT_RED);
  tft.drawString("1분 후 도착", 10, 85);
  
  // 5. 아이콘 추가
  drawSubwayIcon(250, 60);  // 미리 준비한 지하철 아이콘
}
```

### 실시간 업데이트 최적화

#### 부분 업데이트로 깜빡임 방지
```cpp
void updateArrivalTime(int minutes) {
  // 시간 표시 영역만 지우고 다시 그리기
  tft.fillRect(10, 85, 200, 25, TFT_BLACK);  // 기존 텍스트 지우기
  
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.drawString(String(minutes) + "분 후 도착", 10, 85);
}
```

#### 스프라이트를 이용한 부드러운 애니메이션
```cpp
TFT_eSprite clockSprite = TFT_eSprite(&tft);

void updateClock() {
  // 오프스크린에서 시계 그리기
  clockSprite.createSprite(200, 100);
  clockSprite.fillSprite(TFT_BLACK);
  clockSprite.setTextColor(TFT_WHITE);
  clockSprite.drawString(getCurrentTime(), 10, 40);
  
  // 한 번에 화면에 출력 (깜빡임 없음)
  clockSprite.pushSprite(60, 70);
  clockSprite.deleteSprite();  // 메모리 해제
}
```

## 핵심 포인트 요약

1. **프로토콜 선택**: SPI가 I2C보다 빠르므로 대형 디스플레이에는 SPI 권장
2. **메모리 관리**: ESP32의 제한된 RAM 고려하여 스프라이트나 부분 업데이트 활용
3. **이미지 처리**: 미리 변환된 배열 사용이 가장 빠름, SD카드는 용량이 클 때만
4. **텍스트 오버레이**: 배경과 대비되는 색상과 반투명 박스로 가독성 확보
5. **실시간 업데이트**: 전체 화면 새로고침 대신 필요한 부분만 업데이트

이러한 기초 지식을 바탕으로 지하철 정보 표시와 시계 기능을 구현할 수 있습니다.
