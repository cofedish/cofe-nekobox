#pragma once

#include <QPushButton>

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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateAnimationState();
    QString stateText() const;

    State current_state = State::Disconnected;
    bool reduce_motion = false;
    qreal phase = 0.0;
    QTimer *timer = nullptr;
};
