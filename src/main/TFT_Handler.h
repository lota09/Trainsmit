#ifndef TFT_HANDLER_H
#define TFT_HANDLER_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "SeoulData.h"


// --- TFT LCD 설정 (LovyanGFX) ---
class LGFX_Config : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;
  lgfx::Panel_ILI9488 _panel_instance;

public:
  LGFX_Config() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.pin_sclk = 0;
      cfg.pin_mosi = 1;
      cfg.pin_miso = 2;
      cfg.pin_dc = 11;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 15;
      cfg.pin_rst = 21;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

extern LGFX_Config tft;

void initDisplay();
void updateDisplay(const SeoulData& data);

#endif
