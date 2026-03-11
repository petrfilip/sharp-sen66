/*********************************************************************
This is a project-local patch of the Adafruit SHARP Memory Display library.

Original library:
  https://github.com/adafruit/Adafruit_SHARP_Memory_Display
*********************************************************************/

#include "Adafruit_SharpMem.h"

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif
#ifndef _swap_uint16_t
#define _swap_uint16_t(a, b)                                                   \
  {                                                                            \
    uint16_t t = a;                                                            \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

#define TOGGLE_VCOM                                                            \
  do {                                                                         \
    _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM;                \
  } while (0)

Adafruit_SharpMem::Adafruit_SharpMem(uint8_t clk, uint8_t mosi, uint8_t cs,
                                     uint16_t width, uint16_t height,
                                     uint32_t freq)
    : Adafruit_GFX(width, height) {
  _cs = cs;
  if (spidev) {
    delete spidev;
  }
  spidev =
      new Adafruit_SPIDevice(cs, clk, -1, mosi, freq, SPI_BITORDER_LSBFIRST);
}

Adafruit_SharpMem::Adafruit_SharpMem(SPIClass* theSPI, uint8_t cs,
                                     uint16_t width, uint16_t height,
                                     uint32_t freq)
    : Adafruit_GFX(width, height) {
  _cs = cs;
  if (spidev) {
    delete spidev;
  }
  spidev = new Adafruit_SPIDevice(cs, freq, SPI_BITORDER_LSBFIRST, SPI_MODE0,
                                  theSPI);
}

boolean Adafruit_SharpMem::begin(void) {
  if (!spidev->begin()) {
    return false;
  }

  digitalWrite(_cs, LOW);
  _sharpmem_vcom = SHARPMEM_BIT_VCOM;

  const size_t bufferSize = (WIDTH * HEIGHT) / 8U;
  sharpmem_buffer = (uint8_t*)malloc(bufferSize);
  if (!sharpmem_buffer) {
    return false;
  }

  dirty_lines = (uint8_t*)malloc(HEIGHT);
  if (!dirty_lines) {
    free(sharpmem_buffer);
    sharpmem_buffer = NULL;
    return false;
  }

  setRotation(0);
  clearDisplayBuffer();
  return true;
}

static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,  (uint8_t)~2,  (uint8_t)~4,
                                      (uint8_t)~8,  (uint8_t)~16, (uint8_t)~32,
                                      (uint8_t)~64, (uint8_t)~128};

void Adafruit_SharpMem::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height) ||
      (sharpmem_buffer == NULL)) {
    return;
  }

  switch (rotation) {
    case 1:
      _swap_int16_t(x, y);
      x = WIDTH - 1 - x;
      break;
    case 2:
      x = WIDTH - 1 - x;
      y = HEIGHT - 1 - y;
      break;
    case 3:
      _swap_int16_t(x, y);
      y = HEIGHT - 1 - y;
      break;
  }

  if (color) {
    sharpmem_buffer[(y * WIDTH + x) / 8] |= pgm_read_byte(&set[x & 7]);
  } else {
    sharpmem_buffer[(y * WIDTH + x) / 8] &= pgm_read_byte(&clr[x & 7]);
  }
  markLineDirty(static_cast<uint16_t>(y));
}

uint8_t Adafruit_SharpMem::getPixel(uint16_t x, uint16_t y) {
  if ((x >= _width) || (y >= _height) || (sharpmem_buffer == NULL)) {
    return 0;
  }

  switch (rotation) {
    case 1:
      _swap_uint16_t(x, y);
      x = WIDTH - 1 - x;
      break;
    case 2:
      x = WIDTH - 1 - x;
      y = HEIGHT - 1 - y;
      break;
    case 3:
      _swap_uint16_t(x, y);
      y = HEIGHT - 1 - y;
      break;
  }

  return sharpmem_buffer[(y * WIDTH + x) / 8] & pgm_read_byte(&set[x & 7]) ? 1
                                                                           : 0;
}

void Adafruit_SharpMem::clearDisplay() { clearDisplayBuffer(); }

void Adafruit_SharpMem::refresh(void) {
  if (sharpmem_buffer == NULL) {
    return;
  }

  if ((dirty_lines != NULL) && (dirty_line_count == 0U)) {
    toggleVcomOnly();
    return;
  }

  const uint8_t bytesPerLine = WIDTH / 8U;

  spidev->beginTransaction();
  digitalWrite(_cs, HIGH);

  spidev->transfer(_sharpmem_vcom | SHARPMEM_BIT_WRITECMD);
  TOGGLE_VCOM;

  for (uint16_t lineIndex = 0; lineIndex < HEIGHT; ++lineIndex) {
    if ((dirty_lines != NULL) && (dirty_lines[lineIndex] == 0U)) {
      continue;
    }

    uint8_t line[bytesPerLine + 2U];
    line[0] = static_cast<uint8_t>(lineIndex + 1U);
    memcpy(line + 1, sharpmem_buffer + (static_cast<size_t>(lineIndex) * bytesPerLine),
           bytesPerLine);
    line[bytesPerLine + 1U] = 0x00;
    spidev->transfer(line, bytesPerLine + 2U);

    if (dirty_lines != NULL) {
      dirty_lines[lineIndex] = 0U;
    }
  }

  dirty_line_count = 0U;
  spidev->transfer(0x00);
  digitalWrite(_cs, LOW);
  spidev->endTransaction();
}

void Adafruit_SharpMem::clearDisplayBuffer() {
  if (sharpmem_buffer == NULL) {
    return;
  }

  memset(sharpmem_buffer, 0xff, (WIDTH * HEIGHT) / 8U);
  markAllLinesDirty();
}

void Adafruit_SharpMem::markLineDirty(const uint16_t y) {
  if ((dirty_lines == NULL) || (y >= HEIGHT) || (dirty_lines[y] != 0U)) {
    return;
  }

  dirty_lines[y] = 1U;
  ++dirty_line_count;
}

void Adafruit_SharpMem::markAllLinesDirty() {
  if (dirty_lines == NULL) {
    return;
  }

  memset(dirty_lines, 1, HEIGHT);
  dirty_line_count = HEIGHT;
}

void Adafruit_SharpMem::toggleVcomOnly() {
  spidev->beginTransaction();
  digitalWrite(_cs, HIGH);

  uint8_t keepAlive[2] = {_sharpmem_vcom, 0x00};
  spidev->transfer(keepAlive, 2);
  TOGGLE_VCOM;

  digitalWrite(_cs, LOW);
  spidev->endTransaction();
}
