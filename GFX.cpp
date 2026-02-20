#include "GFX.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string.h>
#include <circle/logger.h>

LOGMODULE("CircleGFX");

// ═════════════════════════════════════════════════════════════════════════════
//  OPENGL ES 2.0 BACK-END
// ═════════════════════════════════════════════════════════════════════════════
#ifdef GFX_USE_OPENGL_ES

// ─── GL error checking helper ───────────────────────────────────────────────
static void checkGLError(const char *op) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        //LOGE("%s: GL error 0x%04X", op, err);
    }
}

// ─── Minimal GLSL shaders ───────────────────────────────────────────────────

// Flat-colour shader  (used by fillRect / fillScreen / all CPU-drawn primitives
// that go through setPixel in GL mode via a single-pixel quad)
static const char *s_flatVS =
    "attribute vec2 aPos;\n"
    "uniform mat4 uMVP;\n"
    "void main() { gl_Position = uMVP * vec4(aPos, 0.0, 1.0); }\n";

static const char *s_flatFS =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "void main() { gl_FragColor = uColor; }\n";

// Textured quad shader  (used by drawRGBBitmap)
static const char *s_texVS =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "uniform mat4 uMVP;\n"
    "varying vec2 vUV;\n"
    "void main() { vUV = aUV; gl_Position = uMVP * vec4(aPos, 0.0, 1.0); }\n";

static const char *s_texFS =
    "precision mediump float;\n"
    "uniform sampler2D uTex;\n"
    "varying vec2 vUV;\n"
    "void main() { gl_FragColor = texture2D(uTex, vUV); }\n";

// ─── ortho projection helper ─────────────────────────────────────────────────
// Builds a column-major 4×4 orthographic matrix that maps pixel coordinates
// (0,0) top-left → (width,height) bottom-right to NDC [-1..1].
static void buildOrtho(float *m, float w, float h) {
    // Column-major
    m[ 0]=2.f/w; m[ 1]=0;      m[ 2]=0; m[ 3]=0;
    m[ 4]=0;     m[ 5]=-2.f/h; m[ 6]=0; m[ 7]=0;
    m[ 8]=0;     m[ 9]=0;      m[10]=1; m[11]=0;
    m[12]=-1.f;  m[13]=1.f;    m[14]=0; m[15]=1;
}

// ─── Constructor ─────────────────────────────────────────────────────────────
CircleGFX::CircleGFX(CEglRenderingContext *pContext)
        : m_pGLContext(pContext),
        m_shaderFlat(0), m_uFlatColor(0), m_uFlatMVP(0), m_vboQuad(0),
        m_shaderTex(0),  m_uTexMVP(0),    m_uTexSampler(0),
        m_scratchTex(0), m_scratchW(0),   m_scratchH(0),
        m_width(0), m_height(0),
        m_cursorX(0), m_cursorY(0),
        m_textColor(0xFFFF), m_textBgColor(0x0000),
        m_textSizeX(1), m_textSizeY(1),
        m_textWrap(true), m_rotation(0),
        m_inverted(false), m_inTransaction(false),
        m_pFont(nullptr), m_fontSizeMultiplied(true) {
    if (!m_pGLContext) {
        //LOGE("OpenGL context is null");
        return;
    }

    // Ask libgraphics for the display dimensions
    m_width  = (int16_t)m_pGLContext->GetWidth();
    m_height = (int16_t)m_pGLContext->GetHeight();

    initGLResources();

    if (!m_shaderFlat || !m_shaderTex || !m_vboQuad) {
        //LOGE("Failed to initialize GL resources");
        return;
    }

    // Default GL state
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    checkGLError("GL state setup");

    //LOGI("CircleGFX (OpenGL ES) initialised: %dx%d", m_width, m_height);
}

CircleGFX::~CircleGFX() {
    if (m_scratchTex) glDeleteTextures(1, &m_scratchTex);
    if (m_vboQuad)    glDeleteBuffers(1, &m_vboQuad);
    if (m_shaderFlat) glDeleteProgram(m_shaderFlat);
    if (m_shaderTex)  glDeleteProgram(m_shaderTex);
}

// ─── GL resource initialisation ──────────────────────────────────────────────
GLuint CircleGFX::compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    if (!s) {
        //LOGE("Failed to create shader object");
        return 0;
    }
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char *log = (char *)malloc(len);
            glGetShaderInfoLog(s, len, nullptr, log);
            //LOGE("Shader compile error: %s", log);
            free(log);
        }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint CircleGFX::linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    if (!p) {
        //LOGE("Failed to create program object");
        return 0;
    }
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aUV");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char *log = (char *)malloc(len);
            glGetProgramInfoLog(p, len, nullptr, log);
            //LOGE("Program link error: %s", log);
            free(log);
        }
        glDeleteProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

void CircleGFX::initGLResources() {
    // ── Flat-colour program ──────────────────────────────────────────────────
    GLuint vs = compileShader(GL_VERTEX_SHADER,   s_flatVS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, s_flatFS);
    m_shaderFlat  = linkProgram(vs, fs);
    if (!m_shaderFlat) {
        //LOGE("Failed to create flat shader program");
        return;
    }
    m_uFlatColor  = glGetUniformLocation(m_shaderFlat, "uColor");
    m_uFlatMVP    = glGetUniformLocation(m_shaderFlat, "uMVP");

    // ── Textured-quad program ────────────────────────────────────────────────
    vs = compileShader(GL_VERTEX_SHADER,   s_texVS);
    fs = compileShader(GL_FRAGMENT_SHADER, s_texFS);
    m_shaderTex   = linkProgram(vs, fs);
    if (!m_shaderTex) {
        //LOGE("Failed to create texture shader program");
        return;
    }
    m_uTexMVP     = glGetUniformLocation(m_shaderTex, "uMVP");
    m_uTexSampler = glGetUniformLocation(m_shaderTex, "uTex");

    // ── Unit quad VBO (x,y,u,v) ──────────────────────────────────────────────
    // Two triangles forming a quad.  Actual positions are set per draw call
    // via the uniform MVP, so this is just a unit square [0..1].
    static const float kQuad[] = {
        // x    y    u    v
        0.f, 0.f,  0.f, 0.f,
        1.f, 0.f,  1.f, 0.f,
        0.f, 1.f,  0.f, 1.f,
        1.f, 1.f,  1.f, 1.f,
    };
    glGenBuffers(1, &m_vboQuad);
    if (!m_vboQuad) {
        ////LOGE("Failed to create VBO");
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ─── drawGLRect: GPU-accelerated solid-colour rectangle ──────────────────────
void CircleGFX::drawGLRect(int16_t x, int16_t y, int16_t w, int16_t h,
                            float r, float g, float b, float a) {
    if (w <= 0 || h <= 0 || !m_shaderFlat) return;

    // Build a scale-translate MVP:  scale(w,h) then translate(x,y) in ortho
    float ortho[16];
    buildOrtho(ortho, (float)m_width, (float)m_height);

    // We premultiply scale+translate into the ortho matrix.
    // ortho * T(x,y) * S(w,h)
    // Simpler: just pass x,y,w,h as uniforms and do it in the shader,
    // OR build the full matrix here. We'll build it manually:
    float mvp[16];
    // col-major: mvp = ortho * [w 0 0 x; 0 h 0 y; 0 0 1 0; 0 0 0 1]
    for (int i = 0; i < 4; i++) {
        mvp[i   ] = ortho[i   ]*w + ortho[i+12];  // col0 * w + col3 * x... simplified
        mvp[i+ 4] = ortho[i+ 4]*h + ortho[i+12];
        mvp[i+ 8] = ortho[i+ 8];
        mvp[i+12] = ortho[i   ]*x + ortho[i+ 4]*y + ortho[i+12];
    }
    // Rebuild properly (column-major mat × TRS):
    //   result_col[j] = sum_k( ortho[k*4+i] * TRS[j*4+k] )  for each row i
    // Easier to just build a model matrix and multiply:
    float model[16] = {
        (float)w, 0,        0, 0,
        0,        (float)h, 0, 0,
        0,        0,        1, 0,
        (float)x, (float)y, 0, 1
    };
    // MVP = ortho * model  (both col-major)
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            mvp[col*4+row] = 0;
            for (int k = 0; k < 4; k++)
                mvp[col*4+row] += ortho[k*4+row] * model[col*4+k];
        }
    }

    glUseProgram(m_shaderFlat);
    glUniformMatrix4fv(m_uFlatMVP, 1, GL_FALSE, mvp);
    glUniform4f(m_uFlatColor, r, g, b, a);

    glBindBuffer(GL_ARRAY_BUFFER, m_vboQuad);
    // aPos at location 0, stride = 4 floats, offset 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    // (no UV needed for flat shader)

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glFlush();
    checkGLError("drawGLRect");
}

// ─── uploadAndDrawTex: GPU-accelerated RGB565 bitmap blit ────────────────────
void CircleGFX::uploadAndDrawTex(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels) {
    if (!pixels || w <= 0 || h <= 0 || !m_shaderTex) return;

    // Create or reuse scratch texture (recreate if size differs)
    if (!m_scratchTex || m_scratchW != w || m_scratchH != h) {
        if (m_scratchTex) glDeleteTextures(1, &m_scratchTex);
        glGenTextures(1, &m_scratchTex);
        m_scratchW = w;
        m_scratchH = h;
    }
    glBindTexture(GL_TEXTURE_2D, m_scratchTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload RGB565 pixels
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);

    // Build MVP (same approach as drawGLRect)
    float ortho[16];
    buildOrtho(ortho, (float)m_width, (float)m_height);
    float model[16] = {
        (float)w, 0,        0, 0,
        0,        (float)h, 0, 0,
        0,        0,        1, 0,
        (float)x, (float)y, 0, 1
    };
    float mvp[16];
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++) {
            mvp[col*4+row] = 0;
            for (int k = 0; k < 4; k++)
                mvp[col*4+row] += ortho[k*4+row] * model[col*4+k];
        }

    glUseProgram(m_shaderTex);
    glUniformMatrix4fv(m_uTexMVP, 1, GL_FALSE, mvp);
    glUniform1i(m_uTexSampler, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_scratchTex);

    glBindBuffer(GL_ARRAY_BUFFER, m_vboQuad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glFlush();
    checkGLError("uploadAndDrawTex");
}

// ─── swapBuffers ──────────────────────────────────────────────────────────────
void CircleGFX::swapBuffers() {
    if (m_pGLContext)
        m_pGLContext->SwapBuffers();
}

// ─── setPixel / getPixel in GL mode ─────────────────────────────────────────
// Individual pixels are drawn as 1×1 quads.  This is slow, so all the
// bulk routines (fillRect etc.) are overridden below to avoid hitting setPixel.

// Helper: Convert RGB565 to normalized float colors
static void rgb565ToFloat(uint16_t color, float &r, float &g, float &b) {
    r = ((color >> 11) & 0x1F) / 31.f;
    g = ((color >>  5) & 0x3F) / 63.f;
    b =  (color        & 0x1F) / 31.f;
}

void CircleGFX::setPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    float r, g, b;
    rgb565ToFloat(color, r, g, b);
    drawGLRect(x, y, 1, 1, r, g, b, 1.f);
}

uint16_t CircleGFX::getPixel(int16_t x, int16_t y) const {
    // Reading back from GLES framebuffer is expensive; return 0 as a stub.
    (void)x; (void)y;
    return 0;
}

// ─── Accelerated overrides ───────────────────────────────────────────────────

void CircleGFX::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // Clamp to screen
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > m_width)  w = m_width  - x;
    if (y + h > m_height) h = m_height - y;
    if (w <= 0 || h <= 0) return;

    float r, g, b;
    rgb565ToFloat(color, r, g, b);
    drawGLRect(x, y, w, h, r, g, b, 1.f);
}

void CircleGFX::fillScreen(uint16_t color) {
    float r, g, b;
    rgb565ToFloat(color, r, g, b);
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    checkGLError("fillScreen");
    glFlush();
}

void CircleGFX::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
    uploadAndDrawTex(x, y, w, h, bitmap);
}

void CircleGFX::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
    uploadAndDrawTex(x, y, w, h, bitmap);
}

// ─── All remaining methods are identical to the framebuffer back-end ─────────
// (writeFastHLine / writeFastVLine still call writePixel → setPixel → GL quad,
//  which is fine for thin lines / text; fillRect above is the hot path.)

#else // ═══════════════════════════════════════════════════════════════════════
//  FRAMEBUFFER BACK-END  (original implementation)
// ════════════════════════════════════════════════════════════════════════════

CircleGFX::CircleGFX(CScreenDevice *pScreen)
        : m_pScreen(pScreen), m_pFrameBuffer(nullptr), m_pBuffer(nullptr),
        m_width(0), m_height(0),
        m_cursorX(0), m_cursorY(0),
        m_textColor(0xFFFF), m_textBgColor(0x0000),
        m_textSizeX(1), m_textSizeY(1),
        m_textWrap(true), m_rotation(0),
        m_inverted(false), m_inTransaction(false),
        m_pFont(nullptr), m_fontSizeMultiplied(true) {
    if (!m_pScreen) return;
    m_pFrameBuffer = m_pScreen->GetFrameBuffer();
    if (!m_pFrameBuffer) return;

    m_depth  = m_pFrameBuffer->GetDepth();
    m_width  = (int16_t)m_pFrameBuffer->GetWidth();
    m_height = (int16_t)m_pFrameBuffer->GetHeight();
    m_pitch  = m_pFrameBuffer->GetPitch();
    m_pBuffer= (uint16_t *)m_pFrameBuffer->GetBuffer();
}

CircleGFX::~CircleGFX() {}

void CircleGFX::setPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || !m_pBuffer) return;
    m_pBuffer[y * (m_pitch / 2) + x] = color;
}

uint16_t CircleGFX::getPixel(int16_t x, int16_t y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || !m_pBuffer) return 0;
    return m_pBuffer[y * (m_pitch / 2) + x];
}

void CircleGFX::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t i = y; i < y + h; i++)
        writeFastHLine(x, i, w, color);
}

void CircleGFX::fillScreen(uint16_t color) {
    fillRect(0, 0, m_width, m_height, color);
}

void CircleGFX::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
        for (int16_t i = 0; i < w; i++)
            writePixel(x + i, y, bitmap[j * w + i]);
    endWrite();
}

void CircleGFX::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
        for (int16_t i = 0; i < w; i++)
            writePixel(x + i, y, bitmap[j * w + i]);
    endWrite();
}

#endif // GFX_USE_OPENGL_ES

// ═════════════════════════════════════════════════════════════════════════════
//  COMMON CODE  (same for both back-ends)
// ═════════════════════════════════════════════════════════════════════════════

void CircleGFX::startWrite(void) { m_inTransaction = true;  }
void CircleGFX::endWrite  (void) { m_inTransaction = false; }

void CircleGFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
    startWrite();
    writePixel(x, y, color);
    endWrite();
}

void CircleGFX::writePixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    setPixel(x, y, color);
}

void CircleGFX::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    for (int16_t i = y; i < y + h; i++) writePixel(x, i, color);
}

void CircleGFX::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= m_height) return;
    int16_t xs = MAX(0, x);
    int16_t xe = MIN((int16_t)m_width, (int16_t)(x + w));
    for (int16_t i = xs; i < xe; i++) writePixel(i, y, color);
}

void CircleGFX::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = ABS(x1-x0), dy = ABS(y1-y0);
    int16_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int16_t err = dx - dy, x = x0, y = y0;
    while (true) {
        writePixel(x, y, color);
        if (x == x1 && y == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}

void CircleGFX::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    startWrite(); writeFastVLine(x, y, h, color); endWrite();
}
void CircleGFX::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    startWrite(); writeFastHLine(x, y, w, color); endWrite();
}
void CircleGFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    startWrite(); writeLine(x0, y0, x1, y1, color); endWrite();
}
void CircleGFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawFastHLine(x, y,         w, color);
    drawFastHLine(x, y+h-1,     w, color);
    drawFastVLine(x,     y, h,     color);
    drawFastVLine(x+w-1, y, h,     color);
}
void CircleGFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    startWrite(); writeFillRect(x, y, w, h, color); endWrite();
}

// ─── Circles ────────────────────────────────────────────────────────────────

void CircleGFX::drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t c, uint16_t color) {
    int16_t f=1-r, ddx=1, ddy=-2*r, x=0, y=r;
    while (x < y) {
        if (f >= 0) { y--; ddy+=2; f+=ddy; }
        x++; ddx+=2; f+=ddx;
        if (c&0x4) { writePixel(x0+x,y0+y,color); writePixel(x0+y,y0+x,color); }
        if (c&0x2) { writePixel(x0+x,y0-y,color); writePixel(x0+y,y0-x,color); }
        if (c&0x8) { writePixel(x0-y,y0+x,color); writePixel(x0-x,y0+y,color); }
        if (c&0x1) { writePixel(x0-y,y0-x,color); writePixel(x0-x,y0-y,color); }
    }
}

void CircleGFX::fillCircleHelper(int16_t x0, int16_t y0, int16_t r,uint8_t c, int16_t delta, uint16_t color) {
    int16_t f=1-r, ddx=1, ddy=-2*r, x=0, y=r;
    while (x < y) {
        if (f >= 0) { y--; ddy+=2; f+=ddy; }
        x++; ddx+=2; f+=ddx;
        if (c&0x1) {
            writeFastVLine(x0+x, y0-y, 2*y+1+delta, color);
            writeFastVLine(x0+y, y0-x, 2*x+1+delta, color);
        }
        if (c&0x2) {
            writeFastVLine(x0-x, y0-y, 2*y+1+delta, color);
            writeFastVLine(x0-y, y0-x, 2*x+1+delta, color);
        }
    }
}

void CircleGFX::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    startWrite();
    int16_t f=1-r, ddx=1, ddy=-2*r, x=0, y=r;
    writePixel(x0, y0+r, color); writePixel(x0, y0-r, color);
    writePixel(x0+r, y0, color); writePixel(x0-r, y0, color);
    while (x < y) {
        if (f >= 0) { y--; ddy+=2; f+=ddy; }
        x++; ddx+=2; f+=ddx;
        writePixel(x0+x,y0+y,color); writePixel(x0-x,y0+y,color);
        writePixel(x0+x,y0-y,color); writePixel(x0-x,y0-y,color);
        writePixel(x0+y,y0+x,color); writePixel(x0-y,y0+x,color);
        writePixel(x0+y,y0-x,color); writePixel(x0-y,y0-x,color);
    }
    endWrite();
}

void CircleGFX::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    startWrite();
    writeFastVLine(x0, y0-r, 2*r+1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
    endWrite();
}

// ─── Rounded rectangles ──────────────────────────────────────────────────────

void CircleGFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    startWrite();
    int16_t max_r = ((w < h) ? w : h) / 2;
    if (r > max_r) r = max_r;
    writeFastHLine(x+r,   y,       w-2*r, color);
    writeFastHLine(x+r,   y+h-1,   w-2*r, color);
    writeFastVLine(x,     y+r,     h-2*r, color);
    writeFastVLine(x+w-1, y+r,     h-2*r, color);
    drawCircleHelper(x+r,     y+r,     r, 1, color);
    drawCircleHelper(x+w-r-1, y+r,     r, 2, color);
    drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
    drawCircleHelper(x+r,     y+h-r-1, r, 8, color);
    endWrite();
}

void CircleGFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    startWrite();
    int16_t max_r = ((w < h) ? w : h) / 2;
    if (r > max_r) r = max_r;
    writeFillRect(x+r, y, w-2*r, h, color);
    fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
    fillCircleHelper(x+r,     y+r, r, 2, h-2*r-1, color);
    endWrite();
}

// ─── Triangles ───────────────────────────────────────────────────────────────

void CircleGFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    drawLine(x0,y0,x1,y1,color);
    drawLine(x1,y1,x2,y2,color);
    drawLine(x2,y2,x0,y0,color);
}

void CircleGFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    // Sort vertices by y
    if (y0 > y1) { SWAP(y0,y1); SWAP(x0,x1); }
    if (y1 > y2) { SWAP(y1,y2); SWAP(x1,x2); }
    if (y0 > y1) { SWAP(y0,y1); SWAP(x0,x1); }

    if (y0 == y2) {
        int16_t a = MIN(x0,MIN(x1,x2));
        int16_t b = MAX(x0,MAX(x1,x2));
        drawFastHLine(a, y0, b-a+1, color);
        return;
    }
    startWrite();
    int16_t dx01=x1-x0, dy01=y1-y0, dx02=x2-x0, dy02=y2-y0;
    int16_t dx12=x2-x1, dy12=y2-y1;
    int32_t sa=0, sb=0;
    int16_t last = (y1 == y2) ? y1 : y1-1;
    for (int16_t y=y0; y<=last; y++) {
        int16_t a = x0 + sa/dy01;
        int16_t b = x0 + sb/dy02;
        sa += dx01; sb += dx02;
        if (a > b) SWAP(a,b);
        writeFastHLine(a, y, b-a+1, color);
    }
    sa = (int32_t)dx12*(y1-y0) - (int32_t)dy12*(x1-x0);  // misuse sa as new accumulator
    sb = (int32_t)dx02*(y1-y0);
    // Reset sa for lower half
    sa = 0;
    for (int16_t y=y1; y<=y2; y++) {
        int16_t a = x1 + sa/dy12;
        int16_t b = x0 + sb/dy02;
        sa += dx12; sb += dx02;
        if (a > b) SWAP(a,b);
        writeFastHLine(a, y, b-a+1, color);
    }
    endWrite();
}

// ─── Bitmaps ─────────────────────────────────────────────────────────────────

void CircleGFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
    int16_t bw = (w+7)/8; uint8_t b = 0;
    startWrite();
    for (int16_t j=0; j<h; j++,y++)
        for (int16_t i=0; i<w; i++) {
            if (i&7) b<<=1; else b=bitmap[j*bw+i/8];
            if (b&0x80) writePixel(x+i,y,color);
        }
    endWrite();
}
void CircleGFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg) {
    int16_t bw=(w+7)/8; uint8_t b=0;
    startWrite();
    for (int16_t j=0; j<h; j++,y++)
        for (int16_t i=0; i<w; i++) {
            if (i&7) b<<=1; else b=bitmap[j*bw+i/8];
            writePixel(x+i,y,(b&0x80)?color:bg);
        }
    endWrite();
}
void CircleGFX::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) { 
    drawBitmap(x,y,(const uint8_t*)bitmap,w,h,color); 
}

void CircleGFX::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg) { 
    drawBitmap(x,y,(const uint8_t*)bitmap,w,h,color,bg); 
}

void CircleGFX::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[],  int16_t w, int16_t h, uint16_t color) {
    int16_t bw=(w+7)/8; uint8_t b=0;
    startWrite();
    for (int16_t j=0; j<h; j++,y++)
        for (int16_t i=0; i<w; i++) {
            if (i&7) b>>=1; else b=bitmap[j*bw+i/8];
            if (b&0x01) writePixel(x+i,y,color);
        }
    endWrite();
}

void CircleGFX::drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h) { startWrite(); for(int16_t j=0;j<h;j++,y++) for(int16_t i=0;i<w;i++) writePixel(x+i,y,(uint8_t)bitmap[j*w+i]); endWrite(); }
void CircleGFX::drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h) { 
    drawGrayscaleBitmap(x,y,(const uint8_t*)bitmap,w,h); 
}
void CircleGFX::drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], const uint8_t mask[], int16_t w, int16_t h) {
    int16_t bw=(w+7)/8; uint8_t b=0;
    startWrite();
    for(int16_t j=0;j<h;j++,y++) for(int16_t i=0;i<w;i++){
        if(i&7)b<<=1; else b=mask[j*bw+i/8];
        if(b&0x80) writePixel(x+i,y,(uint8_t)bitmap[j*w+i]);
    }
    endWrite();
}
void CircleGFX::drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap, uint8_t *mask, int16_t w, int16_t h) { 
    drawGrayscaleBitmap(x,y,(const uint8_t*)bitmap,(const uint8_t*)mask,w,h); 
}

void CircleGFX::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], const uint8_t mask[], int16_t w, int16_t h) {
    int16_t bw=(w+7)/8; uint8_t b=0;
    startWrite();
    for(int16_t j=0;j<h;j++,y++) for(int16_t i=0;i<w;i++){
        if(i&7)b<<=1; else b=mask[j*bw+i/8];
        if(b&0x80) writePixel(x+i,y,bitmap[j*w+i]);
    }
    endWrite();
}

void CircleGFX::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, uint8_t *mask, int16_t w, int16_t h) { 
    drawRGBBitmap(x,y,(const uint16_t*)bitmap,(const uint8_t*)mask,w,h); 
}

// ─── Text API ────────────────────────────────────────────────────────────────

// Default 5×8 font (Adafruit classic)
static const uint8_t s_font[] = {
    0x00,0x00,0x00,0x00,0x00, // ' '
    0x00,0x00,0x5F,0x00,0x00, // '!'
    0x00,0x07,0x00,0x07,0x00, // '"'
    0x14,0x7F,0x14,0x7F,0x14, // '#'
    0x24,0x2A,0x7F,0x2A,0x12, // '$'
    0x23,0x13,0x08,0x64,0x62, // '%'
    0x36,0x49,0x55,0x22,0x50, // '&'
    0x00,0x05,0x03,0x00,0x00, // '\''
    0x00,0x1C,0x22,0x41,0x00, // '('
    0x00,0x41,0x22,0x1C,0x00, // ')'
    0x14,0x08,0x3E,0x08,0x14, // '*'
    0x08,0x08,0x3E,0x08,0x08, // '+'
    0x00,0x50,0x30,0x00,0x00, // ','
    0x08,0x08,0x08,0x08,0x08, // '-'
    0x00,0x60,0x60,0x00,0x00, // '.'
    0x20,0x10,0x08,0x04,0x02, // '/'
    0x3E,0x51,0x49,0x45,0x3E, // '0'
    0x00,0x42,0x7F,0x40,0x00, // '1'
    0x42,0x61,0x51,0x49,0x46, // '2'
    0x21,0x41,0x45,0x4B,0x31, // '3'
    0x18,0x14,0x12,0x7F,0x10, // '4'
    0x27,0x45,0x45,0x45,0x39, // '5'
    0x3C,0x4A,0x49,0x49,0x30, // '6'
    0x01,0x71,0x09,0x05,0x03, // '7'
    0x36,0x49,0x49,0x49,0x36, // '8'
    0x06,0x49,0x49,0x29,0x1E, // '9'
    0x00,0x36,0x36,0x00,0x00, // ':'
    0x00,0x56,0x36,0x00,0x00, // ';'
    0x08,0x14,0x22,0x41,0x00, // '<'
    0x14,0x14,0x14,0x14,0x14, // '='
    0x00,0x41,0x22,0x14,0x08, // '>'
    0x02,0x01,0x51,0x09,0x06, // '?'
    0x32,0x49,0x79,0x41,0x3E, // '@'
    0x7E,0x11,0x11,0x11,0x7E, // 'A'
    0x7F,0x49,0x49,0x49,0x36, // 'B'
    0x3E,0x41,0x41,0x41,0x22, // 'C'
    0x7F,0x41,0x41,0x22,0x1C, // 'D'
    0x7F,0x49,0x49,0x49,0x41, // 'E'
    0x7F,0x09,0x09,0x09,0x01, // 'F'
    0x3E,0x41,0x49,0x49,0x7A, // 'G'
    0x7F,0x08,0x08,0x08,0x7F, // 'H'
    0x00,0x41,0x7F,0x41,0x00, // 'I'
    0x20,0x40,0x41,0x3F,0x01, // 'J'
    0x7F,0x08,0x14,0x22,0x41, // 'K'
    0x7F,0x40,0x40,0x40,0x40, // 'L'
    0x7F,0x02,0x0C,0x02,0x7F, // 'M'
    0x7F,0x04,0x08,0x10,0x7F, // 'N'
    0x3E,0x41,0x41,0x41,0x3E, // 'O'
    0x7F,0x09,0x09,0x09,0x06, // 'P'
    0x3E,0x41,0x51,0x21,0x5E, // 'Q'
    0x7F,0x09,0x19,0x29,0x46, // 'R'
    0x46,0x49,0x49,0x49,0x31, // 'S'
    0x01,0x01,0x7F,0x01,0x01, // 'T'
    0x3F,0x40,0x40,0x40,0x3F, // 'U'
    0x1F,0x20,0x40,0x20,0x1F, // 'V'
    0x3F,0x40,0x38,0x40,0x3F, // 'W'
    0x63,0x14,0x08,0x14,0x63, // 'X'
    0x07,0x08,0x70,0x08,0x07, // 'Y'
    0x61,0x51,0x49,0x45,0x43, // 'Z'
    0x00,0x7F,0x41,0x41,0x00, // '['
    0x02,0x04,0x08,0x10,0x20, // '\\'
    0x00,0x41,0x41,0x7F,0x00, // ']'
    0x04,0x02,0x01,0x02,0x04, // '^'
    0x40,0x40,0x40,0x40,0x40, // '_'
    0x00,0x01,0x02,0x04,0x00, // '`'
    0x20,0x54,0x54,0x54,0x78, // 'a'
    0x7F,0x48,0x44,0x44,0x38, // 'b'
    0x38,0x44,0x44,0x44,0x20, // 'c'
    0x38,0x44,0x44,0x48,0x7F, // 'd'
    0x38,0x54,0x54,0x54,0x18, // 'e'
    0x08,0x7E,0x09,0x01,0x02, // 'f'
    0x0C,0x52,0x52,0x52,0x3E, // 'g'
    0x7F,0x08,0x04,0x04,0x78, // 'h'
    0x00,0x44,0x7D,0x40,0x00, // 'i'
    0x20,0x40,0x44,0x3D,0x00, // 'j'
    0x7F,0x10,0x28,0x44,0x00, // 'k'
    0x00,0x41,0x7F,0x40,0x00, // 'l'
    0x7C,0x04,0x18,0x04,0x78, // 'm'
    0x7C,0x08,0x04,0x04,0x78, // 'n'
    0x38,0x44,0x44,0x44,0x38, // 'o'
    0x7C,0x14,0x14,0x14,0x08, // 'p'
    0x08,0x14,0x14,0x18,0x7C, // 'q'
    0x7C,0x08,0x04,0x04,0x08, // 'r'
    0x48,0x54,0x54,0x54,0x20, // 's'
    0x04,0x3F,0x44,0x40,0x20, // 't'
    0x3C,0x40,0x40,0x20,0x7C, // 'u'
    0x1C,0x20,0x40,0x20,0x1C, // 'v'
    0x3C,0x40,0x30,0x40,0x3C, // 'w'
    0x44,0x28,0x10,0x28,0x44, // 'x'
    0x0C,0x50,0x50,0x50,0x3C, // 'y'
    0x44,0x64,0x54,0x4C,0x44, // 'z'
    0x00,0x08,0x36,0x41,0x00, // '{'
    0x00,0x00,0x7F,0x00,0x00, // '|'
    0x00,0x41,0x36,0x08,0x00, // '}'
    0x10,0x08,0x08,0x10,0x08, // '~'
    0x78,0x46,0x41,0x46,0x78, // DEL
};

void CircleGFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y) {
    if (!m_pFont) {
        // Default 5×8 bitmap font
        if ((x >= m_width) || (y >= m_height) ||
            ((x + 6 * size_x - 1) < 0) || ((y + 8 * size_y - 1) < 0))
            return;
        if (c < 32 || c > 126) c = '?';
        const uint8_t *glyph = s_font + (c - 32) * 5;
        startWrite();
        for (int8_t col = 0; col < 5; col++) {
            uint8_t bits = glyph[col];
            for (int8_t row = 0; row < 8; row++, bits >>= 1) {
                uint16_t px = (bits & 1) ? color : bg;
                if (size_x == 1 && size_y == 1) {
                    writePixel(x + col, y + row, px);
                } else {
                    writeFillRect(x + col*size_x, y + row*size_y, size_x, size_y, px);
                }
            }
        }
        // Column 6 spacer
        for (int8_t row = 0; row < 8; row++) {
            if (size_x == 1 && size_y == 1) writePixel(x+5, y+row, bg);
            else writeFillRect(x+5*size_x, y+row*size_y, size_x, size_y, bg);
        }
        endWrite();
    } else {
        // Custom GFXfont
        if (c < m_pFont->first || c > m_pFont->last) return;
        uint8_t ci = c - m_pFont->first;
        const GFXglyph *glyph = &m_pFont->glyph[ci];
        const uint8_t  *bits  =  m_pFont->bitmap + glyph->bitmapOffset;
        int16_t gx = x + glyph->xOffset;
        int16_t gy = y + glyph->yOffset;
        int16_t gw = glyph->width, gh = glyph->height;
        uint8_t bit = 0, bits8 = 0;
        startWrite();
        for (int16_t gy2 = 0; gy2 < gh; gy2++) {
            for (int16_t gx2 = 0; gx2 < gw; gx2++) {
                if (!(bit++ & 7)) bits8 = *bits++;
                if (bits8 & 0x80) {
                    if (size_x == 1 && size_y == 1)
                        writePixel(gx+gx2, gy+gy2, color);
                    else
                        writeFillRect(gx+gx2*size_x, gy+gy2*size_y, size_x, size_y, color);
                }
                bits8 <<= 1;
            }
        }
        endWrite();
    }
}

void CircleGFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size) { 
    drawChar(x, y, c, color, bg, size, size); 
}

void CircleGFX::writeText(const char *text) {
    while (*text) {
        char c = *text++;
        if (c == '\n') {
            m_cursorX  = 0;
            m_cursorY += m_textSizeY * (m_pFont ? m_pFont->yAdvance : 8);
        } else if (c != '\r') {
            int16_t adv = m_pFont ? m_pFont->glyph[c - m_pFont->first].xAdvance : 6;
            if (m_textWrap && (m_cursorX + m_textSizeX * adv > m_width)) {
                m_cursorX  = 0;
                m_cursorY += m_textSizeY * (m_pFont ? m_pFont->yAdvance : 8);
            }
            drawChar(m_cursorX, m_cursorY, c, m_textColor, m_textBgColor, m_textSizeX, m_textSizeY);
            m_cursorX += m_textSizeX * adv;
        }
    }
}

void CircleGFX::setFont      (const GFXfont *f)  { m_pFont = f; }
void CircleGFX::setCursor    (int16_t x, int16_t y) { m_cursorX=x; m_cursorY=y; }
void CircleGFX::setTextColor (uint16_t c)            { m_textColor=c; m_textBgColor=c; }
void CircleGFX::setTextColor (uint16_t c, uint16_t bg){ m_textColor=c; m_textBgColor=bg; }
void CircleGFX::setTextSize  (uint8_t s)             { m_textSizeX=m_textSizeY=s?s:1; }
void CircleGFX::setTextSize  (uint8_t sx, uint8_t sy){ m_textSizeX=sx?sx:1; m_textSizeY=sy?sy:1; }
void CircleGFX::setTextWrap  (bool w)                { m_textWrap=w; }

// ─── Control ─────────────────────────────────────────────────────────────────

void CircleGFX::setRotation(uint8_t r) {
    m_rotation = r % 4;
    if (m_rotation == 1 || m_rotation == 3) SWAP(m_width, m_height);
}
uint8_t CircleGFX::getRotation() const { return m_rotation; }
void    CircleGFX::invertDisplay(bool i) { m_inverted = i; }

// ─── Dimensions ──────────────────────────────────────────────────────────────
int16_t CircleGFX::width()    const { return m_width; }
int16_t CircleGFX::height()   const { return m_height; }
int16_t CircleGFX::getCursorX() const { return m_cursorX; }
int16_t CircleGFX::getCursorY() const { return m_cursorY; }

// ─── Internal line helpers ────────────────────────────────────────────────────
void CircleGFX::drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t c)
{ writeFastVLine(x,y,h,c); }
void CircleGFX::drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t c)
{ writeFastHLine(x,y,w,c); }

// ─── Color helpers ────────────────────────────────────────────────────────────
uint16_t CircleGFX::color565(uint8_t r, uint8_t g, uint8_t b)
{ return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3); }
uint16_t CircleGFX::color565(uint32_t rgb)
{ return color565((rgb>>16)&0xFF,(rgb>>8)&0xFF,rgb&0xFF); }

#ifndef GFX_USE_OPENGL_ES

// ─────────────────────────────────────────────────────────────────────────
// Multi-Buffer Initialization and Management
// ─────────────────────────────────────────────────────────────────────────

boolean CircleGFX::enableMultiBuffer(uint8_t numBuffers)
{
    // Clamp to valid range (1-3 buffers)
    if (numBuffers < 1 || numBuffers > 3) {
        numBuffers = 2;  // Default to double-buffering
    }

    // Calculate buffer size in bytes
    size_t bufferSize = (size_t)m_width * (size_t)m_height * sizeof(uint16_t);

    // Free any existing allocated buffers
    for (uint8_t i = 0; i < m_bufferCount; i++) {
        if (m_buffers[i].bOwned && m_buffers[i].pData != nullptr) {
            free(m_buffers[i].pData);
            m_buffers[i].pData = nullptr;
            m_buffers[i].bOwned = false;
        }
    }

    // Allocate new buffers
    m_bufferCount = numBuffers;
    for (uint8_t i = 0; i < m_bufferCount; i++) {
        m_buffers[i].pData = (uint16_t *)malloc(bufferSize);
        
        if (m_buffers[i].pData == nullptr) {
            // Allocation failed - clean up and revert to single buffer
            for (uint8_t j = 0; j < i; j++) {
                free(m_buffers[j].pData);
                m_buffers[j].pData = nullptr;
            }
            m_bufferCount = 1;
            m_buffers[0].pData = m_pBuffer;  // Revert to original screen buffer
            m_buffers[0].bOwned = false;
            m_multiBufferEnabled = false;
            return false;
        }

        m_buffers[i].bOwned = true;
        m_buffers[i].bReady = false;

        // Initialize buffer to black
        memset(m_buffers[i].pData, 0, bufferSize);
    }

    m_drawBufferIndex = 0;
    m_displayBufferIndex = 0;
    m_multiBufferEnabled = true;
    m_pBuffer = m_buffers[0].pData;  // Point to first buffer for drawing

    return true;
}

boolean CircleGFX::isMultiBuffered() const
{
    return m_multiBufferEnabled;
}

uint8_t CircleGFX::getBufferCount() const
{
    return m_bufferCount;
}

uint8_t CircleGFX::getDrawBufferIndex() const
{
    return m_drawBufferIndex;
}

uint8_t CircleGFX::getDisplayBufferIndex() const
{
    return m_displayBufferIndex;
}

// ─────────────────────────────────────────────────────────────────────────
// Buffer Swapping and Selection
// ─────────────────────────────────────────────────────────────────────────

void CircleGFX::swapBuffers(boolean autoclear) {
    if (!m_multiBufferEnabled) {
        return;
    }

    // Mark current draw buffer as ready
    m_buffers[m_drawBufferIndex].bReady = true;

    // This buffer becomes visible
    m_displayBufferIndex = m_drawBufferIndex;

    // Copy to hardware framebuffer
    if (m_pFrameBuffer != nullptr) {
        uint32_t bufferSize = m_width * m_height * 2;
        memcpy((void*)m_pFrameBuffer->GetBuffer(),
               m_buffers[m_displayBufferIndex].pData,
               bufferSize);
    }

    // Advance draw buffer (round-robin)
    m_drawBufferIndex = (m_drawBufferIndex + 1) % m_bufferCount;

    if (autoclear) {
        uint32_t pixelCount = (uint32_t)m_width * (uint32_t)m_height;
        memset(m_buffers[m_drawBufferIndex].pData, 0, pixelCount * 2);
    }

    // Update pointer
    m_pBuffer = m_buffers[m_drawBufferIndex].pData;
}

boolean CircleGFX::selectDrawBuffer(uint8_t bufferIndex)
{
    if (!m_multiBufferEnabled || bufferIndex >= m_bufferCount) {
        return false;
    }

    m_drawBufferIndex = bufferIndex;
    m_pBuffer = m_buffers[m_drawBufferIndex].pData;
    return true;
}

boolean CircleGFX::selectDisplayBuffer(uint8_t bufferIndex)
{
    if (!m_multiBufferEnabled || bufferIndex >= m_bufferCount) {
        return false;
    }

    m_displayBufferIndex = bufferIndex;

    // Immediately copy to hardware framebuffer
    if (m_pFrameBuffer != nullptr) {
        uint32_t bufferSize = m_width * m_height * 2;  // 16-bit pixels
        memcpy((void*)m_pFrameBuffer->GetBuffer(), m_buffers[m_displayBufferIndex].pData, bufferSize);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// Buffer Clearing and Access
// ─────────────────────────────────────────────────────────────────────────

void CircleGFX::clearBuffer(int8_t bufferIndex, uint16_t color)
{
    uint32_t bufferSize = (uint32_t)m_width * (uint32_t)m_height;
    
    if (bufferIndex == -1) {
        // Clear all buffers
        for (uint8_t i = 0; i < m_bufferCount; i++) {
            if (m_buffers[i].pData != nullptr) {
                if (color == 0) {
                    memset(m_buffers[i].pData, 0, bufferSize * 2);
                } else {
                    // Fill with specific color
                    for (uint32_t j = 0; j < bufferSize; j++) {
                        m_buffers[i].pData[j] = color;
                    }
                }
            }
        }
    } else if (bufferIndex == -2) {
        // clear last buffer
        uint32_t pixelCount = (uint32_t)m_width * (uint32_t)m_height;
        memset(m_buffers[m_drawBufferIndex].pData, 0, pixelCount * 2);
    } else if (bufferIndex < m_bufferCount) {
        // Clear specific buffer
        if (m_buffers[bufferIndex].pData != nullptr) {
            if (color == 0) {
                memset(m_buffers[bufferIndex].pData, 0, bufferSize * 2);
            } else {
                for (uint32_t j = 0; j < bufferSize; j++) {
                    m_buffers[bufferIndex].pData[j] = color;
                }
            }
        }
    }
}

uint16_t* CircleGFX::getBuffer(uint8_t bufferIndex)
{
    if (bufferIndex >= m_bufferCount) {
        return nullptr;
    }
    return m_buffers[bufferIndex].pData;
}

// ─────────────────────────────────────────────────────────────────────────
// External Buffer Management
// ─────────────────────────────────────────────────────────────────────────

boolean CircleGFX::attachExternalBuffer(uint8_t bufferIndex, uint16_t *pBuffer)
{
    if (bufferIndex >= 3 || pBuffer == nullptr) {
        return false;
    }

    // Free any existing owned buffer at this index
    if (m_buffers[bufferIndex].bOwned && m_buffers[bufferIndex].pData != nullptr) {
        free(m_buffers[bufferIndex].pData);
    }

    // Attach the external buffer
    m_buffers[bufferIndex].pData = pBuffer;
    m_buffers[bufferIndex].bOwned = false;  // Not owned, don't free on cleanup
    m_buffers[bufferIndex].bReady = false;

    // Update buffer count if needed
    if (bufferIndex >= m_bufferCount) {
        m_bufferCount = bufferIndex + 1;
    }

    return true;
}

boolean CircleGFX::detachExternalBuffer(uint8_t bufferIndex)
{
    if (bufferIndex >= m_bufferCount) {
        return false;
    }

    if (!m_buffers[bufferIndex].bOwned) {
        m_buffers[bufferIndex].pData = nullptr;
        m_buffers[bufferIndex].bReady = false;
        return true;
    }

    return false;  // Can't detach an owned buffer
}

// ─────────────────────────────────────────────────────────────────────────
// Constructor/Destructor Updates
// ─────────────────────────────────────────────────────────────────────────

// Call this from the framebuffer-based constructor
// Call this from the framebuffer-based constructor
void CircleGFX::_initializeMultiBuffer()
{
    // Initialize all buffer structures
    for (uint8_t i = 0; i < 3; i++) {
        m_buffers[i].pData = nullptr;
        m_buffers[i].bOwned = false;
        m_buffers[i].bReady = false;
    }

    m_bufferCount = 1;
    m_drawBufferIndex = 0;
    m_displayBufferIndex = 0;
    m_multiBufferEnabled = false;

    // Set the main buffer pointer to the primary buffer
    m_buffers[0].pData = m_pBuffer;
    m_buffers[0].bOwned = false;
}

// Call this from the destructor
void CircleGFX::_cleanupMultiBuffer()
{
    // Free all owned buffers
    for (uint8_t i = 0; i < 3; i++) {
        if (m_buffers[i].bOwned && m_buffers[i].pData != nullptr) {
            free(m_buffers[i].pData);
            m_buffers[i].pData = nullptr;
        }
    }
    m_multiBufferEnabled = false;
}

#endif  // !GFX_USE_OPENGL_ES