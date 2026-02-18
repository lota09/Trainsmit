#ifndef SEOUL_DATA_H
#define SEOUL_DATA_H

#include <Arduino.h>

struct SubwayArrival {
  String type;  // 급행, 일반
  int min;
  int sec;
};

struct SubwayDir {
  String direction;  // 상행, 하행
  SubwayArrival arrivals[2];
  int count = 0;
};

struct SeoulData {
  String areaName;
  String curTemp;
  String minTemp;
  String maxTemp;
  String skyStatus;
  String pcpMsg;
  String pm10;
  String pm10Idx;
  String pm25;
  String pm25Idx;

  SubwayDir subway[2];  // 0: 상행, 1: 하행
  int avgCongestion;
  bool valid = false;
};

#endif
