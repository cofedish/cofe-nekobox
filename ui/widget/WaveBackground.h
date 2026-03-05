#pragma once

#include "AnimatedBackgroundWebGL.h"

class WaveBackground : public AnimatedBackgroundWebGL {
    Q_OBJECT

public:
    explicit WaveBackground(QWidget *parent = nullptr);
};
