#ifndef GFX_H
#define GFX_H

#include <cstdint>
#include <cstring>
#include <circle/types.h>
#include <circle/screen.h>

// ===== FONT STRUCTURES (Compatible with Adafruit GFX) =====

/// Font data stored PER GLYPH
typedef struct {
    uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
    uint8_t width;         ///< Bitmap dimensions in pixels
    uint8_t height;        ///< Bitmap dimensions in pixels
    uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
    int8_t xOffset;        ///< X dist from cursor pos to UL corner
    int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
    uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
    GFXglyph *glyph;  ///< Glyph array
    uint16_t first;   ///< ASCII extents (first char)
    uint16_t last;    ///< ASCII extents (last char)
    uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;

/**
 * @class CircleGFX
 * @brief Adafruit GFX compatible graphics library for circle-rpi
 *
 * This class provides a familiar Adafruit GFX-like API for drawing
 * graphics on a circle-rpi framebuffer.
 */
class CircleGFX {
public:
    /**
     * @brief Constructor
     * @param pScreen Pointer to CScreenDevice
     */
    CircleGFX(CScreenDevice *pScreen);

    /**
     * @brief Virtual destructor
     */
    virtual ~CircleGFX();

    // ===== CORE DRAW API =====

    /**
     * @brief Draw a single pixel
     * @param x X coordinate
     * @param y Y coordinate
     * @param color 16-bit RGB565 color
     */
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    /**
     * @brief Start a transaction (batch drawing)
     */
    void startWrite(void);

    /**
     * @brief End a transaction (flush if needed)
     */
    void endWrite(void);

    /**
     * @brief Write pixel during transaction
     * @param x X coordinate
     * @param y Y coordinate
     * @param color 16-bit RGB565 color
     */
    void writePixel(int16_t x, int16_t y, uint16_t color);

    /**
     * @brief Write filled rectangle during transaction
     * @param x X coordinate of top-left
     * @param y Y coordinate of top-left
     * @param w Width in pixels
     * @param h Height in pixels
     * @param color 16-bit RGB565 color
     */
    void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t color);

    /**
     * @brief Write vertical line during transaction
     * @param x X coordinate
     * @param y Starting Y coordinate
     * @param h Height in pixels
     * @param color 16-bit RGB565 color
     */
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

    /**
     * @brief Write horizontal line during transaction
     * @param x X coordinate
     * @param y Y coordinate
     * @param w Width in pixels
     * @param color 16-bit RGB565 color
     */
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

    /**
     * @brief Write line during transaction
     * @param x0 Starting X coordinate
     * @param y0 Starting Y coordinate
     * @param x1 Ending X coordinate
     * @param y1 Ending Y coordinate
     * @param color 16-bit RGB565 color
     */
    void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color);

    // ===== BASIC DRAW API =====

    /**
     * @brief Draw a vertical line
     * @param x X coordinate
     * @param y Starting Y coordinate
     * @param h Height in pixels
     * @param color 16-bit RGB565 color
     */
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

    /**
     * @brief Draw a horizontal line
     * @param x X coordinate
     * @param y Y coordinate
     * @param w Width in pixels
     * @param color 16-bit RGB565 color
     */
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

    /**
     * @brief Draw a line
     * @param x0 Starting X coordinate
     * @param y0 Starting Y coordinate
     * @param x1 Ending X coordinate
     * @param y1 Ending Y coordinate
     * @param color 16-bit RGB565 color
     */
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  uint16_t color);

    /**
     * @brief Draw an empty rectangle
     * @param x X coordinate of top-left
     * @param y Y coordinate of top-left
     * @param w Width in pixels
     * @param h Height in pixels
     * @param color 16-bit RGB565 color
     */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /**
     * @brief Fill a rectangle
     * @param x X coordinate of top-left
     * @param y Y coordinate of top-left
     * @param w Width in pixels
     * @param h Height in pixels
     * @param color 16-bit RGB565 color
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /**
     * @brief Fill entire screen
     * @param color 16-bit RGB565 color
     */
    void fillScreen(uint16_t color);

    /**
     * @brief Draw an empty circle
     * @param x0 Center X coordinate
     * @param y0 Center Y coordinate
     * @param r Radius in pixels
     * @param color 16-bit RGB565 color
     */
    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

    /**
     * @brief Fill a circle
     * @param x0 Center X coordinate
     * @param y0 Center Y coordinate
     * @param r Radius in pixels
     * @param color 16-bit RGB565 color
     */
    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

    /**
     * @brief Draw an empty rounded rectangle
     * @param x X coordinate of top-left
     * @param y Y coordinate of top-left
     * @param w Width in pixels
     * @param h Height in pixels
     * @param radius Corner radius in pixels
     * @param color 16-bit RGB565 color
     */
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                      int16_t radius, uint16_t color);

    /**
     * @brief Fill a rounded rectangle
     * @param x X coordinate of top-left
     * @param y Y coordinate of top-left
     * @param w Width in pixels
     * @param h Height in pixels
     * @param radius Corner radius in pixels
     * @param color 16-bit RGB565 color
     */
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color);

    /**
     * @brief Draw a triangle
     * @param x0 First point X coordinate
     * @param y0 First point Y coordinate
     * @param x1 Second point X coordinate
     * @param y1 Second point Y coordinate
     * @param x2 Third point X coordinate
     * @param y2 Third point Y coordinate
     * @param color 16-bit RGB565 color
     */
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);

    /**
     * @brief Fill a triangle
     * @param x0 First point X coordinate
     * @param y0 First point Y coordinate
     * @param x1 Second point X coordinate
     * @param y1 Second point Y coordinate
     * @param x2 Third point X coordinate
     * @param y2 Third point Y coordinate
     * @param color 16-bit RGB565 color
     */
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);

    // ===== TEXT API =====

    /**
     * @brief Set text cursor position
     * @param x X coordinate
     * @param y Y coordinate
     */
    void setCursor(int16_t x, int16_t y);

    /**
     * @brief Set text color (transparent background)
     * @param c 16-bit RGB565 text color
     */
    void setTextColor(uint16_t c);

    /**
     * @brief Set text color with background
     * @param c 16-bit RGB565 text color
     * @param bg 16-bit RGB565 background color
     */
    void setTextColor(uint16_t c, uint16_t bg);

    /**
     * @brief Set text size (uniform scaling)
     * @param s Text size multiplier (1 = default)
     */
    void setTextSize(uint8_t s);

    /**
     * @brief Set text size with independent X and Y scaling
     * @param sx Text X size multiplier
     * @param sy Text Y size multiplier
     */
    void setTextSize(uint8_t sx, uint8_t sy);

    /**
     * @brief Enable or disable text wrapping
     * @param w true for wrapping, false for clipping
     */
    void setTextWrap(bool w);

    /**
     * @brief Draw a character
     * @param x X coordinate
     * @param y Y coordinate
     * @param c Character to draw
     * @param color 16-bit RGB565 color
     * @param bg 16-bit RGB565 background color
     * @param size Character size multiplier
     */
    void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
                  uint16_t bg, uint8_t size);

    /**
     * @brief Draw a character with separate X/Y scaling
     * @param x X coordinate
     * @param y Y coordinate
     * @param c Character to draw
     * @param color 16-bit RGB565 color
     * @param bg 16-bit RGB565 background color
     * @param size_x Character X size multiplier
     * @param size_y Character Y size multiplier
     */
    void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
                  uint16_t bg, uint8_t size_x, uint8_t size_y);

    /**
     * @brief Write text string at current cursor position
     * @param text Text string to write
     * @note Uses current text color, size, and position
     */
    void writeText(const char *text);

    /**
     * @brief Set font for text rendering
     * @param f Pointer to GFXfont structure (NULL for default bitmap font)
     */
    void setFont(const GFXfont *f = 0);

    // ===== CONTROL API =====

    /**
     * @brief Set display rotation
     * @param r Rotation (0, 1, 2, or 3)
     */
    void setRotation(uint8_t r);

    /**
     * @brief Get current rotation
     * @return Rotation value (0, 1, 2, or 3)
     */
    uint8_t getRotation(void) const;

    /**
     * @brief Invert display colors
     * @param i true to invert, false for normal
     */
    void invertDisplay(bool i);

    // ===== DIMENSION API =====

    /**
     * @brief Get display width accounting for rotation
     * @return Width in pixels
     */
    int16_t width(void) const;

    /**
     * @brief Get display height accounting for rotation
     * @return Height in pixels
     */
    int16_t height(void) const;

    /**
     * @brief Get cursor X position
     * @return X coordinate in pixels
     */
    int16_t getCursorX(void) const;

    /**
     * @brief Get cursor Y position
     * @return Y coordinate in pixels
     */
    int16_t getCursorY(void) const;

    // ===== COLOR HELPER FUNCTIONS =====

    /**
     * @brief Convert RGB888 to RGB565
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @return 16-bit RGB565 color
     */
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Convert RGB888 to RGB565 from packed value
     * @param rgb 32-bit RGB888 value
     * @return 16-bit RGB565 color
     */
    static uint16_t color565(uint32_t rgb);

protected:
    /**
     * @brief Set pixel value in framebuffer memory
     * @param x X coordinate
     * @param y Y coordinate
     * @param color 16-bit RGB565 color
     */
    void setPixel(int16_t x, int16_t y, uint16_t color);

    /**
     * @brief Get pixel value from framebuffer memory
     * @param x X coordinate
     * @param y Y coordinate
     * @return 16-bit RGB565 color
     */
    uint16_t getPixel(int16_t x, int16_t y) const;

    // Helper for circle drawing (Bresenham)
    void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername,
                          uint16_t color);
    void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername,
                         int16_t delta, uint16_t color);

    // Helper for rounded rectangle
    void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color);

    CScreenDevice *m_pScreen;
    CBcmFrameBuffer *m_pFrameBuffer;

    int16_t m_width;
    int16_t m_height;
    uint32_t m_depth;
    uint32_t m_pitch;
    uint16_t *m_pBuffer;

    // Text settings
    int16_t m_cursorX;
    int16_t m_cursorY;
    uint16_t m_textColor;
    uint16_t m_textBgColor;
    uint8_t m_textSizeX;
    uint8_t m_textSizeY;
    boolean m_textWrap;
    uint8_t m_rotation;
    boolean m_inverted;
    boolean m_inTransaction;

    // Font support
    const GFXfont *m_pFont;  ///< Current font (NULL = default bitmap font)
    boolean m_fontSizeMultiplied;  ///< Whether to scale font with textSize
};

#endif // CIRCLE_GFX_H