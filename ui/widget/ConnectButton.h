#pragma once

#include <QPushButton>
#include <QFont>

class QTimer;

class ConnectButton : public QPushButton {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting
    };

    explicit ConnectButton(QWidget *parent = nullptr);

    void setState(State state);
    [[nodiscard]] State state() const;

    void setReduceMotion(bool reduce);
    [[nodiscard]] bool reduceMotion() const;
    void refreshMetrics();

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void updateAnimationState();
    QString stateText() const;
    int minimumDiameter() const;
    int maximumDiameter() const;
    int targetDiameter() const;
    QFont resolveTextFont(const QString &text, qreal diameter) const;

    State current_state = State::Disconnected;
    bool reduce_motion = false;
    qreal phase = 0.0;
    QTimer *timer = nullptr;
};
