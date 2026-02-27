#ifndef LAYOUTCONFIG_H
#define LAYOUTCONFIG_H

#include <QtGlobal>

struct LayoutConfig {
    qreal nodeMarginH = 1.5;   // horizontal gap between layers (TikZ units)
    qreal nodeMarginV = 1.0;   // vertical gap within layers
    bool leftToRight = true;    // flow direction
    qreal traceMargin = 0.3;   // min spacing between parallel edges
    int maxIterations = 50;     // crossing minimization sweeps
};

#endif // LAYOUTCONFIG_H
