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

constexpr auto kFragmentShader = R"(
#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_uv;
uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_accent;
uniform vec3 u_window;
uniform vec3 u_surface;
uniform vec2 u_parallax;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float ribbon(vec2 uv, float yBase, float amp, float freq, float speed, float thick) {
    float y = yBase + sin((uv.x + u_parallax.x * 0.05) * freq + u_time * speed + u_parallax.y * 0.7) * amp;
    float d = abs(uv.y - y);
    return smoothstep(thick, 0.0, d);
}

void main() {
    vec2 uv = v_uv;
    vec2 par = u_parallax;

    vec3 bgTop = mix(u_window * 0.28, u_surface * 0.40, 0.55);
    vec3 bgBottom = mix(u_window * 0.16, u_surface * 0.22, 0.75);
    vec3 color = mix(bgTop, bgBottom, uv.y + par.y * 0.02);

    vec3 accentA = mix(u_accent, vec3(1.0), 0.28);
    vec3 accentB = mix(u_accent, u_surface, 0.42);
    vec3 accentC = mix(u_accent, vec3(0.0), 0.35);

    float farR = ribbon(uv, 0.39 + par.y * 0.015, 0.040, 8.2, 0.13, 0.038);
    float midR = ribbon(uv, 0.46 + par.y * 0.020, 0.055, 9.6, -0.10, 0.052);
    float nearR = ribbon(uv, 0.54 + par.y * 0.028, 0.043, 11.1, 0.07, 0.045);

    color += accentA * farR * 0.40;
    color += accentB * midR * 0.58;
    color += accentC * nearR * 0.44;

    float specFar = ribbon(uv + vec2(0.0, -0.006), 0.39 + par.y * 0.015, 0.040, 8.2, 0.13, 0.006);
    float specMid = ribbon(uv + vec2(0.0, -0.007), 0.46 + par.y * 0.020, 0.055, 9.6, -0.10, 0.007);
    float specNear = ribbon(uv + vec2(0.0, -0.007), 0.54 + par.y * 0.028, 0.043, 11.1, 0.07, 0.007);
    color += vec3(1.0) * (specFar * 0.08 + specMid * 0.12 + specNear * 0.10);

    // Soft star dust, cheap deterministic noise.
    float stars = 0.0;
    for (int i = 0; i < 36; ++i) {
        float fi = float(i);
        vec2 seed = vec2(fi * 1.71, fi * 3.97);
        vec2 p = vec2(hash21(seed), hash21(seed + 11.3));
        float depth = mix(0.20, 0.95, hash21(seed + 23.1));
        vec2 drift = vec2(fract(u_time * (0.004 + depth * 0.010) + p.x), fract(p.y + u_time * (0.001 + depth * 0.004)));
        vec2 starPos = mix(p, drift, 0.55) + par * depth * 0.010;
        float d = length(uv - starPos);
        stars += smoothstep(0.010, 0.0, d) * (0.10 + depth * 0.18);
    }
    color += mix(u_accent, vec3(1.0), 0.55) * stars * 0.16;

    float vignette = smoothstep(0.92, 0.28, distance(uv, vec2(0.5)));
    color *= mix(0.58, 1.0, vignette);

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
    program_->setUniformValue("u_resolution", QVector2D(width(), height()));
    program_->setUniformValue("u_time", reduce_motion_ ? 0.0f : time_);
    program_->setUniformValue("u_accent", toVec3(accent));
    program_->setUniformValue("u_window", toVec3(window));
    program_->setUniformValue("u_surface", toVec3(surface));
    program_->setUniformValue("u_parallax", QVector2D(parallax_current_.x(), parallax_current_.y()));

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
