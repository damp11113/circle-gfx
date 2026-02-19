#include "GFX.h"
#include <cstdint>
#include <cstdlib>
#include <circle/logger.h>

// Inline helper macros for bare metal environment (no std library)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SWAP(a, b) do { auto tmp = (a); (a) = (b); (b) = tmp; } while(0)
#define ABS(a) ((a) < 0 ? -(a) : (a))

LOGMODULE("CircleGFX");

// ===== CONSTRUCTOR & DESTRUCTOR =====

CircleGFX::CircleGFX(CScreenDevice *pScreen)
    : m_pScreen(pScreen), m_pFrameBuffer(nullptr), m_pBuffer(nullptr),
      m_cursorX(0), m_cursorY(0), m_textColor(0xFFFF), m_textBgColor(0x0000),
      m_textSizeX(1), m_textSizeY(1), m_textWrap(true), m_rotation(0),
      m_inverted(false), m_inTransaction(false), m_pFont(0),
      m_fontSizeMultiplied(true) {
    if (!m_pScreen) {
        //LOGE("Screen device is null");
        return;
    }

    m_pFrameBuffer = m_pScreen->GetFrameBuffer();
    if (!m_pFrameBuffer) {
        //LOGE("Failed to get framebuffer");
        return;
    }

    m_depth = m_pFrameBuffer->GetDepth();
    m_width = (int16_t)m_pFrameBuffer->GetWidth();
    m_height = (int16_t)m_pFrameBuffer->GetHeight();
    m_pitch = m_pFrameBuffer->GetPitch();
    m_pBuffer = (uint16_t *)m_pFrameBuffer->GetBuffer();

    //LOGI("CircleGFX initialized: %dx%d, depth=%d bits, pitch=%d", m_width, m_height, m_depth, m_pitch);
    return;
}

CircleGFX::~CircleGFX() {
    // Note: We don't delete framebuffer as it's managed by CScreenDevice
}

// ===== CORE DRAW API =====

void CircleGFX::startWrite(void) { m_inTransaction = true; }

void CircleGFX::endWrite(void) { m_inTransaction = false; }

void CircleGFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
    startWrite();
    writePixel(x, y, color);
    endWrite();
}

void CircleGFX::writePixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return;
    setPixel(x, y, color);
}

void CircleGFX::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color) {
    for (int16_t i = y; i < y + h; i++) {
        writeFastHLine(x, i, w, color);
    }
}

void CircleGFX::writeFastVLine(int16_t x, int16_t y, int16_t h,
                                uint16_t color) {
    for (int16_t i = y; i < y + h; i++) {
        writePixel(x, i, color);
    }
}

void CircleGFX::writeFastHLine(int16_t x, int16_t y, int16_t w,
                                uint16_t color) {
    if (y < 0 || y >= m_height)
        return;

    int16_t xStart = MAX(0, x);
    int16_t xEnd = MIN((int16_t)m_width, (int16_t)(x + w));

    for (int16_t i = xStart; i < xEnd; i++) {
        writePixel(i, y, color);
    }
}

void CircleGFX::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           uint16_t color) {
    // Bresenham's line algorithm
    int16_t dx = ABS(x1 - x0);
    int16_t dy = ABS(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    int16_t x = x0;
    int16_t y = y0;

    while (true) {
        writePixel(x, y, color);

        if (x == x1 && y == y1)
            break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// ===== BASIC DRAW API =====

void CircleGFX::drawFastVLine(int16_t x, int16_t y, int16_t h,
                               uint16_t color) {
    startWrite();
    writeFastVLine(x, y, h, color);
    endWrite();
}

void CircleGFX::drawFastHLine(int16_t x, int16_t y, int16_t w,
                               uint16_t color) {
    startWrite();
    writeFastHLine(x, y, w, color);
    endWrite();
}

void CircleGFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          uint16_t color) {
    startWrite();
    writeLine(x0, y0, x1, y1, color);
    endWrite();
}

void CircleGFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void CircleGFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t color) {
    startWrite();
    writeFillRect(x, y, w, h, color);
    endWrite();
}

void CircleGFX::fillScreen(uint16_t color) {
    fillRect(0, 0, m_width, m_height, color);
}

// ===== CIRCLE DRAWING =====

void CircleGFX::drawCircleHelper(int16_t x0, int16_t y0, int16_t r,
                                  uint8_t cornername, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x4) {
            writePixel(x0 + x, y0 + y, color);
            writePixel(x0 + y, y0 + x, color);
        }
        if (cornername & 0x2) {
            writePixel(x0 + x, y0 - y, color);
            writePixel(x0 + y, y0 - x, color);
        }
        if (cornername & 0x8) {
            writePixel(x0 - y, y0 + x, color);
            writePixel(x0 - x, y0 + y, color);
        }
        if (cornername & 0x1) {
            writePixel(x0 - y, y0 - x, color);
            writePixel(x0 - x, y0 - y, color);
        }
    }
}

void CircleGFX::fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
                                  uint8_t cornername, int16_t delta,
                                  uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x1) {
            writeFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
            writeFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
        }
        if (cornername & 0x2) {
            writeFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
            writeFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
        }
    }
}

void CircleGFX::drawCircle(int16_t x0, int16_t y0, int16_t r,
                            uint16_t color) {
    startWrite();
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    writePixel(x0, y0 + r, color);
    writePixel(x0, y0 - r, color);
    writePixel(x0 + r, y0, color);
    writePixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        writePixel(x0 + x, y0 + y, color);
        writePixel(x0 - x, y0 + y, color);
        writePixel(x0 + x, y0 - y, color);
        writePixel(x0 - x, y0 - y, color);
        writePixel(x0 + y, y0 + x, color);
        writePixel(x0 - y, y0 + x, color);
        writePixel(x0 + y, y0 - x, color);
        writePixel(x0 - y, y0 - x, color);
    }
    endWrite();
}

void CircleGFX::fillCircle(int16_t x0, int16_t y0, int16_t r,
                            uint16_t color) {
    startWrite();
    writeFastVLine(x0, y0 - r, 2 * r + 1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
    endWrite();
}

void CircleGFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               int16_t radius, uint16_t color) {
    startWrite();
    // Draw straight lines
    writeFastHLine(x + radius, y, w - 2 * radius, color); // Top
    writeFastHLine(x + radius, y + h - 1, w - 2 * radius, color); // Bottom
    writeFastVLine(x, y + radius, h - 2 * radius, color); // Left
    writeFastVLine(x + w - 1, y + radius, h - 2 * radius, color); // Right

    // Draw rounded corners
    drawCircleHelper(x + radius, y + radius, radius, 1, color);
    drawCircleHelper(x + w - radius - 1, y + radius, radius, 2, color);
    drawCircleHelper(x + w - radius - 1, y + h - radius - 1, radius, 4, color);
    drawCircleHelper(x + radius, y + h - radius - 1, radius, 8, color);
    endWrite();
}

void CircleGFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               int16_t radius, uint16_t color) {
    startWrite();
    // Draw three rectangles to fill
    writeFillRect(x + radius, y, w - 2 * radius, h, color);
    writeFillRect(x, y + radius, radius, h - 2 * radius, color);
    writeFillRect(x + w - radius, y + radius, radius, h - 2 * radius, color);

    // Fill corners with circle helpers
    fillCircleHelper(x + radius, y + radius, radius, 1, h - 2 * radius - 1,
                     color);
    fillCircleHelper(x + w - radius - 1, y + radius, radius, 2,
                     h - 2 * radius - 1, color);
    endWrite();
}

void CircleGFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2, uint16_t color) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void CircleGFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2, uint16_t color) {
    int16_t a, b, y, last;

    // Sort coordinates by Y
    if (y0 > y1) {
        SWAP(y0, y1);
        SWAP(x0, x1);
    }
    if (y1 > y2) {
        SWAP(y2, y1);
        SWAP(x2, x1);
    }
    if (y0 > y1) {
        SWAP(y0, y1);
        SWAP(x0, x1);
    }

    startWrite();
    if (y0 == y2) {
        a = b = x0;
        if (x1 < a)
            a = x1;
        else if (x1 > b)
            b = x1;
        if (x2 < a)
            a = x2;
        else if (x2 > b)
            b = x2;
        writeFastHLine(a, y0, b - a + 1, color);
        endWrite();
        return;
    }

    int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
            dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;

    if (y1 == y2)
        last = y1;
    else
        last = y1 - 1;

    for (y = y0; y <= last; y++) {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;

        if (a > b)
            SWAP(a, b);
        writeFastHLine(a, y, b - a + 1, color);
    }

    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;

        if (a > b)
            SWAP(a, b);
        writeFastHLine(a, y, b - a + 1, color);
    }
    endWrite();
}

// ===== TEXT API =====

void CircleGFX::setCursor(int16_t x, int16_t y) {
    m_cursorX = x;
    m_cursorY = y;
}

void CircleGFX::setTextColor(uint16_t c) {
    m_textColor = c;
    m_textBgColor = 0;
}

void CircleGFX::setTextColor(uint16_t c, uint16_t bg) {
    m_textColor = c;
    m_textBgColor = bg;
}

void CircleGFX::setTextSize(uint8_t s) {
    m_textSizeX = (s == 0) ? 1 : s;
    m_textSizeY = (s == 0) ? 1 : s;
}

void CircleGFX::setTextSize(uint8_t sx, uint8_t sy) {
    m_textSizeX = (sx == 0) ? 1 : sx;
    m_textSizeY = (sy == 0) ? 1 : sy;
}

void CircleGFX::setTextWrap(bool w) { m_textWrap = w; }

// ===== CLASSIC 5x8 BUILT-IN FONT =====
// Standard Adafruit GFX glcdfont — 5 bytes per character, 256 chars
// Each byte encodes one column (8 rows), LSB = top row
static const uint8_t font[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // (space)
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x41, 0x22, 0x14, 0x08, 0x00, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x01, 0x01, // F
    0x3E, 0x41, 0x41, 0x51, 0x32, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x04, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x7F, 0x20, 0x18, 0x20, 0x7F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x03, 0x04, 0x78, 0x04, 0x03, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x00, 0x7F, 0x41, 0x41, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // '\'
    0x41, 0x41, 0x7F, 0x00, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x01, 0x02, 0x04, 0x00, // `
    0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x38, 0x44, 0x44, 0x44, 0x20, // c
    0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x08, 0x14, 0x54, 0x54, 0x3C, // g
    0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x20, 0x40, 0x44, 0x3D, 0x00, // j
    0x00, 0x7F, 0x10, 0x28, 0x44, // k
    0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x7C, 0x04, 0x18, 0x04, 0x78, // m
    0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x38, 0x44, 0x44, 0x44, 0x38, // o
    0x7C, 0x14, 0x14, 0x14, 0x08, // p
    0x08, 0x14, 0x14, 0x18, 0x7C, // q
    0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x0C, 0x50, 0x50, 0x50, 0x3C, // y
    0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x08, 0x36, 0x41, 0x00, // {
    0x00, 0x00, 0x7F, 0x00, 0x00, // |
    0x00, 0x41, 0x36, 0x08, 0x00, // }
    0x08, 0x08, 0x2A, 0x1C, 0x08, // ->
    0x08, 0x1C, 0x2A, 0x08, 0x08, // <-
};

void CircleGFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
                          uint16_t bg, uint8_t size) {
    drawChar(x, y, c, color, bg, size, size);
}

void CircleGFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
                          uint16_t bg, uint8_t size_x, uint8_t size_y) {
    if (!m_pFont) {
        // ===== Classic built-in 5x8 font =====
        if ((x >= m_width)  ||
            (y >= m_height) ||
            ((x + 6 * size_x - 1) < 0) ||
            ((y + 8 * size_y - 1) < 0))
            return;

        // Only use chars 0x20–0x7F from the table above (offset by 0x20)
        if (c < 0x20 || c > 0x7F)
            c = '?';
        uint8_t charIndex = c - 0x20;

        startWrite();
        for (int8_t i = 0; i < 5; i++) {
            uint8_t line = font[charIndex * 5 + i];
            for (int8_t j = 0; j < 8; j++, line >>= 1) {
                if (line & 1) {
                    if (size_x == 1 && size_y == 1)
                        writePixel(x + i, y + j, color);
                    else
                        writeFillRect(x + i * size_x, y + j * size_y,
                                      size_x, size_y, color);
                } else if (bg != color) {
                    if (size_x == 1 && size_y == 1)
                        writePixel(x + i, y + j, bg);
                    else
                        writeFillRect(x + i * size_x, y + j * size_y,
                                      size_x, size_y, bg);
                }
            }
        }
        // Draw the 6th column (spacing) with background if opaque
        if (bg != color) {
            if (size_x == 1 && size_y == 1)
                writeFastVLine(x + 5, y, 8, bg);
            else
                writeFillRect(x + 5 * size_x, y, size_x, 8 * size_y, bg);
        }
        endWrite();
        // No cursor movement here — writeText() handles it

    } else {
        // ===== Custom GFXfont path =====
        // NOTE: drawChar() only DRAWS pixels. Cursor advancement and wrap
        // checking are the caller's responsibility (done in writeText/write).
        if (c < m_pFont->first || c > m_pFont->last)
            return;

        GFXglyph *glyph  = &m_pFont->glyph[c - m_pFont->first];
        uint8_t  *bitmap = m_pFont->bitmap;

        uint16_t bo  = glyph->bitmapOffset;
        uint8_t  w   = glyph->width;
        uint8_t  h   = glyph->height;
        int8_t   xo  = glyph->xOffset;
        int8_t   yo  = glyph->yOffset;
        int16_t  xo16 = xo, yo16 = yo;

        uint8_t bits = 0, bit = 0;

        startWrite();
        for (uint8_t yy = 0; yy < h; yy++) {
            for (uint8_t xx = 0; xx < w; xx++) {
                if (!(bit++ & 7))
                    bits = bitmap[bo++];

                if (bits & 0x80) {
                    if (size_x == 1 && size_y == 1) {
                        writePixel(x + xo + xx, y + yo + yy, color);
                    } else {
                        writeFillRect(x + (xo16 + xx) * size_x,
                                      y + (yo16 + yy) * size_y,
                                      size_x, size_y, color);
                    }
                }
                bits <<= 1;
            }
        }
        endWrite();
        // No cursor movement here — writeText() handles it
    }
}

void CircleGFX::writeText(const char *text) {
    if (!text)
        return;

    while (*text) {
        unsigned char c = (unsigned char)*text++;

        if (!m_pFont) {
            // ===== Classic font: write() logic =====
            if (c == '\n') {
                m_cursorX  = 0;
                m_cursorY += m_textSizeY * 8;
            } else if (c != '\r') {
                // Wrap before drawing if the character would go off-screen
                if (m_textWrap && ((m_cursorX + m_textSizeX * 6) > m_width)) {
                    m_cursorX  = 0;
                    m_cursorY += m_textSizeY * 8;
                }
                drawChar(m_cursorX, m_cursorY, c, m_textColor, m_textBgColor,
                         m_textSizeX, m_textSizeY);
                m_cursorX += m_textSizeX * 6; // Advance cursor (drawChar is pixel-draw only)
            }

        } else {
            // ===== Custom font: write() logic =====
            if (c == '\n') {
                m_cursorX  = 0;
                m_cursorY += (int16_t)m_textSizeY * m_pFont->yAdvance;
            } else if (c != '\r') {
                if (c >= m_pFont->first && c <= m_pFont->last) {
                    GFXglyph *glyph = &m_pFont->glyph[c - m_pFont->first];
                    uint8_t w  = glyph->width;
                    uint8_t h  = glyph->height;
                    if ((w > 0) && (h > 0)) { // Has a bitmap?
                        int8_t xo = glyph->xOffset;
                        // Wrap if glyph's drawn extent would exceed screen width
                        if (m_textWrap &&
                            ((m_cursorX + (int16_t)m_textSizeX * (xo + w)) > m_width)) {
                            m_cursorX  = 0;
                            m_cursorY += (int16_t)m_textSizeY * m_pFont->yAdvance;
                        }
                        drawChar(m_cursorX, m_cursorY, c, m_textColor, m_textBgColor,
                                 m_textSizeX, m_textSizeY);
                    }
                    // Always advance by xAdvance, even for whitespace glyphs
                    m_cursorX += (int16_t)m_textSizeX * glyph->xAdvance;
                }
            }
        }
    }
}

// ===== CONTROL API =====

void CircleGFX::setRotation(uint8_t r) {
    m_rotation = r % 4;
    // Update width/height if needed
    if (m_rotation == 1 || m_rotation == 3) {
        SWAP(m_width, m_height);
    }
}

uint8_t CircleGFX::getRotation(void) const { return m_rotation; }

void CircleGFX::invertDisplay(bool i) { m_inverted = i; }

// ===== DIMENSION API =====

int16_t CircleGFX::width(void) const { return m_width; }

int16_t CircleGFX::height(void) const { return m_height; }

int16_t CircleGFX::getCursorX(void) const { return m_cursorX; }

int16_t CircleGFX::getCursorY(void) const { return m_cursorY; }

// ===== COLOR HELPERS =====

uint16_t CircleGFX::color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t CircleGFX::color565(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    return color565(r, g, b);
}

// ===== PROTECTED HELPERS =====

void CircleGFX::setPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return;

    if (!m_pBuffer)
        return;

    // Calculate offset in the framebuffer
    uint32_t offset = y * (m_pitch / 2) + x; // pitch is in bytes, we have 16-bit pixels
    m_pBuffer[offset] = color;
}

uint16_t CircleGFX::getPixel(int16_t x, int16_t y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return 0;

    if (!m_pBuffer)
        return 0;

    uint32_t offset = y * (m_pitch / 2) + x;
    return m_pBuffer[offset];
}

void CircleGFX::drawFastVLineInternal(int16_t x, int16_t y, int16_t h,
                                       uint16_t color) {
    writeFastVLine(x, y, h, color);
}

void CircleGFX::drawFastHLineInternal(int16_t x, int16_t y, int16_t w,
                                       uint16_t color) {
    writeFastHLine(x, y, w, color);
}

// ===== FONT SUPPORT =====

void CircleGFX::setFont(const GFXfont *f) {
    m_pFont = f;
}
