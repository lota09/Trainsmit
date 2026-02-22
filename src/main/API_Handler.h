#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <WiFi.h>

#include "SeoulData.h"


// 함수 선언
String urlEncode(String str);
SeoulData fetchSeoulData();

#endif
