#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Convivencia con la MicroSD en el mismo bus (GP11/GP12/GP13):
// Con Arduino, LovyanGFX no crea un bus propio para SPI2_HOST: reutiliza el
// objeto global SPI (el mismo que usa SD) y en cada transaccion toma su mismo
// mutex del HAL (spiSimpleTransaction). Con bus_shared = true, ademas, vuelve
// a aplicar su reloj en cada transaccion, porque la SD lo habra cambiado.
// Resultado: los accesos de SD y pantalla quedan serializados sin mutex extra.
//
// Requisito: pin_miso debe ser GP13 aunque la pantalla no lea nada. Al
// arrancar, LovyanGFX hace SPI.end() + SPI.begin(sclk, miso, mosi); con
// miso = -1 desconectaria el MISO y la SD dejaria de poder leer.

class Display : public lgfx::LGFX_Device {
 public:
  Display();

 private:
#ifdef WOKWI_BUILD
  lgfx::Panel_ILI9341 panel_;
#else
  lgfx::Panel_ST7789 panel_;
#endif
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
};

Display& display();
