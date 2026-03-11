/*********************************************************************
This is a project-local patch of the Adafruit SHARP Memory Display library.

Original library:
  https://github.com/adafruit/Adafruit_SHARP_Memory_Display

This variant keeps the public API used by the firmware, but changes refresh
behavior so the display transport updates only dirty lines and performs a
VCOM-only keepalive when the framebuffer did not change.
*********************************************************************/
#ifndef LIB_ADAFRUIT_SHARPMEM
#define LIB_ADAFRUIT_SHARPMEM

#include <Adafruit_GFX.h>
#include <Adafruit_SPIDevice.h>
#include <Arduino.h>

#if defined(RAMSTART) && defined(RAMEND) && ((RAMEND - RAMSTART) < 4096)
#warning "Display may not work on devices with less than 4K RAM"
#endif

#define SHARPMEM_BIT_WRITECMD (0x01)
#define SHARPMEM_BIT_VCOM (0x02)
#define SHARPMEM_BIT_CLEAR (0x04)

class Adafruit_SharpMem : public Adafruit_GFX {
 public:
  Adafruit_SharpMem(uint8_t clk, uint8_t mosi, uint8_t cs, uint16_t w = 96,
                    uint16_t h = 96, uint32_t freq = 2000000);
  Adafruit_SharpMem(SPIClass* theSPI, uint8_t cs, uint16_t w = 96,
                    uint16_t h = 96, uint32_t freq = 2000000);
  boolean begin();
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  uint8_t getPixel(uint16_t x, uint16_t y);
  void clearDisplay();
  void refresh(void);
  void clearDisplayBuffer();
  uint8_t* getBuffer() { return sharpmem_buffer; }

 private:
  void markLineDirty(uint16_t y);
  void markAllLinesDirty();
  void toggleVcomOnly();

  Adafruit_SPIDevice* spidev = NULL;
  uint8_t* sharpmem_buffer = NULL;
  uint8_t* dirty_lines = NULL;
  uint16_t dirty_line_count = 0;
  uint8_t _cs;
  uint8_t _sharpmem_vcom;
};

#endif
