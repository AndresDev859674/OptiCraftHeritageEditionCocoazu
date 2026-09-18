#include "LegacyColorGradePass.h"
#include "LegacyColorGradePolicy.h"
#include "LegacyLook.h"

#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/Minecraft.h"
#include "net/minecraft/src/Tessellator.h"
#include "platform/PlatformConfig.h"
#include "platform/RenderAPI.h"

#if PLATFORM_PC
#include "pc/render/PcRenderBackend.h"
#include <glad/glad.h>
#include <SDL.h>
#include <cstdio>
#endif

namespace
{
#if PLATFORM_PC
int g_gradeTexture = 0;
int g_gradeTextureWidth = 0;
int g_gradeTextureHeight = 0;
GLuint g_gammaProgram = 0;
GLint g_gammaTextureUniform = -1;
GLint g_gammaExponentUniform = -1;
bool g_gammaInitAttempted = false;

// This project intentionally creates a legacy OpenGL context and its generated
// GLAD surface does not expose the OpenGL 2.0 shader entry points at compile
// time. Load only the optional GLSL functions needed by Legacy Look through
// SDL, so the normal OpenGL 1.1 renderer remains untouched.
#ifndef APIENTRY
#if defined(_WIN32)
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

constexpr GLenum LEGACY_GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum LEGACY_GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum LEGACY_GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum LEGACY_GL_LINK_STATUS = 0x8B82;

using LegacyCreateShaderProc = GLuint(APIENTRY *)(GLenum);
using LegacyShaderSourceProc = void(APIENTRY *)(GLuint, GLsizei, const char *const *, const GLint *);
using LegacyCompileShaderProc = void(APIENTRY *)(GLuint);
using LegacyGetShaderivProc = void(APIENTRY *)(GLuint, GLenum, GLint *);
using LegacyGetShaderInfoLogProc = void(APIENTRY *)(GLuint, GLsizei, GLsizei *, char *);
using LegacyDeleteShaderProc = void(APIENTRY *)(GLuint);
using LegacyCreateProgramProc = GLuint(APIENTRY *)(void);
using LegacyAttachShaderProc = void(APIENTRY *)(GLuint, GLuint);
using LegacyLinkProgramProc = void(APIENTRY *)(GLuint);
using LegacyGetProgramivProc = void(APIENTRY *)(GLuint, GLenum, GLint *);
using LegacyGetProgramInfoLogProc = void(APIENTRY *)(GLuint, GLsizei, GLsizei *, char *);
using LegacyDeleteProgramProc = void(APIENTRY *)(GLuint);
using LegacyUseProgramProc = void(APIENTRY *)(GLuint);
using LegacyGetUniformLocationProc = GLint(APIENTRY *)(GLuint, const char *);
using LegacyUniform1iProc = void(APIENTRY *)(GLint, GLint);
using LegacyUniform1fProc = void(APIENTRY *)(GLint, GLfloat);

struct LegacyShaderApi
{
    LegacyCreateShaderProc createShader = nullptr;
    LegacyShaderSourceProc shaderSource = nullptr;
    LegacyCompileShaderProc compileShader = nullptr;
    LegacyGetShaderivProc getShaderiv = nullptr;
    LegacyGetShaderInfoLogProc getShaderInfoLog = nullptr;
    LegacyDeleteShaderProc deleteShader = nullptr;
    LegacyCreateProgramProc createProgram = nullptr;
    LegacyAttachShaderProc attachShader = nullptr;
    LegacyLinkProgramProc linkProgram = nullptr;
    LegacyGetProgramivProc getProgramiv = nullptr;
    LegacyGetProgramInfoLogProc getProgramInfoLog = nullptr;
    LegacyDeleteProgramProc deleteProgram = nullptr;
    LegacyUseProgramProc useProgram = nullptr;
    LegacyGetUniformLocationProc getUniformLocation = nullptr;
    LegacyUniform1iProc uniform1i = nullptr;
    LegacyUniform1fProc uniform1f = nullptr;

    bool load()
    {
#define LOAD_LEGACY_GL(member, type, name) member = reinterpret_cast<type>(SDL_GL_GetProcAddress(name))
        LOAD_LEGACY_GL(createShader, LegacyCreateShaderProc, "glCreateShader");
        LOAD_LEGACY_GL(shaderSource, LegacyShaderSourceProc, "glShaderSource");
        LOAD_LEGACY_GL(compileShader, LegacyCompileShaderProc, "glCompileShader");
        LOAD_LEGACY_GL(getShaderiv, LegacyGetShaderivProc, "glGetShaderiv");
        LOAD_LEGACY_GL(getShaderInfoLog, LegacyGetShaderInfoLogProc, "glGetShaderInfoLog");
        LOAD_LEGACY_GL(deleteShader, LegacyDeleteShaderProc, "glDeleteShader");
        LOAD_LEGACY_GL(createProgram, LegacyCreateProgramProc, "glCreateProgram");
        LOAD_LEGACY_GL(attachShader, LegacyAttachShaderProc, "glAttachShader");
        LOAD_LEGACY_GL(linkProgram, LegacyLinkProgramProc, "glLinkProgram");
        LOAD_LEGACY_GL(getProgramiv, LegacyGetProgramivProc, "glGetProgramiv");
        LOAD_LEGACY_GL(getProgramInfoLog, LegacyGetProgramInfoLogProc, "glGetProgramInfoLog");
        LOAD_LEGACY_GL(deleteProgram, LegacyDeleteProgramProc, "glDeleteProgram");
        LOAD_LEGACY_GL(useProgram, LegacyUseProgramProc, "glUseProgram");
        LOAD_LEGACY_GL(getUniformLocation, LegacyGetUniformLocationProc, "glGetUniformLocation");
        LOAD_LEGACY_GL(uniform1i, LegacyUniform1iProc, "glUniform1i");
        LOAD_LEGACY_GL(uniform1f, LegacyUniform1fProc, "glUniform1f");
#undef LOAD_LEGACY_GL

        return createShader && shaderSource && compileShader && getShaderiv &&
               getShaderInfoLog && deleteShader && createProgram && attachShader &&
               linkProgram && getProgramiv && getProgramInfoLog && deleteProgram &&
               useProgram && getUniformLocation && uniform1i && uniform1f;
    }
};

LegacyShaderApi g_shaderApi;

int nextPowerOfTwo(int value)
{
    int result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

bool ensureGradeTexture(int width, int height)
{
    const int textureWidth = nextPowerOfTwo(width);
    const int textureHeight = nextPowerOfTwo(height);
    if (g_gradeTexture != 0 && g_gradeTextureWidth == textureWidth && g_gradeTextureHeight == textureHeight)
        return true;

    if (g_gradeTexture != 0)
        renderDeleteTextures(1, &g_gradeTexture);

    g_gradeTexture = 0;
    renderGenerateTextures(1, &g_gradeTexture);
    if (g_gradeTexture == 0)
        return false;

    g_gradeTextureWidth = textureWidth;
    g_gradeTextureHeight = textureHeight;
    renderBindTexture(g_gradeTexture);
    renderTextureImageRgba(0, textureWidth, textureHeight, nullptr);
    renderTextureParameters(true, false, true);
    return true;
}

void drawGradeTexture(float uMax, float vMax)
{
    Tessellator *tess = &Tessellator::instance;
    tess->startDrawingQuads();
    tess->addVertexWithUV(0.0, 0.0, 0.0, 0.0, 0.0);
    tess->addVertexWithUV(1.0, 0.0, 0.0, uMax, 0.0);
    tess->addVertexWithUV(1.0, 1.0, 0.0, uMax, vMax);
    tess->addVertexWithUV(0.0, 1.0, 0.0, 0.0, vMax);
    tess->draw();
}

bool checkShader(GLuint shader, const char *label)
{
    GLint ok = GL_FALSE;
    g_shaderApi.getShaderiv(shader, LEGACY_GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE)
        return true;

    char log[1024] = {};
    GLsizei length = 0;
    g_shaderApi.getShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log) - 1), &length, log);
    std::fprintf(stderr, "[LegacyLook] %s shader compile failed: %s\n", label, log);
    return false;
}

bool ensureGammaShader()
{
    if (g_gammaProgram != 0)
        return true;
    if (g_gammaInitAttempted)
        return false;
    g_gammaInitAttempted = true;

    if (!g_shaderApi.load())
    {
        std::fprintf(stderr, "[LegacyLook] GLSL is unavailable; framebuffer gamma pass disabled.\n");
        return false;
    }

    static const char *vertexSource =
        "#version 120\n"
        "void main()\n"
        "{\n"
        "    gl_Position = ftransform();\n"
        "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "}\n";

    // Matches Legacy4J's gamma operation: pow(diffuseColor.rgb, 1.0 / gamma).
    // OptiCraft stores the already-resolved exponent (0.8 for the current toggle).
    static const char *fragmentSource =
        "#version 120\n"
        "uniform sampler2D uTexture;\n"
        "uniform float uExponent;\n"
        "void main()\n"
        "{\n"
        "    vec4 color = texture2D(uTexture, gl_TexCoord[0].st);\n"
        "    color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(uExponent));\n"
        "    gl_FragColor = color;\n"
        "}\n";

    GLuint vertexShader = g_shaderApi.createShader(LEGACY_GL_VERTEX_SHADER);
    GLuint fragmentShader = g_shaderApi.createShader(LEGACY_GL_FRAGMENT_SHADER);
    if (vertexShader == 0 || fragmentShader == 0)
        return false;

    g_shaderApi.shaderSource(vertexShader, 1, &vertexSource, nullptr);
    g_shaderApi.compileShader(vertexShader);
    if (!checkShader(vertexShader, "vertex"))
    {
        g_shaderApi.deleteShader(vertexShader);
        g_shaderApi.deleteShader(fragmentShader);
        return false;
    }

    g_shaderApi.shaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    g_shaderApi.compileShader(fragmentShader);
    if (!checkShader(fragmentShader, "fragment"))
    {
        g_shaderApi.deleteShader(vertexShader);
        g_shaderApi.deleteShader(fragmentShader);
        return false;
    }

    GLuint program = g_shaderApi.createProgram();
    g_shaderApi.attachShader(program, vertexShader);
    g_shaderApi.attachShader(program, fragmentShader);
    g_shaderApi.linkProgram(program);
    g_shaderApi.deleteShader(vertexShader);
    g_shaderApi.deleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    g_shaderApi.getProgramiv(program, LEGACY_GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        char log[1024] = {};
        GLsizei length = 0;
        g_shaderApi.getProgramInfoLog(program, static_cast<GLsizei>(sizeof(log) - 1), &length, log);
        std::fprintf(stderr, "[LegacyLook] gamma program link failed: %s\n", log);
        g_shaderApi.deleteProgram(program);
        return false;
    }

    g_gammaTextureUniform = g_shaderApi.getUniformLocation(program, "uTexture");
    g_gammaExponentUniform = g_shaderApi.getUniformLocation(program, "uExponent");
    g_gammaProgram = program;
    std::fprintf(stderr, "[LegacyLook] Legacy4J framebuffer gamma shader enabled (exponent %.3f).\n",
        static_cast<double>(LEGACY_LOOK_GAMMA_EXPONENT));
    return true;
}
#endif
}

void legacyLookApplyWorldGrade(Minecraft *mc)
{
#if PLATFORM_WII
    if (mc != nullptr && mc->gameSettings != nullptr && mc->gameSettings->legacyLook)
        renderSetLegacyPresentationGamma(true);
#elif PLATFORM_PC
    if (pcRenderBackendIsDirect3D9())
        return;
    if (!legacyLookGradeFramebufferPassEnabled())
        return;
    if (mc == nullptr || mc->gameSettings == nullptr || !mc->gameSettings->legacyLook)
        return;
    if (mc->displayWidth <= 0 || mc->displayHeight <= 0)
        return;
    if (!ensureGammaShader())
        return;
    if (!ensureGradeTexture(mc->displayWidth, mc->displayHeight))
        return;

    renderBindTexture(g_gradeTexture);
    if (!renderCopyFramebufferToBoundTexture(0, 0, mc->displayWidth, mc->displayHeight))
        return;

    const float uMax = static_cast<float>(mc->displayWidth) / static_cast<float>(g_gradeTextureWidth);
    const float vMax = static_cast<float>(mc->displayHeight) / static_cast<float>(g_gradeTextureHeight);

    renderDisable(RenderCapability::DepthTest);
    renderDepthMask(false);
    renderDisable(RenderCapability::AlphaTest);
    renderDisable(RenderCapability::Fog);
    renderDisable(RenderCapability::Lighting);
    renderEnable(RenderCapability::Texture2D);
    renderDisable(RenderCapability::Blend);

    renderMatrixMode(RenderMatrixMode::Projection);
    renderPushMatrix();
    renderLoadIdentity();
    renderOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    renderMatrixMode(RenderMatrixMode::ModelView);
    renderPushMatrix();
    renderLoadIdentity();

    g_shaderApi.useProgram(g_gammaProgram);
    if (g_gammaTextureUniform >= 0)
        g_shaderApi.uniform1i(g_gammaTextureUniform, 0);
    if (g_gammaExponentUniform >= 0)
        g_shaderApi.uniform1f(g_gammaExponentUniform, LEGACY_LOOK_GAMMA_EXPONENT);

    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    drawGradeTexture(uMax, vMax);

    g_shaderApi.useProgram(0);

    renderMatrixMode(RenderMatrixMode::ModelView);
    renderPopMatrix();
    renderMatrixMode(RenderMatrixMode::Projection);
    renderPopMatrix();
    renderMatrixMode(RenderMatrixMode::ModelView);

    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    renderDepthMask(true);
    renderEnable(RenderCapability::AlphaTest);
    renderEnable(RenderCapability::DepthTest);
#else
    (void)mc;
#endif
}
