#include "AnimatedBackgroundWebGL.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QTimer>
#include <QPalette>
#include <QVector3D>
#include <QtMath>

namespace {
constexpr auto kVertexShader = R"(
attribute vec2 a_position;
varying vec2 v_uv;
void main() {
    v_uv = a_position * 0.5 + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

// 3D volumetric silk-ribbon background shader.
// Each layer uses smooth-noise FBM for organic wave shape, analytical fake
// normals from finite-difference slope, and Blinn-Phong + silk-shimmer
// specular for a lustrous, fabric-like appearance.
constexpr auto kFragmentShader = R"(
#ifdef GL_ES
precision highp float;
#endif

varying vec2 v_uv;
uniform float u_time;
uniform vec3 u_accent;
uniform vec3 u_window;
uniform vec3 u_surface;
uniform vec2 u_parallax;
uniform float u_connected;   // 0=disconnected .. 1=connected
uniform float u_traffic;     // 0=idle .. 1=peak

// ── Noise primitives ────────────────────────────────────────────────────────

float hash21(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p.yx + 19.19);
    return fract(p.x * p.y);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i),             hash21(i + vec2(1.0, 0.0)), f.x),
        mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x),
        f.y);
}

// 4-octave FBM — organic wave shape
float fbm4(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * vnoise(p);
        p = p * 2.03 + vec2(3.71, 1.93);
        a *= 0.5;
    }
    return v;
}

// Wave height + X-slope via finite difference (one extra FBM sample)
vec2 waveHS(float x, float t, float layerSeed) {
    const float eps = 0.012;
    vec2 base = vec2(x,       t + layerSeed * 4.37);
    vec2 offt = vec2(x + eps, t + layerSeed * 4.37);
    float h  = fbm4(base);
    float hr = fbm4(offt);
    return vec2(h, (hr - h) / eps);
}

// ── Single silk-ribbon layer ─────────────────────────────────────────────────
//   depth: 0=foreground (bright, thick, fast)   1=background (dim, thin, slow)
vec3 silkLayer(vec2 uv, float yBase, float speed, float depth) {
    float t  = u_time * speed;
    float px = u_parallax.x * 0.048 * (1.0 - depth * 0.55);
    float py = u_parallax.y * 0.032 * (1.0 - depth * 0.55);

    // Amplitude: calmer when disconnected, breathes with traffic
    float ampMod   = mix(0.55, 1.0, u_connected) * (1.0 + u_traffic * 0.20);
    float thickness = mix(0.040, 0.082, 1.0 - depth);

    // Wave position & slope
    vec2 hs    = waveHS(uv.x * 2.7 + px, t, depth + 0.1);
    float waveY = yBase + py + (hs.x - 0.5) * 0.30 * ampMod;
    float slope = hs.y * 2.7 * 0.30 * ampMod;   // dY/dX (analytical from FD)

    float dy   = uv.y - waveY;
    float dist = abs(dy);

    // Early-out: pixel is far from this ribbon
    if (dist > thickness * 2.6) return vec3(0.0);

    // ── Fake 3-D normal from wave slope ──────────────────────────────────────
    // Tangent along X: (1, slope, 0).  Normal = perpendicular in XY + Z bias.
    vec3 normal = normalize(vec3(-slope * 2.3, 1.0, 0.80));

    // ── Lighting ──────────────────────────────────────────────────────────────
    vec3 lightDir = normalize(vec3(-0.22, 0.88, 0.42));
    float diff    = clamp(dot(normal, lightDir), 0.0, 1.0);

    // Blinn-Phong specular
    vec3  halfV = normalize(lightDir + vec3(0.0, 0.0, 1.0));
    float spec  = pow(clamp(dot(normal, halfV), 0.0, 1.0), 24.0);

    // Silk shimmer: modulate specular with a fine high-freq ripple so the
    // highlight breaks into narrow shimmering bands (characteristic of silk).
    float ripple = vnoise(vec2(uv.x * 20.0 + t * 3.8, waveY * 10.0));
    spec *= max(0.0, 0.5 + ripple * 1.0);   // [0 .. 1.5] range, mostly 0–1

    // ── Cross-section profile ─────────────────────────────────────────────────
    float normDist = dist / thickness;
    float profile  = pow(max(0.0, 1.0 - normDist), 1.65);

    // Top face (negative dy = pixel is above ribbon center line)
    float topFace = clamp((-dy / thickness) * 0.65 + 0.55, 0.0, 1.0);

    // ── Color construction ────────────────────────────────────────────────────
    float bright      = 1.0 - depth * 0.72;
    vec3  accentBright = mix(u_accent, vec3(1.0), 0.36 + bright * 0.12);
    vec3  accentDark   = u_accent * bright * 0.26;

    // Surface: lighter top-lit face, darker bottom-shadowed face
    vec3 baseColor = mix(accentDark, accentBright, topFace * 0.65 + diff * 0.35);

    // Specular sheen (silk anisotropic highlight)
    baseColor += mix(accentBright, vec3(1.0), 0.42) * spec * 0.48 * bright;

    // Crest glint: bright thin stripe along the very top edge
    float edgeGlint = smoothstep(0.55, 0.0, normDist) * topFace;
    baseColor += accentBright * edgeGlint * 0.38;

    // ── Glow halo beyond the physical ribbon ─────────────────────────────────
    float halo     = exp(-dist * dist / (thickness * thickness * 1.3));
    vec3  glowColor = u_accent * halo * bright * 0.32;

    // ── Dim when VPN is disconnected ─────────────────────────────────────────
    float dimFactor = mix(0.22, 1.0, u_connected);

    return (baseColor * profile + glowColor) * dimFactor;
}

// ── Main ─────────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = v_uv;

    // Deep background gradient (darker when disconnected)
    float connFac = mix(0.72, 1.0, u_connected);
    vec3  bgTop   = u_window * 0.22 * connFac;
    vec3  bgBot   = u_window * 0.07 * connFac;
    vec3  color   = mix(bgTop, bgBot, uv.y + u_parallax.y * 0.015);

    // Faint ambient accent tint drifting from the mid-screen upward
    color += u_accent * 0.022 * smoothstep(0.25, 0.65, uv.y) * connFac;

    // ── Silk ribbon layers (far → near, additive blend) ──────────────────────
    //           yBase   speed   depth
    color += silkLayer(uv, 0.21,  0.045,  0.93);   // deepest, slowest, dimmest
    color += silkLayer(uv, 0.37,  0.072,  0.73);
    color += silkLayer(uv, 0.51,  0.112,  0.48);
    color += silkLayer(uv, 0.64,  0.162,  0.22);
    color += silkLayer(uv, 0.75,  0.213,  0.05);   // closest, fastest, brightest

    // ── Soft glowing dust particles ───────────────────────────────────────────
    vec3  dustColor = mix(u_accent, vec3(1.0), 0.58);
    float dimFactor = mix(0.22, 1.0, u_connected);

    for (int i = 0; i < 22; ++i) {
        float fi   = float(i);
        vec2  seed = vec2(fi * 2.71828, fi * 4.97311);
        float depth = hash21(seed + 8.31);
        vec2  sp    = vec2(hash21(seed), hash21(seed + 5.51));
        vec2  pos   = vec2(
            fract(sp.x + u_time * (0.005 + depth * 0.013) + u_parallax.x * depth * 0.010),
            fract(sp.y - u_time * (0.002 + depth * 0.005) + u_parallax.y * depth * 0.007)
        );
        float d          = length(uv - pos);
        float sz         = mix(0.003, 0.011, depth);
        float brightness = exp(-d * d / (sz * sz)) * mix(0.08, 0.44, depth);
        color += dustColor * brightness * dimFactor;
    }

    // ── Vignette ──────────────────────────────────────────────────────────────
    color *= mix(0.42, 1.0, smoothstep(0.90, 0.28, distance(uv, vec2(0.5))));

    // ── Filmic tone mapping + gentle gamma ────────────────────────────────────
    color  = color / (color + vec3(0.88));
    color  = pow(max(color, vec3(0.0)), vec3(0.90));

    gl_FragColor = vec4(color, 1.0);
}
)";
} // namespace

AnimatedBackgroundWebGL::AnimatedBackgroundWebGL(QWidget *parent)
    : QOpenGLWidget(parent) {
    setAutoFillBackground(false);
    setMouseTracking(true);

    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, [this] {
        if (!reduce_motion_) {
            time_ += 0.016f;
            if (time_ > 10000.0f) time_ = 0.0f;
        }

        const qreal lerp = reduce_motion_ ? 0.25 : 0.08;
        parallax_current_.setX(parallax_current_.x() + (parallax_target_.x() - parallax_current_.x()) * lerp);
        parallax_current_.setY(parallax_current_.y() + (parallax_target_.y() - parallax_current_.y()) * lerp);
        update();
    });
    timer_->start();
}

AnimatedBackgroundWebGL::~AnimatedBackgroundWebGL() {
    makeCurrent();
    cleanupGl();
    doneCurrent();
}

void AnimatedBackgroundWebGL::setReduceMotion(bool reduce) {
    reduce_motion_ = reduce;
    if (reduce_motion_) {
        timer_->stop();
    } else if (!timer_->isActive()) {
        timer_->start();
    }
    update();
}

bool AnimatedBackgroundWebGL::reduceMotion() const {
    return reduce_motion_;
}

void AnimatedBackgroundWebGL::setConnected(float connected) {
    connected_ = qBound(0.0f, connected, 1.0f);
    if (reduce_motion_) update();
}

void AnimatedBackgroundWebGL::setTraffic(float traffic) {
    traffic_ = qBound(0.0f, traffic, 1.0f);
    if (reduce_motion_) update();
}

void AnimatedBackgroundWebGL::setParallaxOffset(QPointF offset) {
    parallax_target_.setX(qBound(-1.0, offset.x(), 1.0));
    parallax_target_.setY(qBound(-1.0, offset.y(), 1.0));
    if (reduce_motion_) {
        parallax_current_ = parallax_target_;
        update();
    }
}

void AnimatedBackgroundWebGL::initializeGL() {
    initializeOpenGLFunctions();
    ensureGlResources();
}

void AnimatedBackgroundWebGL::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void AnimatedBackgroundWebGL::paintGL() {
    ensureGlResources();
    if (program_ == nullptr || quad_vbo_ == nullptr) {
        glClearColor(0.04f, 0.03f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    const auto window = palette().color(QPalette::Window);
    const auto surface = palette().color(QPalette::Base);
    const auto accent = palette().color(QPalette::Highlight);
    const auto toVec3 = [](const QColor &c) { return QVector3D(c.redF(), c.greenF(), c.blueF()); };

    program_->bind();
    program_->setUniformValue("u_time", reduce_motion_ ? 0.0f : time_);
    program_->setUniformValue("u_accent", toVec3(accent));
    program_->setUniformValue("u_window", toVec3(window));
    program_->setUniformValue("u_surface", toVec3(surface));
    program_->setUniformValue("u_parallax", QVector2D(parallax_current_.x(), parallax_current_.y()));
    program_->setUniformValue("u_connected", connected_);
    program_->setUniformValue("u_traffic", traffic_);

    quad_vbo_->bind();
    program_->enableAttributeArray("a_position");
    program_->setAttributeBuffer("a_position", GL_FLOAT, 0, 2, sizeof(float) * 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program_->disableAttributeArray("a_position");
    quad_vbo_->release();
    program_->release();
}

void AnimatedBackgroundWebGL::cleanupGl() {
    if (quad_vbo_ != nullptr) {
        if (quad_vbo_->isCreated()) {
            quad_vbo_->destroy();
        }
        delete quad_vbo_;
        quad_vbo_ = nullptr;
    }
    delete program_;
    program_ = nullptr;
}

void AnimatedBackgroundWebGL::ensureGlResources() {
    if (program_ != nullptr && quad_vbo_ != nullptr) return;

    if (program_ == nullptr) {
        program_ = new QOpenGLShaderProgram(this);
        if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) ||
            !program_->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader) ||
            !program_->link()) {
            delete program_;
            program_ = nullptr;
            return;
        }
    }

    if (quad_vbo_ == nullptr) {
        quad_vbo_ = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        if (!quad_vbo_->create()) {
            delete quad_vbo_;
            quad_vbo_ = nullptr;
            return;
        }

        static const float kQuad[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f
        };
        quad_vbo_->bind();
        quad_vbo_->allocate(kQuad, static_cast<int>(sizeof(kQuad)));
        quad_vbo_->release();
    }
}
