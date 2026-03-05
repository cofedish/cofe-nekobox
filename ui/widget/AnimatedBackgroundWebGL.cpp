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

// Silk ribbon shader: sharp isobands with pseudo-normals and minimal bloom.
constexpr auto kFragmentShader = R"(
#ifdef GL_ES
precision highp float;
#extension GL_OES_standard_derivatives : enable
#endif

varying vec2 v_uv;
uniform float u_time;
uniform vec3 u_accent;
uniform vec3 u_window;
uniform vec3 u_surface;
uniform vec2 u_parallax;
uniform float u_connected;
uniform float u_traffic;

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.35));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
        mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x),
        f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.55;
    for (int i = 0; i < 4; ++i) {
        v += noise2(p) * a;
        p = p * 2.04 + vec2(1.7, 4.2);
        a *= 0.5;
    }
    return v;
}

float waveField(vec2 p, float t, float seed) {
    float primary = sin(p.x * 1.7 + t * 0.62 + seed * 1.4) * 0.16;
    float secondary = sin(p.x * 3.4 - t * 0.31 + seed * 2.1) * 0.08;
    float diagonal = sin((p.x + p.y * 0.85) * 4.9 + t * 0.43 + seed * 0.9) * 0.05;
    float tertiary = sin((p.x - p.y * 0.45) * 6.7 - t * 0.24 + seed * 2.9) * 0.03;
    float organic = (fbm(p * 1.9 + vec2(seed * 2.3, -t * 0.14)) - 0.5) * 0.11;
    return p.y + primary + secondary + diagonal + tertiary + organic;
}

float ribbonMask(float field, float offset, float width) {
    return 1.0 - smoothstep(width * 0.72, width, abs(field - offset));
}

vec3 shadeRibbon(float field, float offset, float width, vec3 accentDark, vec3 accentLight, vec3 accentEdge, float diffuse, float depthFade) {
    float dist = abs(field - offset) / width;
    float ribbon = ribbonMask(field, offset, width);
    float core = 1.0 - smoothstep(0.0, 0.42, dist);
    float edge = smoothstep(1.0, 0.58, dist) * smoothstep(0.14, 0.46, dist);
    float translucency = smoothstep(1.0, 0.18, dist);

    vec3 ribbonColor = mix(accentDark, accentLight, clamp(diffuse * 0.82 + 0.12, 0.0, 1.0));
    ribbonColor = mix(ribbonColor, accentDark * 0.86, core * 0.48);
    ribbonColor += accentEdge * edge * 0.76;
    ribbonColor += accentEdge * pow(clamp(1.0 - dist, 0.0, 1.0), 4.0) * 0.12;

    vec3 glow = accentEdge * ribbon * (1.0 - core) * 0.12;
    return (ribbonColor * translucency + glow) * depthFade;
}

vec3 ribbonLayer(vec2 uv, float yBase, float scale, float speed, float width, float depth) {
    float t = u_time * speed;
    vec2 parallax = u_parallax * mix(0.06, 0.16, 1.0 - depth);
    float amplitude = mix(0.82, 1.08, u_connected) * (1.0 + u_traffic * 0.12);

    vec2 p = vec2(
        (uv.x - 0.5) * scale + parallax.x * 0.12,
        (uv.y - yBase) * mix(2.6, 3.4, 1.0 - depth) + parallax.y * 0.08
    );
    p.y /= amplitude;

    float field = waveField(p, t, depth * 4.0 + 1.0);
    vec2 grad = vec2(dFdx(field), dFdy(field));
    vec3 normal = normalize(vec3(-grad.x * 3.1, -grad.y * 3.1, 1.0));
    vec3 lightDir = normalize(vec3(0.2, 0.4, 1.0));
    float diffuse = clamp(dot(normal, lightDir), 0.0, 1.0);

    float shimmer = noise2(vec2(uv.x * 18.0 + t * 1.4, uv.y * 12.0 - t * 0.6 + depth * 3.1));
    vec3 accentDark = mix(u_window * 0.26, u_accent * 0.48, 0.68) * mix(0.70, 1.0, 1.0 - depth);
    vec3 accentLight = mix(u_accent, vec3(1.0), 0.24 + diffuse * 0.12);
    vec3 accentEdge = mix(accentLight, vec3(1.0), 0.18 + shimmer * 0.16);
    float depthFade = mix(1.06, 0.46, depth) * mix(0.42, 1.0, u_connected);

    vec3 color = vec3(0.0);
    color += shadeRibbon(field, -0.26, width, accentDark, accentLight, accentEdge, diffuse, depthFade);
    color += shadeRibbon(field, 0.00, width, accentDark, accentLight, accentEdge, diffuse, depthFade);
    color += shadeRibbon(field, 0.24, width, accentDark, accentLight, accentEdge, diffuse, depthFade);
    return color;
}

vec3 dustLayer(vec2 uv) {
    vec3 dustColor = mix(u_accent, vec3(1.0), 0.54);
    vec3 color = vec3(0.0);
    float dim = mix(0.26, 1.0, u_connected);
    for (int i = 0; i < 20; ++i) {
        float fi = float(i);
        vec2 seed = vec2(fi * 1.723, fi * 3.117);
        float depth = hash21(seed + 7.2);
        vec2 pos = vec2(
            fract(hash21(seed + 2.0) + u_time * (0.004 + depth * 0.010) + u_parallax.x * depth * 0.015),
            fract(hash21(seed + 5.0) - u_time * (0.0015 + depth * 0.004) + u_parallax.y * depth * 0.010)
        );
        float size = mix(0.0025, 0.0085, depth);
        float d = length(uv - pos);
        float alpha = exp(-d * d / (size * size)) * mix(0.04, 0.22, depth);
        color += dustColor * alpha * dim;
    }
    return color;
}

void main() {
    vec2 uv = v_uv;
    float conn = mix(0.72, 1.0, u_connected);
    vec3 bgTop = mix(u_window * 0.30, u_surface * 0.72, 0.35) * conn;
    vec3 bgMid = mix(u_window * 0.12, u_surface * 0.44, 0.55) * conn;
    vec3 bgBottom = u_window * 0.06 * conn;
    vec3 color = mix(bgTop, bgMid, smoothstep(0.08, 0.52, uv.y));
    color = mix(color, bgBottom, smoothstep(0.48, 1.0, uv.y));
    color += u_accent * 0.016 * smoothstep(0.18, 0.78, uv.y);

    float starNoise = noise2(uv * vec2(180.0, 120.0) + vec2(u_time * 0.04, 0.0));
    color += vec3(1.0) * smoothstep(0.985, 1.0, starNoise) * 0.07;

    color += ribbonLayer(uv, 0.72, 2.4, 0.08, 0.072, 0.88);
    color += ribbonLayer(uv, 0.56, 2.9, 0.12, 0.064, 0.58);
    color += ribbonLayer(uv, 0.42, 3.5, 0.18, 0.056, 0.22);
    color += dustLayer(uv);

    float vignette = smoothstep(1.05, 0.24, distance(uv, vec2(0.5)));
    color *= mix(0.36, 1.0, vignette);
    color = color / (color + vec3(0.92));
    color = pow(max(color, vec3(0.0)), vec3(0.94));

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
