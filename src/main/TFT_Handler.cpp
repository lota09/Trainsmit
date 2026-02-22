#include "TFT_Handler.h"

LGFX_Config tft;

void initDisplay() {
  tft.init();
  tft.setRotation(5);  // 가로 모드 (480x320)
  tft.fillScreen(TFT_BLACK);
}

void updateDisplay(const SeoulData& data) {
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

  // 한국어 전용 폰트 설정
  tft.setFont(&lgfx::fonts::efontKR_16);
  tft.setTextSize(1);

  if (!data.valid) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(10, 140);
    tft.println("데이터를 불러오는 중입니다...");
    tft.endWrite();
    return;
  }

  // 1. 상단 바 (Header)
  tft.fillRect(0, 0, 480, 40, tft.color565(0, 80, 160));
  tft.setTextColor(TFT_WHITE);

  // 현재 시간/날짜 가져오기 (NTP)
  struct tm timeinfo;
  char dateStr[32];
  char timeStr[16];
  const char* wday_ko[] = {"일", "월", "화", "수", "목", "금", "토"};

  if (getLocalTime(&timeinfo)) {
    sprintf(dateStr, "%d월 %d일 %s요일", timeinfo.tm_mon + 1, timeinfo.tm_mday, wday_ko[timeinfo.tm_wday]);
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    strcpy(dateStr, "시간 로딩 중...");
    strcpy(timeStr, "00:00");
  }

  tft.setCursor(15, 12);
  tft.print(dateStr);

  // 시간은 우측 정렬 (약 420px 위치)
  tft.setCursor(415, 12);
  tft.print(timeStr);

  // 2. 날씨 및 미세먼지 정보 (통합 구역)
  int y = 55;
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);  // 32px
  tft.setCursor(15, y);
  tft.printf("%s°C", data.curTemp.c_str());

  tft.setTextSize(1);  // 16px
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(150, y + 10);
  tft.printf("(%s / %s)", data.minTemp.c_str(), data.maxTemp.c_str());

  y += 40;
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(15, y);
  tft.printf("날씨 %s | %s", data.skyStatus.c_str(), data.pcpMsg.c_str());

  // 미세먼지 (날씨 바로 아래)
  y += 25;
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(15, y);
  tft.printf("미세먼지 %s(%s)", data.pm10Idx.c_str(), data.pm10.c_str());
  tft.setCursor(240, y);
  tft.printf("초미세 %s(%s)", data.pm25Idx.c_str(), data.pm25.c_str());

  // 구분선
  y += 25;
  tft.drawFastHLine(10, y, 460, TFT_DARKGREY);

  // 3. 역 이름 배너 (9호선 골드 컬러 적용)
  y += 10;
  uint16_t line9_gold = tft.color565(189, 176, 146);
  tft.fillRoundRect(10, y, 460, 32, 5, line9_gold);
  tft.setTextColor(TFT_BLACK);  // 골드 배경엔 검은색 글자가 가독성이 좋음
  tft.setCursor(160, y + 8);

  tft.setFont(&lgfx::fonts::efontKR_16_b);
  tft.print("마곡나루역 9호선");
  tft.setFont(&lgfx::fonts::efontKR_16);


  // 4. 지하철 도착 정보
  y += 45;

  // 상행 (Left Column)
  int sub_y = y;
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(15, sub_y);

  tft.setFont(&lgfx::fonts::efontKR_16_b);
  tft.printf("▲ %s", data.subway[0].direction.c_str());
  tft.setFont(&lgfx::fonts::efontKR_16);

  sub_y += 25;

  tft.setTextColor(TFT_WHITE);
  if (data.subway[0].count == 0) {
    tft.setCursor(15, sub_y);
    tft.print("도착 정보 없음");
  } else {
    for (int j = 0; j < data.subway[0].count; j++) {
      tft.setCursor(15, sub_y);
      tft.printf("%s: %d분 %d초", data.subway[0].arrivals[j].type.c_str(), data.subway[0].arrivals[j].min, data.subway[0].arrivals[j].sec);
      sub_y += 25;
    }
  }

  // 하행 (Right Column - X좌표 240부터 시작)
  sub_y = y;
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(245, sub_y);

  tft.setFont(&lgfx::fonts::efontKR_16_b);
  tft.printf("▼ %s", data.subway[1].direction.c_str());
  tft.setFont(&lgfx::fonts::efontKR_16);

  sub_y += 25;

  tft.setTextColor(TFT_WHITE);
  if (data.subway[1].count == 0) {
    tft.setCursor(245, sub_y);
    tft.print("도착 정보 없음");
  } else {
    for (int j = 0; j < data.subway[1].count; j++) {
      tft.setCursor(245, sub_y);
      tft.printf("%s: %d분 %d초", data.subway[1].arrivals[j].type.c_str(), data.subway[1].arrivals[j].min, data.subway[1].arrivals[j].sec);
      sub_y += 25;
    }
  }

  // 5. 하단 역사 정보 (Footer)
  y = 290;
  tft.drawFastHLine(10, y, 460, TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setCursor(15, y + 8);
  tft.printf("역사 실시간 혼잡도: %d명 (10분 기준)", data.avgCongestion);

  tft.endWrite();
}
