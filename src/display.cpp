#include "display.h"

#include "config.h"

Display::Display() {
  {
    auto cfg = bus_.config();
    cfg.spi_host = SPI2_HOST;  // FSPI: el mismo periferico que usa la SD
    cfg.spi_mode = 0;
    cfg.freq_write = LCD_SPI_FREQ_HZ;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = PIN_SPI_SCK;
    cfg.pin_mosi = PIN_SPI_MOSI;
    cfg.pin_miso = PIN_SPI_MISO;
    cfg.pin_dc = PIN_LCD_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }
  {
    auto cfg = panel_.config();
    cfg.pin_cs = PIN_LCD_CS;
    cfg.pin_rst = PIN_LCD_RST;
    cfg.pin_busy = -1;
    // Tamano fisico; el giro a horizontal lo hace setRotation(LCD_ROTATION)
    cfg.panel_width = LCD_PANEL_WIDTH;
    cfg.panel_height = LCD_PANEL_HEIGHT;
    cfg.memory_width = LCD_PANEL_WIDTH;
    cfg.memory_height = LCD_MEMORY_HEIGHT;
    cfg.readable = false;
    cfg.bus_shared = true;
#ifdef WOKWI_BUILD
    // El ILI9341 simulado es 240x320 sin desplazamiento: se usan las 280 filas de arriba
    cfg.offset_y = 0;
    cfg.invert = false;
#else
    cfg.offset_y = LCD_OFFSET_Y;
    // Los ST7789 de este tipo vienen con los colores invertidos de fabrica
    cfg.invert = true;
#endif
    // Orientacion base del panel. En Wokwi la imagen no sale bien con ningun
    // valor: el ILI9341 simulado no implementa bien MADCTL (las 8 rotaciones
    // solo dan 4 resultados distintos y ninguno correcto). Es una limitacion
    // del simulador, no del codigo. En la placa real hay que ajustar este
    // valor y LCD_ROTATION (src/config.h) mirando la pantalla.
    cfg.offset_rotation = 0;

    panel_.config(cfg);
  }
  {
    auto cfg = light_.config();
    cfg.pin_bl = PIN_LCD_BLK;
    cfg.invert = false;
    cfg.freq = LCD_BL_PWM_FREQ_HZ;
    cfg.pwm_channel = LCD_BL_PWM_CHANNEL;
    light_.config(cfg);
    panel_.setLight(&light_);
  }
  setPanel(&panel_);
}

Display& display() {
  static Display instance;
  return instance;
}
