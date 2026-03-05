#pragma once

#if __has_include(<QtOpenGLWidgets/QOpenGLWidget>)
#include <QtOpenGLWidgets/QOpenGLWidget>
#else
#include <QOpenGLWidget>
#endif
#include <QOpenGLFunctions>
#include <QPointF>

class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QTimer;

class AnimatedBackgroundWebGL : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit AnimatedBackgroundWebGL(QWidget *parent = nullptr);
    ~AnimatedBackgroundWebGL() override;

    void setReduceMotion(bool reduce);
    [[nodiscard]] bool reduceMotion() const;
    void setParallaxOffset(QPointF offset);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void cleanupGl();
    void ensureGlResources();

    QOpenGLShaderProgram *program_ = nullptr;
    QOpenGLBuffer *quad_vbo_ = nullptr;
    QTimer *timer_ = nullptr;

    bool reduce_motion_ = false;
    float time_ = 0.0f;
    QPointF parallax_target_;
    QPointF parallax_current_;
};
