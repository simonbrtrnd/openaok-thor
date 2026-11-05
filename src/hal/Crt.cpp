#include "hal/Crt.h"

#ifdef __ANDROID__
#include <GLES2/gl2.h>
#include <android/log.h>
#endif

namespace HAL::Crt {

namespace {
bool g_wanted = true;
} // namespace

void SetWanted(bool wanted) { g_wanted = wanted; }

#ifdef __ANDROID__

namespace {

// The attributes take the places SDL's renderer gives its own position and
// texture coordinates, which it sets afresh for each of its draws; its colour
// attribute, in between, keeps pointing into SDL's own buffer.
constexpr GLuint kPositionAttribute = 0;
constexpr GLuint kUvAttribute = 2;

constexpr const char* kVertexShader = R"(
attribute vec2 a_position;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER by Timothy Lottes, as libretro
// carries it (crt-lottes), for OpenGL ES 2: its default settings, the
// stretched VGA shadow mask, and no warp. u_swap is 1 for a frame whose red and
// blue lie the other way round in memory, as SDL's own shaders allow for.
constexpr const char* kFragmentShader = R"(
precision highp float;
uniform sampler2D u_frame;
uniform vec2 u_input;
uniform float u_swap;
varying vec2 v_uv;

const float hardScan = -8.0;
const float hardPix = -3.0;
const float maskDark = 0.5;
const float maskLight = 1.5;
const float brightBoost = 1.0;
const float hardBloomPix = -1.5;
const float hardBloomScan = -2.0;
const float bloomAmount = 0.15;
const float shape = 2.0;

float ToLinear1(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
vec3 ToLinear(vec3 c) { return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b)); }
float ToSrgb1(float c) { return c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055; }
vec3 ToSrgb(vec3 c) { return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b)); }

vec3 Fetch(vec2 pos, vec2 off) {
    pos = (floor(pos * u_input + off) + vec2(0.5)) / u_input;
    vec3 c = texture2D(u_frame, pos).rgb;
    return ToLinear(brightBoost * mix(c, c.bgr, u_swap));
}

vec2 Dist(vec2 pos) {
    pos = pos * u_input;
    return -((pos - floor(pos)) - vec2(0.5));
}

float Gaus(float pos, float scale) { return exp2(scale * pow(abs(pos), shape)); }

vec3 Horz3(vec2 pos, float off) {
    vec3 b = Fetch(pos, vec2(-1.0, off));
    vec3 c = Fetch(pos, vec2(0.0, off));
    vec3 d = Fetch(pos, vec2(1.0, off));
    float dst = Dist(pos).x;
    float wb = Gaus(dst - 1.0, hardPix);
    float wc = Gaus(dst + 0.0, hardPix);
    float wd = Gaus(dst + 1.0, hardPix);
    return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

vec3 Horz5(vec2 pos, float off) {
    vec3 a = Fetch(pos, vec2(-2.0, off));
    vec3 b = Fetch(pos, vec2(-1.0, off));
    vec3 c = Fetch(pos, vec2(0.0, off));
    vec3 d = Fetch(pos, vec2(1.0, off));
    vec3 e = Fetch(pos, vec2(2.0, off));
    float dst = Dist(pos).x;
    float wa = Gaus(dst - 2.0, hardPix);
    float wb = Gaus(dst - 1.0, hardPix);
    float wc = Gaus(dst + 0.0, hardPix);
    float wd = Gaus(dst + 1.0, hardPix);
    float we = Gaus(dst + 2.0, hardPix);
    return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

vec3 Horz7(vec2 pos, float off) {
    vec3 a = Fetch(pos, vec2(-3.0, off));
    vec3 b = Fetch(pos, vec2(-2.0, off));
    vec3 c = Fetch(pos, vec2(-1.0, off));
    vec3 d = Fetch(pos, vec2(0.0, off));
    vec3 e = Fetch(pos, vec2(1.0, off));
    vec3 f = Fetch(pos, vec2(2.0, off));
    vec3 g = Fetch(pos, vec2(3.0, off));
    float dst = Dist(pos).x;
    float wa = Gaus(dst - 3.0, hardBloomPix);
    float wb = Gaus(dst - 2.0, hardBloomPix);
    float wc = Gaus(dst - 1.0, hardBloomPix);
    float wd = Gaus(dst + 0.0, hardBloomPix);
    float we = Gaus(dst + 1.0, hardBloomPix);
    float wf = Gaus(dst + 2.0, hardBloomPix);
    float wg = Gaus(dst + 3.0, hardBloomPix);
    return (a * wa + b * wb + c * wc + d * wd + e * we + f * wf + g * wg) /
           (wa + wb + wc + wd + we + wf + wg);
}

float Scan(vec2 pos, float off) { return Gaus(Dist(pos).y + off, hardScan); }
float BloomScan(vec2 pos, float off) { return Gaus(Dist(pos).y + off, hardBloomScan); }

vec3 Tri(vec2 pos) {
    vec3 a = Horz3(pos, -1.0);
    vec3 b = Horz5(pos, 0.0);
    vec3 c = Horz3(pos, 1.0);
    return a * Scan(pos, -1.0) + b * Scan(pos, 0.0) + c * Scan(pos, 1.0);
}

vec3 Bloom(vec2 pos) {
    vec3 a = Horz5(pos, -2.0);
    vec3 b = Horz7(pos, -1.0);
    vec3 c = Horz7(pos, 0.0);
    vec3 d = Horz7(pos, 1.0);
    vec3 e = Horz5(pos, 2.0);
    return a * BloomScan(pos, -2.0) + b * BloomScan(pos, -1.0) + c * BloomScan(pos, 0.0) +
           d * BloomScan(pos, 1.0) + e * BloomScan(pos, 2.0);
}

vec3 Mask(vec2 pos) {
    pos.x += pos.y * 3.0;
    vec3 mask = vec3(maskDark);
    pos.x = fract(pos.x / 6.0);
    if (pos.x < 0.333) mask.r = maskLight;
    else if (pos.x < 0.666) mask.g = maskLight;
    else mask.b = maskLight;
    return mask;
}

void main() {
    vec3 colour = Tri(v_uv) + Bloom(v_uv) * bloomAmount;
    colour *= Mask(gl_FragCoord.xy * 1.000001);
    gl_FragColor = vec4(ToSrgb(colour), 1.0);
}
)";

// The whole viewport as a strip of two triangles: each corner's place, and
// the frame's point there, its top row first.
constexpr GLfloat kQuad[] = {
    -1.0f, 1.0f, 0.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 0.0f,
    1.0f, -1.0f, 1.0f, 1.0f,
};

struct Shader {
    bool failed = false;
    GLuint program = 0;
    GLuint quad = 0;
    GLint frame = -1;
    GLint input = -1;
    GLint swap = -1;
};
Shader g;

GLuint Compile(GLenum kind, const char* source) {
    const GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, "OpenAoK", "CRT shader: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// The program and its quad, made on first use and again should the context
// have been lost; false when the shader cannot be had.
bool Ready() {
    if (g.failed) return false;
    if (g.program != 0 && glIsProgram(g.program) == GL_TRUE) return true;
    const GLuint vertex = Compile(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = Compile(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertex == 0 || fragment == 0) {
        g.failed = true;
        return false;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, kPositionAttribute, "a_position");
    glBindAttribLocation(program, kUvAttribute, "a_uv");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(program);
        g.failed = true;
        return false;
    }
    g.program = program;
    g.frame = glGetUniformLocation(program, "u_frame");
    g.input = glGetUniformLocation(program, "u_input");
    g.swap = glGetUniformLocation(program, "u_swap");
    GLint bound = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound);
    glGenBuffers(1, &g.quad);
    glBindBuffer(GL_ARRAY_BUFFER, g.quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(bound));
    return true;
}

} // namespace

bool Present(SDL_Renderer* renderer, SDL_Texture* frame, int frame_w, int frame_h, const SDL_Rect& dest,
             int drawable_h) {
    if (!g_wanted) return false;
    Uint32 format = 0;
    if (SDL_QueryTexture(frame, &format, nullptr, nullptr, nullptr) != 0) return false;
    const bool swap = format == SDL_PIXELFORMAT_ARGB8888 || format == SDL_PIXELFORMAT_RGB888;
    SDL_RenderFlush(renderer);
    if (!Ready()) return false;

    // What SDL's renderer last set and expects to find again: it keeps track
    // of these rather than set them for every draw.
    GLint program = 0, buffer = 0;
    GLint viewport[4] = {};
    GLint uv_enabled = GL_FALSE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetVertexAttribiv(kUvAttribute, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &uv_enabled);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);

    if (SDL_GL_BindTexture(frame, nullptr, nullptr) != 0) return false;
    glUseProgram(g.program);
    glBindBuffer(GL_ARRAY_BUFFER, g.quad);
    glEnableVertexAttribArray(kPositionAttribute);
    glEnableVertexAttribArray(kUvAttribute);
    glVertexAttribPointer(kPositionAttribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(kUvAttribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glViewport(dest.x, drawable_h - dest.y - dest.h, dest.w, dest.h);
    glUniform1i(g.frame, 0);
    glUniform2f(g.input, static_cast<GLfloat>(frame_w), static_cast<GLfloat>(frame_h));
    glUniform1f(g.swap, swap ? 1.0f : 0.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_UnbindTexture(frame);

    glUseProgram(static_cast<GLuint>(program));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
    if (uv_enabled != GL_TRUE) glDisableVertexAttribArray(kUvAttribute);
    if (blend == GL_TRUE) glEnable(GL_BLEND);
    if (scissor == GL_TRUE) glEnable(GL_SCISSOR_TEST);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return true;
}

#else

bool Present(SDL_Renderer*, SDL_Texture*, int, int, const SDL_Rect&, int) { return false; }

#endif

} // namespace HAL::Crt
