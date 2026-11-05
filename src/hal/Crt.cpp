#include "hal/Crt.h"

#include <algorithm>

#ifdef __ANDROID__
#include <GLES2/gl2.h>
#include <android/log.h>
#endif

namespace HAL::Crt {

namespace {

// Each mode's monitor: how many screen pixels tall its scan lines are, how
// hard their edges and its pixels' are (crt-lottes's hardScan and hardPix),
// its shadow mask (0 none, 1 an aperture grille, 2 a stretched VGA mask), the
// mask's size in screen pixels, how dark and light its stripes are, its bloom,
// and how much brighter it is drawn, for what the lines and the mask darken.
struct Monitor {
    const char* name;
    float pixels_per_line;
    float hard_scan;
    float hard_pix;
    float mask_kind;
    float mask_size;
    float mask_dark;
    float mask_light;
    float bloom;
    float boost;
};
constexpr Monitor kMonitors[kModeCount] = {
    {"Off", 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
    {"Soft", 1.5f, -8.0f, -3.0f, 2.0f, 1.0f, 0.75f, 1.25f, 0.10f, 1.1f},
    {"Arcade", 2.0f, -10.0f, -3.0f, 1.0f, 1.0f, 0.75f, 1.25f, 0.15f, 1.3f},
    {"TV", 3.0f, -12.0f, -2.0f, 2.0f, 2.0f, 0.55f, 1.45f, 0.25f, 1.45f},
};

// Where the scan lines fall: with an even number of screen pixels to a line,
// they are moved a half pixel so that one pixel lies on a line's middle and
// one between two lines, else both would be lit alike and no line would show.
float LineShift(float pixels_per_line) {
    const int whole = static_cast<int>(pixels_per_line);
    return whole == pixels_per_line && whole % 2 == 0 ? 0.5f / pixels_per_line : 0.0f;
}

struct Setting {
    int mode = kArcade;
    int intensity = 100;
    Uint32 changed_ms = 0;
    bool unsaved = false;
};
Setting g_setting;

void Changed() {
    g_setting.changed_ms = SDL_GetTicks();
    if (g_setting.changed_ms == 0) g_setting.changed_ms = 1;
    g_setting.unsaved = true;
}

} // namespace

void Set(int mode, int intensity_percent) {
    g_setting.mode = std::clamp(mode, 0, kModeCount - 1);
    g_setting.intensity = std::clamp(intensity_percent, 0, 100);
}

int CurrentMode() { return g_setting.mode; }
int Intensity() { return g_setting.intensity; }

const char* ModeName(int mode) { return kMonitors[std::clamp(mode, 0, kModeCount - 1)].name; }

void StepMode(int by) {
    g_setting.mode = ((g_setting.mode + by) % kModeCount + kModeCount) % kModeCount;
    Changed();
}

void StepIntensity(int by) {
    g_setting.intensity = std::clamp(g_setting.intensity + by * kIntensityStep, kIntensityStep, 100);
    Changed();
}

Uint32 ChangedMs() { return g_setting.changed_ms; }

bool TakeChange() {
    const bool unsaved = g_setting.unsaved;
    g_setting.unsaved = false;
    return unsaved;
}

#ifdef __ANDROID__

namespace {

// The attributes take the places SDL's renderer gives its own position and
// texture coordinates, which it sets afresh for each of its draws; its colour
// attribute, in between, keeps pointing into SDL's own buffer.
constexpr GLuint kPositionAttribute = 0;
constexpr GLuint kUvAttribute = 2;

// u_flip is 1 when drawing into a render target, whose rows SDL keeps top
// first.
constexpr const char* kVertexShader = R"(
attribute vec2 a_position;
attribute vec2 a_uv;
uniform float u_flip;
varying vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_position.x, mix(a_position.y, -a_position.y, u_flip), 0.0, 1.0);
}
)";

// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER by Timothy Lottes, as libretro
// carries it (crt-lottes), for OpenGL ES 2, with no warp. Its settings come
// from the mode (Monitor), and it samples the picture at the monitor's own
// pixels, u_lines across and down, rather than at the picture's. The result
// is mixed with the plain picture by u_intensity. u_swap is 1 for a picture
// whose red and blue lie the other way round in memory, as SDL's own shaders
// allow for.
constexpr const char* kFragmentShader = R"(
precision highp float;
uniform sampler2D u_frame;
uniform vec2 u_lines;
uniform float u_swap;
uniform float u_hard_scan;
uniform float u_hard_pix;
uniform float u_mask_kind;
uniform float u_mask_size;
uniform float u_mask_dark;
uniform float u_mask_light;
uniform float u_bloom;
uniform float u_boost;
uniform float u_line_shift;
uniform float u_intensity;
varying vec2 v_uv;

const float hardBloomPix = -1.5;
const float hardBloomScan = -2.0;
const float shape = 2.0;

float ToLinear1(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
vec3 ToLinear(vec3 c) { return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b)); }
float ToSrgb1(float c) { return c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055; }
vec3 ToSrgb(vec3 c) { return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b)); }

vec3 Sample(vec2 pos) {
    vec3 c = texture2D(u_frame, pos).rgb;
    return ToLinear(mix(c, c.bgr, u_swap));
}

vec3 Fetch(vec2 pos, vec2 off) {
    return u_boost * Sample((floor(pos * u_lines + off) + vec2(0.5)) / u_lines);
}

vec2 Dist(vec2 pos) {
    pos = pos * u_lines;
    return -((pos - floor(pos)) - vec2(0.5));
}

float Gaus(float pos, float scale) { return exp2(scale * pow(abs(pos), shape)); }

vec3 Horz3(vec2 pos, float off) {
    vec3 b = Fetch(pos, vec2(-1.0, off));
    vec3 c = Fetch(pos, vec2(0.0, off));
    vec3 d = Fetch(pos, vec2(1.0, off));
    float dst = Dist(pos).x;
    float wb = Gaus(dst - 1.0, u_hard_pix);
    float wc = Gaus(dst + 0.0, u_hard_pix);
    float wd = Gaus(dst + 1.0, u_hard_pix);
    return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

vec3 Horz5(vec2 pos, float off) {
    vec3 a = Fetch(pos, vec2(-2.0, off));
    vec3 b = Fetch(pos, vec2(-1.0, off));
    vec3 c = Fetch(pos, vec2(0.0, off));
    vec3 d = Fetch(pos, vec2(1.0, off));
    vec3 e = Fetch(pos, vec2(2.0, off));
    float dst = Dist(pos).x;
    float wa = Gaus(dst - 2.0, u_hard_pix);
    float wb = Gaus(dst - 1.0, u_hard_pix);
    float wc = Gaus(dst + 0.0, u_hard_pix);
    float wd = Gaus(dst + 1.0, u_hard_pix);
    float we = Gaus(dst + 2.0, u_hard_pix);
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

float Scan(vec2 pos, float off) { return Gaus(Dist(pos).y + off, u_hard_scan); }
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
    vec3 mask = vec3(u_mask_dark);
    if (u_mask_kind < 0.5) return vec3(1.0);
    if (u_mask_kind > 1.5) pos.x += pos.y * 3.0;
    pos.x = fract(pos.x / (u_mask_kind > 1.5 ? 6.0 : 3.0));
    if (pos.x < 0.333) mask.r = u_mask_light;
    else if (pos.x < 0.666) mask.g = u_mask_light;
    else mask.b = u_mask_light;
    return mask;
}

void main() {
    vec2 pos = v_uv + vec2(0.0, u_line_shift / u_lines.y);
    vec3 crt = Tri(pos);
    if (u_bloom > 0.0) crt += Bloom(pos) * u_bloom;
    crt *= Mask(floor(gl_FragCoord.xy / u_mask_size));
    gl_FragColor = vec4(ToSrgb(mix(Sample(v_uv), crt, u_intensity)), 1.0);
}
)";

// The whole viewport as a strip of two triangles: each corner's place, and
// the picture's point there, its top row first.
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
    GLint frame = -1, lines = -1, swap = -1, flip = -1;
    GLint hard_scan = -1, hard_pix = -1, mask_kind = -1, mask_size = -1;
    GLint mask_dark = -1, mask_light = -1, bloom = -1, boost = -1, line_shift = -1, intensity = -1;
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
    g.lines = glGetUniformLocation(program, "u_lines");
    g.swap = glGetUniformLocation(program, "u_swap");
    g.flip = glGetUniformLocation(program, "u_flip");
    g.hard_scan = glGetUniformLocation(program, "u_hard_scan");
    g.hard_pix = glGetUniformLocation(program, "u_hard_pix");
    g.mask_kind = glGetUniformLocation(program, "u_mask_kind");
    g.mask_size = glGetUniformLocation(program, "u_mask_size");
    g.mask_dark = glGetUniformLocation(program, "u_mask_dark");
    g.mask_light = glGetUniformLocation(program, "u_mask_light");
    g.bloom = glGetUniformLocation(program, "u_bloom");
    g.boost = glGetUniformLocation(program, "u_boost");
    g.line_shift = glGetUniformLocation(program, "u_line_shift");
    g.intensity = glGetUniformLocation(program, "u_intensity");
    GLint bound = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound);
    glGenBuffers(1, &g.quad);
    glBindBuffer(GL_ARRAY_BUFFER, g.quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(bound));
    return true;
}

// Draws `source`, `w` by `h` pixels, into the viewport at (x, y) of the
// framebuffer SDL has bound, `view_w` by `view_h`, through the mode's
// monitor. SDL keeps track of the state it last set rather than set it for
// every draw, so what is changed here is put back after.
bool Draw(SDL_Renderer* renderer, SDL_Texture* source, int w, int h, int x, int y, int view_w, int view_h,
          bool into_target) {
    const Monitor& monitor = kMonitors[g_setting.mode];
    Uint32 format = 0;
    if (g_setting.mode == kOff || SDL_QueryTexture(source, &format, nullptr, nullptr, nullptr) != 0) return false;
    SDL_RenderFlush(renderer);
    if (!Ready()) return false;

    GLint program = 0, buffer = 0;
    GLint viewport[4] = {};
    GLint uv_enabled = GL_FALSE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetVertexAttribiv(kUvAttribute, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &uv_enabled);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);

    if (SDL_GL_BindTexture(source, nullptr, nullptr) != 0) return false;
    glUseProgram(g.program);
    glBindBuffer(GL_ARRAY_BUFFER, g.quad);
    glEnableVertexAttribArray(kPositionAttribute);
    glEnableVertexAttribArray(kUvAttribute);
    glVertexAttribPointer(kPositionAttribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(kUvAttribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glViewport(x, y, view_w, view_h);
    const float lines_down = static_cast<float>(view_h) / monitor.pixels_per_line;
    glUniform1i(g.frame, 0);
    glUniform2f(g.lines, lines_down * static_cast<float>(w) / static_cast<float>(h), lines_down);
    glUniform1f(g.swap, format == SDL_PIXELFORMAT_ARGB8888 || format == SDL_PIXELFORMAT_RGB888 ? 1.0f : 0.0f);
    glUniform1f(g.flip, into_target ? 1.0f : 0.0f);
    glUniform1f(g.hard_scan, monitor.hard_scan);
    glUniform1f(g.hard_pix, monitor.hard_pix);
    glUniform1f(g.mask_kind, monitor.mask_kind);
    glUniform1f(g.mask_size, monitor.mask_size);
    glUniform1f(g.mask_dark, monitor.mask_dark);
    glUniform1f(g.mask_light, monitor.mask_light);
    glUniform1f(g.bloom, monitor.bloom);
    glUniform1f(g.boost, monitor.boost);
    glUniform1f(g.line_shift, LineShift(monitor.pixels_per_line));
    glUniform1f(g.intensity, static_cast<float>(g_setting.intensity) / 100.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_UnbindTexture(source);

    glUseProgram(static_cast<GLuint>(program));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
    if (uv_enabled != GL_TRUE) glDisableVertexAttribArray(kUvAttribute);
    if (blend == GL_TRUE) glEnable(GL_BLEND);
    if (scissor == GL_TRUE) glEnable(GL_SCISSOR_TEST);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return true;
}

} // namespace

bool Present(SDL_Renderer* renderer, SDL_Texture* frame, int frame_w, int frame_h, const SDL_Rect& dest,
             int drawable_h) {
    return Draw(renderer, frame, frame_w, frame_h, dest.x, drawable_h - dest.y - dest.h, dest.w, dest.h, false);
}

bool Apply(SDL_Renderer* renderer, SDL_Texture* source, SDL_Texture* target, int w, int h) {
    if (g_setting.mode == kOff) return false;
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, target);
    const bool drawn = Draw(renderer, source, w, h, 0, 0, w, h, true);
    SDL_SetRenderTarget(renderer, previous);
    return drawn;
}

#else

bool Present(SDL_Renderer*, SDL_Texture*, int, int, const SDL_Rect&, int) { return false; }
bool Apply(SDL_Renderer*, SDL_Texture*, SDL_Texture*, int, int) { return false; }

#endif

} // namespace HAL::Crt
