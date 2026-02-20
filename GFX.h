#ifndef GFX_H
#define GFX_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <circle/types.h>

#define MAX(a,b)  ((a)>(b)?(a):(b))
#define MIN(a,b)  ((a)<(b)?(a):(b))
#define SWAP(a,b) do { auto _t=(a);(a)=(b);(b)=_t; } while(0)
#define ABS(a)    ((a)<0?-(a):(a))

// ─── Backend selection ────────────────────────────────────────────────────────
// Define GFX_USE_OPENGL_ES before including this header (or in your Makefile)
// to use the hardware-accelerated OpenGL ES 2.0 back-end via libgraphics.
// Without that define the original CScreenDevice / framebuffer back-end is used.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef GFX_USE_OPENGL_ES
  #include <graphics/eglrenderingcontext.h>   // libgraphics: CEglRenderingContext
  #include <GLES2/gl2.h>               // OpenGL ES 2.0 (from libgraphics/circle)
  #include <EGL/egl.h>
#else
  #include <circle/screen.h>
#endif

// ===== FONT STRUCTURES (Compatible with Adafruit GFX) ========================

/// Font data stored PER GLYPH
typedef struct {
    u16 bitmapOffset; ///< Pointer into GFXfont->bitmap
    u8  width;        ///< Bitmap dimensions in pixels
    u8  height;       ///< Bitmap dimensions in pixels
    u8  xAdvance;     ///< Distance to advance cursor (x axis)
    s8  xOffset;      ///< X dist from cursor pos to UL corner
    s8  yOffset;      ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
    u8  *bitmap;  ///< Glyph bitmaps, concatenated
    GFXglyph *glyph;   ///< Glyph array
    u16  first;   ///< ASCII extents (first char)
    u16  last;    ///< ASCII extents (last char)
    u8   yAdvance;///< Newline distance (y axis)
} GFXfont;

// ===== MULTI-BUFFER SUPPORT (Software Renderer Only) ==========================

/// Buffer index enumeration for easy reference
enum BufferIndex {
    BUFFER_0 = 0,
    BUFFER_1 = 1,
    BUFFER_2 = 2
};

/// Structure describing a single frame buffer
typedef struct {
    uint16_t *pData;      ///< Pointer to buffer data
    boolean   bOwned;     ///< Whether CircleGFX allocated this buffer
    boolean   bReady;     ///< Whether buffer is ready for display
} FrameBuffer;

/**
 * @class CircleGFX
 * @brief Adafruit GFX-compatible graphics library for Circle.
 *
 * Two back-ends are available, selected at compile time:
 *
 *   1. Framebuffer back-end (default)
 *      Uses CScreenDevice + CBcmFrameBuffer.  No extra libraries needed.
 *      Supports triple-buffering for smooth rendering without tearing.
 *
 *   2. OpenGL ES 2.0 back-end  (define GFX_USE_OPENGL_ES)
 *      Uses libgraphics (CEglRenderingContext) + the VideoCore GPU on the Pi.
 *      fillRect / fillScreen / drawRGBBitmap are GPU-accelerated.
 *      All other primitives still run on the CPU and call the same
 *      GL draw path so that the image stays consistent.
 */
class CircleGFX {
public:

    // ──────────────────────────────────────────────────────────────────────────
    // Constructors – one per back-end
    // ──────────────────────────────────────────────────────────────────────────

#ifdef GFX_USE_OPENGL_ES
    /**
     * @brief Constructor for the OpenGL ES 2.0 back-end.
     * @param pContext  Pointer to an already-initialised CEglRenderingContext.
     *                  Call pContext->Initialize() BEFORE constructing CircleGFX.
     */
    explicit CircleGFX(CEglRenderingContext *pContext);
#else
    /**
     * @brief Constructor for the framebuffer back-end.
     * @param pScreen Pointer to CScreenDevice (must already be initialised).
     */
    explicit CircleGFX(CScreenDevice *pScreen);
#endif

    virtual ~CircleGFX();

    // ===== CORE DRAW API =====================================================

    void drawPixel      (int16_t x, int16_t y, uint16_t color);
    void startWrite     (void);
    void endWrite       (void);
    void writePixel     (int16_t x, int16_t y, uint16_t color);
    void writeFillRect  (int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void writeFastVLine (int16_t x, int16_t y, int16_t h, uint16_t color);
    void writeFastHLine (int16_t x, int16_t y, int16_t w, uint16_t color);
    void writeLine      (int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

    // ===== BASIC DRAW API ====================================================

    void drawFastVLine  (int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawFastHLine  (int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawLine       (int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRect       (int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect       (int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillScreen     (uint16_t color);

    void drawCircle     (int16_t x0, int16_t y0, int16_t r, uint16_t color);
    void fillCircle     (int16_t x0, int16_t y0, int16_t r, uint16_t color);

    void drawRoundRect  (int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color);
    void fillRoundRect  (int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color);

    void drawTriangle   (int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, uint16_t color);
    void fillTriangle   (int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         int16_t x2, int16_t y2, uint16_t color);

    // ===== BITMAP DRAW API ===================================================

    void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                    int16_t w, int16_t h, uint16_t color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                    int16_t w, int16_t h, uint16_t color, uint16_t bg);
    void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                    int16_t w, int16_t h, uint16_t color);
    void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                    int16_t w, int16_t h, uint16_t color, uint16_t bg);

    void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                     int16_t w, int16_t h, uint16_t color);

    void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h);
    void drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap,        int16_t w, int16_t h);
    void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                             const uint8_t mask[], int16_t w, int16_t h);
    void drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                             uint8_t *mask, int16_t w, int16_t h);

    void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h);
    void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap,        int16_t w, int16_t h);
    void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[],
                       const uint8_t mask[], int16_t w, int16_t h);
    void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap,
                       uint8_t *mask, int16_t w, int16_t h);

    // ===== TEXT API ==========================================================

    void setCursor      (int16_t x, int16_t y);
    void setTextColor   (uint16_t c);
    void setTextColor   (uint16_t c, uint16_t bg);
    void setTextSize    (uint8_t s);
    void setTextSize    (uint8_t sx, uint8_t sy);
    void setTextWrap    (bool w);
    void drawChar       (int16_t x, int16_t y, unsigned char c,
                         uint16_t color, uint16_t bg, uint8_t size);
    void drawChar       (int16_t x, int16_t y, unsigned char c,
                         uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y);
    void writeText      (const char *text);
    void setFont        (const GFXfont *f = 0);

    // ===== CONTROL API =======================================================

    void    setRotation (uint8_t r);
    uint8_t getRotation (void) const;
    void    invertDisplay(bool i);

    // ===== DIMENSION API =====================================================

    int16_t width()    const;
    int16_t height()   const;
    int16_t getCursorX() const;
    int16_t getCursorY() const;

    // ===== COLOR HELPERS =====================================================

    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
    static uint16_t color565(uint32_t rgb);

    // ===== OPENGL ES SPECIFIC ================================================
#ifdef GFX_USE_OPENGL_ES
    /// Call once per frame after all drawing is done to swap EGL buffers.
    void swapBuffers();
#else
    // ===== MULTI-BUFFER API (Software Renderer Only) ==========================

    /**
     * @brief Enable multi-buffering mode (double or triple buffer).
     *        Must be called AFTER constructor but before drawing.
     *        Only available for software renderer (not OpenGL ES).
     * @param numBuffers Number of buffers (2 or 3). Default is 2 (double-buffer).
     * @return true if successful, false if allocation failed.
     */
    boolean enableMultiBuffer(uint8_t numBuffers = 2);

    /**
     * @brief Check if multi-buffering is enabled.
     * @return true if multi-buffering is active.
     */
    boolean isMultiBuffered() const;

    /**
     * @brief Get the number of buffers currently allocated.
     * @return Number of buffers (1, 2, or 3).
     */
    uint8_t getBufferCount() const;

    /**
     * @brief Get the index of the current drawing buffer.
     * @return Buffer index (0, 1, or 2).
     */
    uint8_t getDrawBufferIndex() const;

    /**
     * @brief Get the index of the currently displayed buffer.
     * @return Buffer index (0, 1, or 2).
     */
    uint8_t getDisplayBufferIndex() const;

    /**
     * @brief Swap to the next drawing buffer and update display.
     *        Should be called once per frame.
     *        Only affects software renderer.
     */
    void swapBuffers(boolean autoclear = true);

    /**
     * @brief Select which buffer to draw to (manual mode).
     *        Use this if you want explicit control instead of automatic swapping.
     * @param bufferIndex Buffer to select (0, 1, or 2).
     * @return true if successful, false if index out of range.
     */
    boolean selectDrawBuffer(uint8_t bufferIndex);

    /**
     * @brief Select which buffer to display (manual mode).
     *        Use this for explicit display buffer control.
     * @param bufferIndex Buffer to display (0, 1, or 2).
     * @return true if successful, false if index out of range.
     */
    boolean selectDisplayBuffer(uint8_t bufferIndex);

    /**
     * @brief Clear the specified buffer.
     * @param bufferIndex Buffer to clear (0, 1, 2, or -1 for all).
     * @param color Color to fill with (default black).
     */
    void clearBuffer(int8_t bufferIndex = -1, uint16_t color = 0);

    /**
     * @brief Get direct access to a buffer for low-level operations.
     * @param bufferIndex Buffer to access (0, 1, or 2).
     * @return Pointer to buffer data, or nullptr if invalid index.
     */
    uint16_t* getBuffer(uint8_t bufferIndex);

    /**
     * @brief Attach an external buffer for manual management.
     *        Useful for pre-allocated memory or external buffer sources.
     * @param bufferIndex Which buffer slot to use (0, 1, or 2).
     * @param pBuffer Pointer to external buffer (must be width*height*2 bytes).
     * @return true if successful.
     */
    boolean attachExternalBuffer(uint8_t bufferIndex, uint16_t *pBuffer);

    /**
     * @brief Detach an external buffer, allowing CircleGFX to clean up.
     * @param bufferIndex Which buffer to detach.
     * @return true if successful.
     */
    boolean detachExternalBuffer(uint8_t bufferIndex);

#endif

protected:

    // Internal pixel access
    void     setPixel (int16_t x, int16_t y, uint16_t color);
    uint16_t getPixel (int16_t x, int16_t y) const;

    // Circle / rounded-rect helpers
    void drawCircleHelper  (int16_t x0, int16_t y0, int16_t r,
                             uint8_t cornername, uint16_t color);
    void fillCircleHelper  (int16_t x0, int16_t y0, int16_t r,
                             uint8_t cornername, int16_t delta, uint16_t color);
    void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color);

    // ── Back-end specific members ────────────────────────────────────────────
#ifdef GFX_USE_OPENGL_ES
    CEglRenderingContext *m_pGLContext;   ///< libgraphics OpenGL ES context

    // GLSL program for flat-colour quads (fillRect / fillScreen)
    GLuint m_shaderFlat;
    GLuint m_uFlatColor;    ///< uniform location
    GLuint m_uFlatMVP;      ///< uniform location
    GLuint m_vboQuad;       ///< VBO for a unit quad

    // GLSL program for textured quads (drawRGBBitmap)
    GLuint m_shaderTex;
    GLuint m_uTexMVP;       ///< uniform location
    GLuint m_uTexSampler;   ///< uniform location

    // Scratch texture re-used for bitmap uploads
    GLuint m_scratchTex;
    int16_t m_scratchW, m_scratchH;

    // Private GL helpers
    GLuint  compileShader  (GLenum type, const char *src);
    GLuint  linkProgram    (GLuint vs, GLuint fs);
    void    initGLResources();
    void    drawGLRect     (int16_t x, int16_t y, int16_t w, int16_t h,
                            float r, float g, float b, float a);
    void    uploadAndDrawTex(int16_t x, int16_t y, int16_t w, int16_t h,
                             const uint16_t *pixels);

#else
    CScreenDevice   *m_pScreen;
    CBcmFrameBuffer *m_pFrameBuffer;
    uint32_t         m_depth;
    uint32_t         m_pitch;
    uint16_t        *m_pBuffer;

    // ── Multi-buffer members (Software Renderer) ─────────────────────────────
    FrameBuffer m_buffers[3];           ///< Up to 3 frame buffers
    uint8_t     m_bufferCount;          ///< Number of allocated buffers (1, 2, or 3)
    uint8_t     m_drawBufferIndex;      ///< Index of current drawing buffer
    uint8_t     m_displayBufferIndex;   ///< Index of currently displayed buffer
    boolean     m_multiBufferEnabled;   ///< Whether multi-buffering is active

    // Private multi-buffer helpers
    void _initializeMultiBuffer();
    void _cleanupMultiBuffer();
#endif

    // ── Common members ───────────────────────────────────────────────────────
    int16_t  m_width;
    int16_t  m_height;

    int16_t  m_cursorX, m_cursorY;
    uint16_t m_textColor, m_textBgColor;
    uint8_t  m_textSizeX, m_textSizeY;
    boolean  m_textWrap;
    uint8_t  m_rotation;
    boolean  m_inverted;
    boolean  m_inTransaction;

    const GFXfont *m_pFont;
    boolean        m_fontSizeMultiplied;
};

#endif // GFX_H